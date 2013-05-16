// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Initial input buffer layout, dividing into regions r0_ to r4_ (note: r0_, r3_
// and r4_ will move after the first load):
//
// |----------------|-----------------------------------------|----------------|
//
//                                        request_frames_
//                   <--------------------------------------------------------->
//                                    r0_ (during first load)
//
//  kKernelSize / 2   kKernelSize / 2         kKernelSize / 2   kKernelSize / 2
// <---------------> <--------------->       <---------------> <--------------->
//        r1_               r2_                     r3_               r4_
//
//                             block_size_ == r4_ - r2_
//                   <--------------------------------------->
//
//                                                  request_frames_
//                                    <------------------ ... ----------------->
//                                               r0_ (during second load)
//
// On the second request r0_ slides to the right by kKernelSize / 2 and r3_, r4_
// and block_size_ are reinitialized via step (3) in the algorithm below.
//
// These new regions remain constant until a Flush() occurs.  While complicated,
// this allows us to reduce jitter by always requesting the same amount from the
// provided callback.
//
// The algorithm:
//
// 1) Allocate input_buffer of size: request_frames_ + kKernelSize; this ensures
//    there's enough room to read request_frames_ from the callback into region
//    r0_ (which will move between the first and subsequent passes).
//
// 2) Let r1_, r2_ each represent half the kernel centered around r0_:
//
//        r0_ = input_buffer_ + kKernelSize / 2
//        r1_ = input_buffer_
//        r2_ = r0_
//
//    r0_ is always request_frames_ in size.  r1_, r2_ are kKernelSize / 2 in
//    size.  r1_ must be zero initialized to avoid convolution with garbage (see
//    step (5) for why).
//
// 3) Let r3_, r4_ each represent half the kernel right aligned with the end of
//    r0_ and choose block_size_ as the distance in frames between r4_ and r2_:
//
//        r3_ = r0_ + request_frames_ - kKernelSize
//        r4_ = r0_ + request_frames_ - kKernelSize / 2
//        block_size_ = r4_ - r2_ = request_frames_ - kKernelSize / 2
//
// 4) Consume request_frames_ frames into r0_.
//
// 5) Position kernel centered at start of r2_ and generate output frames until
//    the kernel is centered at the start of r4_ or we've finished generating
//    all the output frames.
//
// 6) Wrap left over data from the r3_ to r1_ and r4_ to r2_.
//
// 7) If we're on the second load, in order to avoid overwriting the frames we
//    just wrapped from r4_ we need to slide r0_ to the right by the size of
//    r4_, which is kKernelSize / 2:
//
//        r0_ = r0_ + kKernelSize / 2 = input_buffer_ + kKernelSize
//
//    r3_, r4_, and block_size_ then need to be reinitialized, so goto (3).
//
// 8) Else, if we're not on the second load, goto (4).
//
// Note: we're glossing over how the sub-sample handling works with
// |virtual_source_idx_|, etc.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "media/base/sinc_resampler.h"

#include <cmath>
#include <limits>

#include "base/cpu.h"
#include "base/logging.h"

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#include <arm_neon.h>
#endif

namespace media {

static double SincScaleFactor(double io_ratio) {
  // |sinc_scale_factor| is basically the normalized cutoff frequency of the
  // low-pass filter.
  double sinc_scale_factor = io_ratio > 1.0 ? 1.0 / io_ratio : 1.0;

  // The sinc function is an idealized brick-wall filter, but since we're
  // windowing it the transition from pass to stop does not happen right away.
  // So we should adjust the low pass filter cutoff slightly downward to avoid
  // some aliasing at the very high-end.
  // TODO(crogers): this value is empirical and to be more exact should vary
  // depending on kKernelSize.
  sinc_scale_factor *= 0.9;

  return sinc_scale_factor;
}

// If we know the minimum architecture at compile time, avoid CPU detection.
// Force NaCl code to use C routines since (at present) nothing there uses these
// methods and plumbing the -msse built library is non-trivial.  iOS lies
// about its architecture, so we also need to exclude it here.
#if defined(ARCH_CPU_X86_FAMILY) && !defined(OS_NACL) && !defined(OS_IOS)
#if defined(__SSE__)
#define CONVOLVE_FUNC Convolve_SSE
void SincResampler::InitializeCPUSpecificFeatures() {}
#else
// X86 CPU detection required.  Functions will be set by
// InitializeCPUSpecificFeatures().
// TODO(dalecurtis): Once Chrome moves to an SSE baseline this can be removed.
#define CONVOLVE_FUNC g_convolve_proc_

typedef float (*ConvolveProc)(const float*, const float*, const float*, double);
static ConvolveProc g_convolve_proc_ = NULL;

void SincResampler::InitializeCPUSpecificFeatures() {
  CHECK(!g_convolve_proc_);
  g_convolve_proc_ = base::CPU().has_sse() ? Convolve_SSE : Convolve_C;
}
#endif
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#define CONVOLVE_FUNC Convolve_NEON
void SincResampler::InitializeCPUSpecificFeatures() {}
#else
// Unknown architecture.
#define CONVOLVE_FUNC Convolve_C
void SincResampler::InitializeCPUSpecificFeatures() {}
#endif

SincResampler::SincResampler(double io_sample_rate_ratio,
                             size_t request_frames,
                             const ReadCB& read_cb)
    : io_sample_rate_ratio_(io_sample_rate_ratio),
      read_cb_(read_cb),
      request_frames_(request_frames),
      input_buffer_size_(request_frames_ + kKernelSize),
      // Create input buffers with a 16-byte alignment for SSE optimizations.
      kernel_storage_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kKernelStorageSize, 16))),
      kernel_pre_sinc_storage_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kKernelStorageSize, 16))),
      kernel_window_storage_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * kKernelStorageSize, 16))),
      input_buffer_(static_cast<float*>(
          base::AlignedAlloc(sizeof(float) * input_buffer_size_, 16))),
      r1_(input_buffer_.get()),
      r2_(input_buffer_.get() + kKernelSize / 2) {
  Flush();
  CHECK_GT(block_size_, static_cast<size_t>(kKernelSize))
      << "block_size must be greater than kKernelSize!";

  memset(kernel_storage_.get(), 0,
         sizeof(*kernel_storage_.get()) * kKernelStorageSize);
  memset(kernel_pre_sinc_storage_.get(), 0,
         sizeof(*kernel_pre_sinc_storage_.get()) * kKernelStorageSize);
  memset(kernel_window_storage_.get(), 0,
         sizeof(*kernel_window_storage_.get()) * kKernelStorageSize);

  InitializeKernel();
}

SincResampler::~SincResampler() {}

void SincResampler::UpdateRegions(bool second_load) {
  // Setup various region pointers in the buffer (see diagram above).  If we're
  // on the second load we need to slide r0_ to the right by kKernelSize / 2.
  r0_ = input_buffer_.get() + (second_load ? kKernelSize : kKernelSize / 2);
  r3_ = r0_ + request_frames_ - kKernelSize;
  r4_ = r0_ + request_frames_ - kKernelSize / 2;
  block_size_ = r4_ - r2_;

  // r1_ at the beginning of the buffer.
  CHECK_EQ(r1_, input_buffer_.get());
  // r1_ left of r2_, r4_ left of r3_ and size correct.
  CHECK_EQ(r2_ - r1_, r4_ - r3_);
  // r2_ left of r3.
  CHECK_LT(r2_, r3_);
}

void SincResampler::InitializeKernel() {
  // Blackman window parameters.
  static const double kAlpha = 0.16;
  static const double kA0 = 0.5 * (1.0 - kAlpha);
  static const double kA1 = 0.5;
  static const double kA2 = 0.5 * kAlpha;

  // Generates a set of windowed sinc() kernels.
  // We generate a range of sub-sample offsets from 0.0 to 1.0.
  const double sinc_scale_factor = SincScaleFactor(io_sample_rate_ratio_);
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    const float subsample_offset =
        static_cast<float>(offset_idx) / kKernelOffsetCount;

    for (int i = 0; i < kKernelSize; ++i) {
      const int idx = i + offset_idx * kKernelSize;
      const float pre_sinc = M_PI * (i - kKernelSize / 2 - subsample_offset);
      kernel_pre_sinc_storage_[idx] = pre_sinc;

      // Compute Blackman window, matching the offset of the sinc().
      const float x = (i - subsample_offset) / kKernelSize;
      const float window = kA0 - kA1 * cos(2.0 * M_PI * x) + kA2
          * cos(4.0 * M_PI * x);
      kernel_window_storage_[idx] = window;

      // Compute the sinc with offset, then window the sinc() function and store
      // at the correct offset.
      if (pre_sinc == 0) {
        kernel_storage_[idx] = sinc_scale_factor * window;
      } else {
        kernel_storage_[idx] =
            window * sin(sinc_scale_factor * pre_sinc) / pre_sinc;
      }
    }
  }
}

void SincResampler::SetRatio(double io_sample_rate_ratio) {
  if (fabs(io_sample_rate_ratio_ - io_sample_rate_ratio) <
      std::numeric_limits<double>::epsilon()) {
    return;
  }

  io_sample_rate_ratio_ = io_sample_rate_ratio;

  // Optimize reinitialization by reusing values which are independent of
  // |sinc_scale_factor|.  Provides a 3x speedup.
  const double sinc_scale_factor = SincScaleFactor(io_sample_rate_ratio_);
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    for (int i = 0; i < kKernelSize; ++i) {
      const int idx = i + offset_idx * kKernelSize;
      const float window = kernel_window_storage_[idx];
      const float pre_sinc = kernel_pre_sinc_storage_[idx];

      if (pre_sinc == 0) {
        kernel_storage_[idx] = sinc_scale_factor * window;
      } else {
        kernel_storage_[idx] =
            window * sin(sinc_scale_factor * pre_sinc) / pre_sinc;
      }
    }
  }
}

void SincResampler::Resample(int frames, float* destination) {
  int remaining_frames = frames;

  // Step (1) -- Prime the input buffer at the start of the input stream.
  if (!buffer_primed_) {
    read_cb_.Run(request_frames_, r0_);
    buffer_primed_ = true;
  }

  // Step (2) -- Resample!
  while (remaining_frames) {
    while (virtual_source_idx_ < block_size_) {
      // |virtual_source_idx_| lies in between two kernel offsets so figure out
      // what they are.
      const int source_idx = virtual_source_idx_;
      const double subsample_remainder = virtual_source_idx_ - source_idx;

      const double virtual_offset_idx =
          subsample_remainder * kKernelOffsetCount;
      const int offset_idx = virtual_offset_idx;

      // We'll compute "convolutions" for the two kernels which straddle
      // |virtual_source_idx_|.
      const float* k1 = kernel_storage_.get() + offset_idx * kKernelSize;
      const float* k2 = k1 + kKernelSize;

      // Ensure |k1|, |k2| are 16-byte aligned for SIMD usage.  Should always be
      // true so long as kKernelSize is a multiple of 16.
      DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(k1) & 0x0F);
      DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(k2) & 0x0F);

      // Initialize input pointer based on quantized |virtual_source_idx_|.
      const float* input_ptr = r1_ + source_idx;

      // Figure out how much to weight each kernel's "convolution".
      const double kernel_interpolation_factor =
          virtual_offset_idx - offset_idx;
      *destination++ = CONVOLVE_FUNC(
          input_ptr, k1, k2, kernel_interpolation_factor);

      // Advance the virtual index.
      virtual_source_idx_ += io_sample_rate_ratio_;

      if (!--remaining_frames)
        return;
    }

    // Wrap back around to the start.
    virtual_source_idx_ -= block_size_;

    // Step (3) -- Copy r3_, r4_ to r1_, r2_.
    // This wraps the last input frames back to the start of the buffer.
    memcpy(r1_, r3_, sizeof(*input_buffer_.get()) * kKernelSize);

    // Step (4) -- Reinitialize regions if necessary.
    if (r0_ == r2_)
      UpdateRegions(true);

    // Step (5) -- Refresh the buffer with more input.
    read_cb_.Run(request_frames_, r0_);
  }
}

#undef CONVOLVE_FUNC

int SincResampler::ChunkSize() const {
  return block_size_ / io_sample_rate_ratio_;
}

void SincResampler::Flush() {
  virtual_source_idx_ = 0;
  buffer_primed_ = false;
  memset(input_buffer_.get(), 0,
         sizeof(*input_buffer_.get()) * input_buffer_size_);
  UpdateRegions(false);
}

float SincResampler::Convolve_C(const float* input_ptr, const float* k1,
                                const float* k2,
                                double kernel_interpolation_factor) {
  float sum1 = 0;
  float sum2 = 0;

  // Generate a single output sample.  Unrolling this loop hurt performance in
  // local testing.
  int n = kKernelSize;
  while (n--) {
    sum1 += *input_ptr * *k1++;
    sum2 += *input_ptr++ * *k2++;
  }

  // Linearly interpolate the two "convolutions".
  return (1.0 - kernel_interpolation_factor) * sum1
      + kernel_interpolation_factor * sum2;
}

#if defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
float SincResampler::Convolve_NEON(const float* input_ptr, const float* k1,
                                   const float* k2,
                                   double kernel_interpolation_factor) {
  float32x4_t m_input;
  float32x4_t m_sums1 = vmovq_n_f32(0);
  float32x4_t m_sums2 = vmovq_n_f32(0);

  const float* upper = input_ptr + kKernelSize;
  for (; input_ptr < upper; ) {
    m_input = vld1q_f32(input_ptr);
    input_ptr += 4;
    m_sums1 = vmlaq_f32(m_sums1, m_input, vld1q_f32(k1));
    k1 += 4;
    m_sums2 = vmlaq_f32(m_sums2, m_input, vld1q_f32(k2));
    k2 += 4;
  }

  // Linearly interpolate the two "convolutions".
  m_sums1 = vmlaq_f32(
      vmulq_f32(m_sums1, vmovq_n_f32(1.0 - kernel_interpolation_factor)),
      m_sums2, vmovq_n_f32(kernel_interpolation_factor));

  // Sum components together.
  float32x2_t m_half = vadd_f32(vget_high_f32(m_sums1), vget_low_f32(m_sums1));
  return vget_lane_f32(vpadd_f32(m_half, m_half), 0);
}
#endif

}  // namespace media
