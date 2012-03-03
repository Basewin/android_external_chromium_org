// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/decoder_vp8.h"

#include <math.h>

#include "base/logging.h"
#include "media/base/media.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/libvpx.h"
}

namespace remoting {

DecoderVp8::DecoderVp8()
    : state_(kUninitialized),
      codec_(NULL),
      last_image_(NULL),
      screen_size_(SkISize::Make(0, 0)) {
}

DecoderVp8::~DecoderVp8() {
  if (codec_) {
    vpx_codec_err_t ret = vpx_codec_destroy(codec_);
    CHECK(ret == VPX_CODEC_OK) << "Failed to destroy codec";
  }
  delete codec_;
}

void DecoderVp8::Initialize(const SkISize& screen_size) {
  screen_size_ = screen_size;
  state_ = kReady;
}

Decoder::DecodeResult DecoderVp8::DecodePacket(const VideoPacket* packet) {
  DCHECK_EQ(kReady, state_);

  // Initialize the codec as needed.
  if (!codec_) {
    codec_ = new vpx_codec_ctx_t();

    // TODO(hclam): Scale the number of threads with number of cores of the
    // machine.
    vpx_codec_dec_cfg config;
    config.w = 0;
    config.h = 0;
    config.threads = 2;
    vpx_codec_err_t ret =
        vpx_codec_dec_init(
            codec_, vpx_codec_vp8_dx(), &config, 0);
    if (ret != VPX_CODEC_OK) {
      LOG(INFO) << "Cannot initialize codec.";
      delete codec_;
      codec_ = NULL;
      state_ = kError;
      return DECODE_ERROR;
    }
  }

  // Do the actual decoding.
  vpx_codec_err_t ret = vpx_codec_decode(
      codec_, reinterpret_cast<const uint8*>(packet->data().data()),
      packet->data().size(), NULL, 0);
  if (ret != VPX_CODEC_OK) {
    LOG(INFO) << "Decoding failed:" << vpx_codec_err_to_string(ret) << "\n"
              << "Details: " << vpx_codec_error(codec_) << "\n"
              << vpx_codec_error_detail(codec_);
    return DECODE_ERROR;
  }

  // Gets the decoded data.
  vpx_codec_iter_t iter = NULL;
  vpx_image_t* image = vpx_codec_get_frame(codec_, &iter);
  if (!image) {
    LOG(INFO) << "No video frame decoded";
    return DECODE_ERROR;
  }
  last_image_ = image;

  SkRegion region;
  for (int i = 0; i < packet->dirty_rects_size(); ++i) {
    Rect remoting_rect = packet->dirty_rects(i);
    SkIRect rect = SkIRect::MakeXYWH(remoting_rect.x(),
                                     remoting_rect.y(),
                                     remoting_rect.width(),
                                     remoting_rect.height());
    region.op(rect, SkRegion::kUnion_Op);
  }

  updated_region_.op(region, SkRegion::kUnion_Op);
  return DECODE_DONE;
}

bool DecoderVp8::IsReadyForData() {
  return state_ == kReady;
}

VideoPacketFormat::Encoding DecoderVp8::Encoding() {
  return VideoPacketFormat::ENCODING_VP8;
}

void DecoderVp8::Invalidate(const SkISize& view_size,
                            const SkRegion& region) {
  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    SkIRect rect = i.rect();
    rect = ScaleRect(rect, view_size, screen_size_);
    updated_region_.op(rect, SkRegion::kUnion_Op);
  }
}

void DecoderVp8::RenderFrame(const SkISize& view_size,
                             const SkIRect& clip_area,
                             uint8* image_buffer,
                             int image_stride,
                             SkRegion* output_region) {
  SkIRect source_clip = SkIRect::MakeWH(last_image_->d_w, last_image_->d_h);

  // ScaleYUVToRGB32WithRect doesn't support up-scaling, and our web-app never
  // intentionally up-scales, so if we see up-scaling (e.g. during host resize
  // or if the user applies page zoom) just don't render anything.
  // TODO(wez): Remove this hack when ScaleYUVToRGB32WithRect can up-scale.
  if (source_clip.width() < view_size.width() ||
      source_clip.height() < view_size.height()) {
    return;
  }

  for (SkRegion::Iterator i(updated_region_); !i.done(); i.next()) {
    // Determine the scaled area affected by this rectangle changing.
    SkIRect rect = i.rect();
    if (!rect.intersect(source_clip))
      continue;
    rect = ScaleRect(rect, screen_size_, view_size);
    if (!rect.intersect(clip_area))
      continue;

    ConvertAndScaleYUVToRGB32Rect(last_image_->planes[0],
                                  last_image_->planes[1],
                                  last_image_->planes[2],
                                  last_image_->stride[0],
                                  last_image_->stride[1],
                                  screen_size_,
                                  source_clip,
                                  image_buffer,
                                  image_stride,
                                  view_size,
                                  clip_area,
                                  rect);

    output_region->op(rect, SkRegion::kUnion_Op);
  }

  updated_region_.op(ScaleRect(clip_area, view_size, screen_size_),
                     SkRegion::kDifference_Op);
}

}  // namespace remoting
