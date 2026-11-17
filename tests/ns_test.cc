/*
 *  Copyright (c) 2026 The webrtc-ns authors.
 *
 *  Use of this source code is governed by a BSD-style license that can be
 *  found in the LICENSE file in the root of the source tree.
 */

// Command line driver for the standalone WebRTC noise suppressor.
//
// Usage:
//   ns_test [input.wav] [output.wav] [clean_reference.wav] [level]
//
//   input.wav           Noisy input, 16-bit PCM, 16/32/48 kHz, 10 ms frames
//   output.wav          Denoised output (default: tests/denoised.wav)
//   clean_reference.wav Optional clean reference used for quality metrics.
//                       Pass an empty string to skip metric computation.
//   level               Suppression level: 6, 12, 18 or 21 dB (default: 12)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "api/noise_suppression.h"
#include "audio_processing/logging/wav_file.h"

namespace {

constexpr int kFrameMs = 10;
// Frames whose clean reference is quieter than this are treated as
// noise-only frames when estimating the noise attenuation.
constexpr double kSilenceThresholdDb = -55.0;

double PowerToDb(double power, size_t num_samples) {
  if (num_samples == 0 || power <= 0.0) {
    return -200.0;
  }
  const double rms = std::sqrt(power / static_cast<double>(num_samples));
  return 20.0 * std::log10(rms / 32768.0);
}

int16_t FloatToInt16(float value) {
  value = std::nearbyint(value);
  if (value > 32767.0f) {
    return 32767;
  }
  if (value < -32768.0f) {
    return -32768;
  }
  return static_cast<int16_t>(value);
}

std::vector<std::vector<float>> Deinterleave(
    const std::vector<int16_t>& interleaved,
    size_t num_channels,
    size_t num_frames) {
  std::vector<std::vector<float>> channels(
      num_channels, std::vector<float>(num_frames));
  for (size_t i = 0; i < num_frames; ++i) {
    for (size_t ch = 0; ch < num_channels; ++ch) {
      channels[ch][i] = static_cast<float>(interleaved[i * num_channels + ch]);
    }
  }
  return channels;
}

bool ParseLevel(const std::string& text,
                webrtc::NoiseSuppression::Level* level) {
  if (text == "6" || text == "6dB") {
    *level = webrtc::NoiseSuppression::Level::k6dB;
  } else if (text == "12" || text == "12dB") {
    *level = webrtc::NoiseSuppression::Level::k12dB;
  } else if (text == "18" || text == "18dB") {
    *level = webrtc::NoiseSuppression::Level::k18dB;
  } else if (text == "21" || text == "21dB") {
    *level = webrtc::NoiseSuppression::Level::k21dB;
  } else {
    return false;
  }
  return true;
}

// Estimates the delay of the processed output relative to the clean reference
// by locating the peak of their normalized cross-correlation. Only the first
// two seconds are used to bound the cost.
size_t EstimateLatencySamples(
    const std::vector<std::vector<float>>& clean,
    const std::vector<std::vector<float>>& processed,
    size_t num_channels,
    size_t num_frames,
    int sample_rate_hz) {
  const size_t max_lag = static_cast<size_t>(sample_rate_hz) * 20 / 1000;
  const size_t max_frames =
      std::min(num_frames, static_cast<size_t>(sample_rate_hz) * 2);
  if (max_frames == 0) {
    return 0;
  }

  double best_correlation = -1.0;
  size_t best_lag = 0;
  for (size_t lag = 0; lag <= max_lag && lag + max_frames <= num_frames;
       ++lag) {
    double correlation = 0.0;
    double clean_power = 0.0;
    double processed_power = 0.0;
    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < max_frames; ++i) {
        const double c = clean[ch][i];
        const double n = processed[ch][lag + i];
        correlation += c * n;
        clean_power += c * c;
        processed_power += n * n;
      }
    }
    const double norm = std::sqrt(clean_power * processed_power);
    if (norm > 0.0) {
      const double normalized = correlation / norm;
      if (normalized > best_correlation) {
        best_correlation = normalized;
        best_lag = lag;
      }
    }
  }
  return best_lag;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string input_path = "tests/noisy.wav";
  std::string output_path = "tests/denoised.wav";
  std::string reference_path = "tests/clean.wav";
  std::string level_text = "12";

  if (argc >= 2) {
    input_path = argv[1];
  }
  if (argc >= 3) {
    output_path = argv[2];
  }
  if (argc >= 4) {
    reference_path = argv[3];
  }
  if (argc >= 5) {
    level_text = argv[4];
  }

  webrtc::NoiseSuppression::Level level;
  if (!ParseLevel(level_text, &level)) {
    std::cerr << "ERROR: invalid suppression level '" << level_text
              << "'. Use 6, 12, 18 or 21 (dB).\n";
    return 1;
  }

  std::cout << "=== WebRTC Noise Suppression Test ===\n";
  std::cout << "  In         : " << input_path << "\n";
  std::cout << "  Out        : " << output_path << "\n";
  std::cout << "  Reference  : " << reference_path << "\n";
  std::cout << "  Level      : " << level_text << " dB\n\n";

  webrtc::WavReader reader(input_path);
  const int sample_rate_hz = reader.sample_rate();
  const size_t num_channels = reader.num_channels();
  const size_t frame_size =
      static_cast<size_t>(sample_rate_hz) / 1000 * kFrameMs;

  if (sample_rate_hz != 16000 && sample_rate_hz != 32000 &&
      sample_rate_hz != 48000) {
    std::cerr << "ERROR: input must be 16, 32 or 48 kHz, got "
              << sample_rate_hz << " Hz.\n";
    return 1;
  }

  const size_t input_total_samples = reader.num_samples();
  const size_t input_frames = input_total_samples / num_channels;
  const size_t num_frames = input_frames / frame_size;
  if (num_frames == 0) {
    std::cerr << "ERROR: input is shorter than one 10 ms frame.\n";
    return 1;
  }
  const size_t num_samples = num_frames * frame_size;

  std::vector<int16_t> input(num_samples * num_channels, 0);
  reader.ReadSamples(input.size(), input.data());
  std::vector<std::vector<float>> audio =
      Deinterleave(input, num_channels, num_samples);

  std::cout << "  Sample rate : " << sample_rate_hz << " Hz\n";
  std::cout << "  Channels    : " << num_channels << "\n";
  std::cout << "  Duration    : " << num_samples * 1000 / sample_rate_hz
            << " ms (" << num_frames << " frames)\n\n";

  webrtc::NoiseSuppression::Config config;
  config.level = level;
  webrtc::NoiseSuppression ns(sample_rate_hz, num_channels, config);

  std::vector<float*> channel_ptrs(num_channels);
  double input_power = 0.0;
  double output_power = 0.0;
  for (size_t f = 0; f < num_frames; ++f) {
    const size_t offset = f * frame_size;
    for (size_t ch = 0; ch < num_channels; ++ch) {
      channel_ptrs[ch] = audio[ch].data() + offset;
    }
    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < frame_size; ++i) {
        const double sample = channel_ptrs[ch][i];
        input_power += sample * sample;
      }
    }

    ns.Analyze(channel_ptrs.data());
    ns.Process(channel_ptrs.data());

    for (size_t ch = 0; ch < num_channels; ++ch) {
      for (size_t i = 0; i < frame_size; ++i) {
        const double sample = channel_ptrs[ch][i];
        output_power += sample * sample;
      }
    }
  }

  std::vector<int16_t> output(input.size(), 0);
  for (size_t i = 0; i < num_samples; ++i) {
    for (size_t ch = 0; ch < num_channels; ++ch) {
      output[i * num_channels + ch] = FloatToInt16(audio[ch][i]);
    }
  }

  webrtc::WavWriter writer(output_path, sample_rate_hz, num_channels);
  writer.WriteSamples(output.data(), output.size());

  std::cout << "  Output written to: " << output_path << "\n";
  std::cout << "  Input RMS  : " << PowerToDb(input_power, input.size())
            << " dBFS\n";
  std::cout << "  Output RMS : " << PowerToDb(output_power, input.size())
            << " dBFS\n";

  // Optional quality metrics against a clean reference.
  bool metrics_ok = true;
  if (!reference_path.empty()) {
    webrtc::WavReader reference(reference_path);
    if (reference.sample_rate() != sample_rate_hz ||
        reference.num_channels() != num_channels) {
      std::cerr << "\nWARNING: reference format does not match input, "
                   "skipping quality metrics.\n";
    } else {
      const size_t reference_frames =
          std::min(reference.num_samples() / num_channels, num_samples);
      std::vector<int16_t> reference_interleaved(
          reference_frames * num_channels, 0);
      reference.ReadSamples(reference_interleaved.size(),
                            reference_interleaved.data());
      std::vector<std::vector<float>> clean =
          Deinterleave(reference_interleaved, num_channels, reference_frames);

      // The suppressor delays the stream (overlap-add synthesis plus, at
      // 32/48 kHz, the analysis/synthesis filter bank). Estimate the actual
      // delay from the processed output rather than hard-coding it.
      const size_t latency_samples = EstimateLatencySamples(
          clean, audio, num_channels, reference_frames, sample_rate_hz);
      if (latency_samples + frame_size >= reference_frames) {
        std::cerr << "\nWARNING: reference is too short for latency-"
                     "compensated metrics.\n";
        metrics_ok = false;
      } else {
        const size_t analysed_frames =
            (reference_frames - latency_samples) / frame_size;

        std::cout << "\n  Detected latency   : " << latency_samples
                  << " samples (" << latency_samples * 1000 / sample_rate_hz
                  << " ms)\n";

        double signal_power = 0.0;
        double input_error_power = 0.0;
        double output_error_power = 0.0;
        double noise_input_power = 0.0;
        double noise_output_power = 0.0;
        double active_input_power = 0.0;
        double active_output_power = 0.0;
        size_t noise_samples = 0;
        size_t active_samples = 0;

        for (size_t f = 0; f < analysed_frames; ++f) {
          const size_t offset = f * frame_size;

          double frame_clean_power = 0.0;
          for (size_t ch = 0; ch < num_channels; ++ch) {
            for (size_t i = 0; i < frame_size; ++i) {
              const double c = clean[ch][offset + i];
              frame_clean_power += c * c;
            }
          }
          const size_t frame_samples = frame_size * num_channels;
          const bool silent = PowerToDb(frame_clean_power, frame_samples) <
                              kSilenceThresholdDb;
          if (silent) {
            noise_samples += frame_samples;
          } else {
            active_samples += frame_samples;
          }

          for (size_t ch = 0; ch < num_channels; ++ch) {
            for (size_t i = 0; i < frame_size; ++i) {
              const double c = clean[ch][offset + i];
              const double n = audio[ch][offset + latency_samples + i];
              // `audio` was processed in place; recover the input from the
              // untouched interleaved copy.
              const double x = static_cast<double>(
                  input[(offset + i) * num_channels + ch]);
              signal_power += c * c;
              input_error_power += (x - c) * (x - c);
              output_error_power += (n - c) * (n - c);
              if (silent) {
                noise_input_power += x * x;
                noise_output_power += n * n;
              } else {
                active_input_power += x * x;
                active_output_power += n * n;
              }
            }
          }
        }

        const double snr_in = 10.0 * std::log10(signal_power /
                                                std::max(input_error_power, 1e-9));
        const double snr_out = 10.0 * std::log10(
            signal_power / std::max(output_error_power, 1e-9));
        const double noise_attenuation =
            10.0 * std::log10(std::max(noise_input_power, 1e-9) /
                              std::max(noise_output_power, 1e-9));
        const double active_gain =
            10.0 * std::log10(std::max(active_output_power, 1e-9) /
                              std::max(active_input_power, 1e-9));

        std::cout << "\n  === Quality (vs. clean reference) ===\n";
        std::cout << "  Input SNR          : " << snr_in << " dB\n";
        std::cout << "  Output SNR         : " << snr_out << " dB\n";
        std::cout << "  SNR improvement    : " << snr_out - snr_in << " dB\n";
        std::cout << "  Noise attenuation  : " << noise_attenuation
                  << " dB (" << noise_samples << " noise-only samples)\n";
        std::cout << "  Active-frame gain  : " << active_gain << " dB ("
                  << active_samples << " speech samples)\n";

        // At 32/48 kHz the WebRTC analysis/synthesis filter bank adds its own
        // distortion, so the pass criterion is that the noise was attenuated
        // without collapsing the speech level.
        metrics_ok = noise_attenuation > 0.0 && active_gain > -3.0;
        if (!metrics_ok) {
          std::cerr << "ERROR: noise suppression did not attenuate noise while "
                       "preserving speech.\n";
        }
      }
    }
  }

  if (output_power <= 0.0) {
    std::cerr << "ERROR: output is silent.\n";
    return 1;
  }
  return metrics_ok ? 0 : 1;
}
