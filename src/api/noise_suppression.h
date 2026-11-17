/*
 *  Copyright (c) 2026 The webrtc-ns authors.
 *
 *  Use of this source code is governed by a BSD-style license that can be
 *  found in the LICENSE file in the root of the source tree.
 */

#ifndef API_NOISE_SUPPRESSION_H_
#define API_NOISE_SUPPRESSION_H_

#include <cstddef>
#include <memory>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Standalone WebRTC noise suppressor (modules/audio_processing/ns).
//
// Operates on 10 ms deinterleaved frames of float samples in the int16 range
// [-32768, 32767]. Supported sample rates are 16, 32 and 48 kHz. The number of
// frames per channel is sample_rate_hz() / 100.
//
// Typical real-time use:
//
//   webrtc::NoiseSuppression ns(48000, 2);
//   ns.Analyze(capture_channels);   // before AEC/AGC if those are used
//   // ... AEC/AGC may modify capture_channels here ...
//   ns.Process(capture_channels);   // in-place noise suppression
//
// Instances are not thread-safe.
class RTC_EXPORT NoiseSuppression {
 public:
  // Noise attenuation target. Mirrors NsConfig::SuppressionLevel.
  enum class Level { k6dB, k12dB, k18dB, k21dB };

  struct Config {
    Level level = Level::k12dB;
  };

  NoiseSuppression(size_t sample_rate_hz, size_t num_channels);
  NoiseSuppression(size_t sample_rate_hz,
                   size_t num_channels,
                   const Config& config);
  ~NoiseSuppression();

  NoiseSuppression(const NoiseSuppression&) = delete;
  NoiseSuppression& operator=(const NoiseSuppression&) = delete;

  // Analyses the current 10 ms frame without modifying it. Must be called
  // before Process() for the same frame.
  void Analyze(const float* const* channels);

  // Applies noise suppression to the current 10 ms frame in place.
  void Process(float* const* channels);

  // Allows the suppressor to skip output processing when the output is not
  // going to be used, for instance while muted.
  void SetCaptureOutputUsage(bool capture_output_used);

  size_t sample_rate_hz() const { return sample_rate_hz_; }
  size_t num_channels() const { return num_channels_; }
  size_t num_frames_per_channel() const { return num_frames_per_channel_; }

 private:
  class Impl;

  const size_t sample_rate_hz_;
  const size_t num_channels_;
  const size_t num_frames_per_channel_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace webrtc

#endif  // API_NOISE_SUPPRESSION_H_
