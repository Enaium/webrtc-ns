/*
 *  Copyright (c) 2026 The webrtc-ns authors.
 *
 *  Use of this source code is governed by a BSD-style license that can be
 *  found in the LICENSE file in the root of the source tree.
 */

#include "api/noise_suppression.h"

#include <algorithm>
#include <cstddef>

#include "audio_processing/audio_buffer.h"
#include "audio_processing/ns/noise_suppressor.h"
#include "audio_processing/ns/ns_config.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

NsConfig::SuppressionLevel ToNsLevel(NoiseSuppression::Level level) {
  switch (level) {
    case NoiseSuppression::Level::k6dB:
      return NsConfig::SuppressionLevel::k6dB;
    case NoiseSuppression::Level::k12dB:
      return NsConfig::SuppressionLevel::k12dB;
    case NoiseSuppression::Level::k18dB:
      return NsConfig::SuppressionLevel::k18dB;
    case NoiseSuppression::Level::k21dB:
      return NsConfig::SuppressionLevel::k21dB;
  }
  RTC_CHECK_NOTREACHED();
  return NsConfig::SuppressionLevel::k12dB;
}

}  // namespace

class NoiseSuppression::Impl {
 public:
  Impl(size_t sample_rate_hz,
       size_t num_channels,
       size_t num_frames_per_channel,
       const Config& config)
      : audio_(sample_rate_hz,
               num_channels,
               sample_rate_hz,
               num_channels,
               sample_rate_hz,
               num_channels),
        suppressor_([&config] {
          NsConfig ns_config;
          ns_config.target_level = ToNsLevel(config.level);
          return ns_config;
        }(),
                    sample_rate_hz,
                    num_channels),
        num_frames_per_channel_(num_frames_per_channel) {}

  void Analyze(const float* const* channels) {
    CopyIn(channels);
    audio_.SplitIntoFrequencyBands();
    suppressor_.Analyze(audio_);
  }

  void Process(float* const* channels) {
    if (!capture_output_used_) {
      return;
    }
    CopyIn(channels);
    audio_.SplitIntoFrequencyBands();
    suppressor_.Process(&audio_);
    audio_.MergeFrequencyBands();

    const size_t num_channels = audio_.num_channels();
    for (size_t ch = 0; ch < num_channels; ++ch) {
      std::copy_n(audio_.channels_const()[ch], num_frames_per_channel_,
                  channels[ch]);
    }
  }

  void SetCaptureOutputUsage(bool capture_output_used) {
    capture_output_used_ = capture_output_used;
    suppressor_.SetCaptureOutputUsage(capture_output_used);
  }

 private:
  void CopyIn(const float* const* channels) {
    const size_t num_channels = audio_.num_channels();
    for (size_t ch = 0; ch < num_channels; ++ch) {
      std::copy_n(channels[ch], num_frames_per_channel_,
                  audio_.channels()[ch]);
    }
  }

  AudioBuffer audio_;
  NoiseSuppressor suppressor_;
  const size_t num_frames_per_channel_;
  bool capture_output_used_ = true;
};

NoiseSuppression::NoiseSuppression(size_t sample_rate_hz, size_t num_channels)
    : NoiseSuppression(sample_rate_hz, num_channels, Config()) {}

NoiseSuppression::NoiseSuppression(size_t sample_rate_hz,
                                   size_t num_channels,
                                   const Config& config)
    : sample_rate_hz_(sample_rate_hz),
      num_channels_(num_channels),
      num_frames_per_channel_(sample_rate_hz / 100),
      impl_(nullptr) {
  RTC_CHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000 ||
            sample_rate_hz == 48000)
      << "Unsupported sample rate: " << sample_rate_hz;
  RTC_CHECK_GT(num_channels, 0u);
  impl_ = std::make_unique<Impl>(sample_rate_hz_, num_channels_,
                                 num_frames_per_channel_, config);
}

NoiseSuppression::~NoiseSuppression() = default;

void NoiseSuppression::Analyze(const float* const* channels) {
  impl_->Analyze(channels);
}

void NoiseSuppression::Process(float* const* channels) {
  impl_->Process(channels);
}

void NoiseSuppression::SetCaptureOutputUsage(bool capture_output_used) {
  impl_->SetCaptureOutputUsage(capture_output_used);
}

}  // namespace webrtc
