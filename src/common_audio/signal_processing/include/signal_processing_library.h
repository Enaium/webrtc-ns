/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Minimal subset of WebRTC's signal_processing_library.h containing only the
// float QMF functions used by SplittingFilter. The full library is not part of
// this standalone NS extraction.

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SIGNAL_PROCESSING_LIBRARY_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SIGNAL_PROCESSING_LIBRARY_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void WebRtcSpl_AnalysisQMF(const float* in_data,
                           size_t in_data_length,
                           float* low_band,
                           float* high_band,
                           float* filter_state1,
                           float* filter_state2);

void WebRtcSpl_SynthesisQMF(const float* low_band,
                            const float* high_band,
                            size_t band_length,
                            float* out_data,
                            float* filter_state1,
                            float* filter_state2);

#ifdef __cplusplus
}
#endif

#endif  // COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SIGNAL_PROCESSING_LIBRARY_H_
