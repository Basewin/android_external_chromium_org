// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/encoder_vp8.h"

#include "base/logging.h"
#include "base/sys_info.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/util.h"
#include "remoting/proto/video.pb.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx/libvpx.h"
}

namespace {

// Defines the dimension of a macro block. This is used to compute the active
// map for the encoder.
const int kMacroBlockSize = 16;

}  // namespace remoting

namespace remoting {

EncoderVp8::EncoderVp8()
    : initialized_(false),
      codec_(NULL),
      image_(NULL),
      active_map_width_(0),
      active_map_height_(0),
      last_timestamp_(0),
      size_(SkISize::Make(0, 0)) {
}

EncoderVp8::~EncoderVp8() {
  Destroy();
}

void EncoderVp8::Destroy() {
  if (initialized_) {
    vpx_codec_err_t ret = vpx_codec_destroy(codec_.get());
    DCHECK(ret == VPX_CODEC_OK) << "Failed to destroy codec";
    initialized_ = false;
  }
}

bool EncoderVp8::Init(const SkISize& size) {
  Destroy();
  size_ = size;
  codec_.reset(new vpx_codec_ctx_t());
  image_.reset(new vpx_image_t());
  memset(image_.get(), 0, sizeof(vpx_image_t));

  image_->fmt = VPX_IMG_FMT_YV12;

  // libvpx seems to require both to be assigned.
  image_->d_w = size.width();
  image_->w = size.width();
  image_->d_h = size.height();
  image_->h = size.height();

  // Initialize active map.
  active_map_width_ = (size.width() + kMacroBlockSize - 1) / kMacroBlockSize;
  active_map_height_ = (size.height() + kMacroBlockSize - 1) / kMacroBlockSize;
  active_map_.reset(new uint8[active_map_width_ * active_map_height_]);

  // YUV image size is 1.5 times of a plane. Multiplication is performed first
  // to avoid rounding error.
  const int y_plane_size = size.width() * size.height();
  const int uv_width = (size.width() + 1) / 2;
  const int uv_height = (size.height() + 1) / 2;
  const int uv_plane_size = uv_width * uv_height;
  const int yuv_image_size = y_plane_size + uv_plane_size * 2;

  // libvpx may try to access memory after the buffer (it still
  // doesn't use it) - it copies the data in 16x16 blocks:
  // crbug.com/119633 . Here we workaround that problem by adding
  // padding at the end of the buffer. Overreading to U and V buffers
  // is safe so the padding is necessary only at the end.
  //
  // TODO(sergeyu): Remove this padding when the bug is fixed in libvpx.
  const int active_map_area = active_map_width_ * kMacroBlockSize *
      active_map_height_ * kMacroBlockSize;
  const int padding_size = active_map_area - y_plane_size;
  const int buffer_size = yuv_image_size + padding_size;

  yuv_image_.reset(new uint8[buffer_size]);

  // Reset image value to 128 so we just need to fill in the y plane.
  memset(yuv_image_.get(), 128, yuv_image_size);

  // Fill in the information for |image_|.
  unsigned char* image = reinterpret_cast<unsigned char*>(yuv_image_.get());
  image_->planes[0] = image;
  image_->planes[1] = image + y_plane_size;
  image_->planes[2] = image + y_plane_size + uv_plane_size;

  // In YV12 Y plane has full width, UV plane has half width because of
  // subsampling.
  image_->stride[0] = image_->w;
  image_->stride[1] = image_->w / 2;
  image_->stride[2] = image_->w / 2;

  vpx_codec_enc_cfg_t config;
  const vpx_codec_iface_t* algo = vpx_codec_vp8_cx();
  CHECK(algo);
  vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
  if (ret != VPX_CODEC_OK)
    return false;

  config.rc_target_bitrate = size.width() * size.height() *
      config.rc_target_bitrate / config.g_w / config.g_h;
  config.g_w = size.width();
  config.g_h = size.height();
  config.g_pass = VPX_RC_ONE_PASS;

  // Value of 2 means using the real time profile. This is basically a
  // redundant option since we explicitly select real time mode when doing
  // encoding.
  config.g_profile = 2;

  // Using 2 threads gives a great boost in performance for most systems with
  // adequate processing power. NB: Going to multiple threads on low end
  // windows systems can really hurt performance.
  // http://crbug.com/99179
  config.g_threads = (base::SysInfo::NumberOfProcessors() > 2) ? 2 : 1;
  config.rc_min_quantizer = 20;
  config.rc_max_quantizer = 30;
  config.g_timebase.num = 1;
  config.g_timebase.den = 20;

  if (vpx_codec_enc_init(codec_.get(), algo, &config, 0))
    return false;

  // Value of 16 will have the smallest CPU load. This turns off subpixel
  // motion search.
  if (vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 16))
    return false;

  // Use the lowest level of noise sensitivity so as to spend less time
  // on motion estimation and inter-prediction mode.
  if (vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0))
    return false;
  return true;
}

void EncoderVp8::PrepareImage(scoped_refptr<CaptureData> capture_data,
                              SkRegion* updated_region) {
  // Perform RGB->YUV conversion.
  CHECK_EQ(capture_data->pixel_format(), media::VideoFrame::RGB32)
    << "Only RGB32 is supported";

  const SkRegion& region = capture_data->dirty_region();
  if (region.isEmpty()) {
    updated_region->setEmpty();
    return;
  }

  // Align rectangles in the region, to avoid encoding artefacts.
  // Note that this also results in rectangles with even-aligned positions,
  // which is a requirement for some of the conversion routines to work.
  std::vector<SkIRect> updated_rects;
  for (SkRegion::Iterator r(region); !r.done(); r.next()) {
    updated_rects.push_back(AlignRect(r.rect()));
  }
  if (!updated_rects.empty()) {
    updated_region->setRects(&updated_rects[0], updated_rects.size());
  }

  // Clip to the screen again, in case it has non-aligned size.
  // Note that we round the screen down to even dimensions to satisfy the
  // limitations of some of the conversion routines.
  int even_width = RoundToTwosMultiple(image_->w);
  int even_height = RoundToTwosMultiple(image_->h);
  updated_region->op(SkRegion(SkIRect::MakeWH(even_width, even_height)),
                     SkRegion::kIntersect_Op);

  // Convert the updated region to YUV ready for encoding.
  const uint8* in = capture_data->data_planes().data[0];
  const int in_stride = capture_data->data_planes().strides[0];
  const int plane_size =
      capture_data->size().width() * capture_data->size().height();
  uint8* y_out = yuv_image_.get();
  uint8* u_out = yuv_image_.get() + plane_size;
  uint8* v_out = yuv_image_.get() + plane_size + plane_size / 4;
  const int y_stride = image_->stride[0];
  const int uv_stride = image_->stride[1];

  for (SkRegion::Iterator r(*updated_region); !r.done(); r.next()) {
    const SkIRect& rect = r.rect();
    ConvertRGB32ToYUVWithRect(
        in, y_out, u_out, v_out,
        rect.x(), rect.y(), rect.width(), rect.height(),
        in_stride, y_stride, uv_stride);
  }
}

void EncoderVp8::PrepareActiveMap(const SkRegion& updated_region) {
  // Clear active map first.
  memset(active_map_.get(), 0, active_map_width_ * active_map_height_);

  // Mark updated areas active.
  for (SkRegion::Iterator r(updated_region); !r.done(); r.next()) {
    const SkIRect& rect = r.rect();
    int left = rect.left() / kMacroBlockSize;
    int right = (rect.right() - 1) / kMacroBlockSize;
    int top = rect.top() / kMacroBlockSize;
    int bottom = (rect.bottom() - 1) / kMacroBlockSize;
    CHECK(right < active_map_width_);
    CHECK(bottom < active_map_height_);

    uint8* map = active_map_.get() + top * active_map_width_;
    for (int y = top; y <= bottom; ++y) {
      for (int x = left; x <= right; ++x)
        map[x] = 1;
      map += active_map_width_;
    }
  }
}

void EncoderVp8::Encode(scoped_refptr<CaptureData> capture_data,
                        bool key_frame,
                        const DataAvailableCallback& data_available_callback) {
  if (!initialized_ || (capture_data->size() != size_)) {
    bool ret = Init(capture_data->size());
    // TODO(hclam): Handle error better.
    CHECK(ret) << "Initialization of encoder failed";
    initialized_ = ret;
  }

  // Convert the updated capture data ready for encode.
  SkRegion updated_region;
  PrepareImage(capture_data, &updated_region);

  // Update active map based on updated region.
  PrepareActiveMap(updated_region);

  // Apply active map to the encoder.
  vpx_active_map_t act_map;
  act_map.rows = active_map_height_;
  act_map.cols = active_map_width_;
  act_map.active_map = active_map_.get();
  if (vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &act_map)) {
    LOG(ERROR) << "Unable to apply active map";
  }

  // Do the actual encoding.
  vpx_codec_err_t ret = vpx_codec_encode(codec_.get(), image_.get(),
                                         last_timestamp_,
                                         1, 0, VPX_DL_REALTIME);
  DCHECK_EQ(ret, VPX_CODEC_OK)
      << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n"
      << "Details: " << vpx_codec_error(codec_.get()) << "\n"
      << vpx_codec_error_detail(codec_.get());

  // TODO(hclam): Apply the proper timestamp here.
  last_timestamp_ += 50;

  // Read the encoded data.
  vpx_codec_iter_t iter = NULL;
  bool got_data = false;

  // TODO(hclam): Make sure we get exactly one frame from the packet.
  // TODO(hclam): We should provide the output buffer to avoid one copy.
  scoped_ptr<VideoPacket> packet(new VideoPacket());

  while (!got_data) {
    const vpx_codec_cx_pkt_t* vpx_packet = vpx_codec_get_cx_data(codec_.get(),
                                                                 &iter);
    if (!vpx_packet)
      continue;

    switch (vpx_packet->kind) {
      case VPX_CODEC_CX_FRAME_PKT:
        got_data = true;
        // TODO(sergeyu): Split each frame into multiple partitions.
        packet->set_data(vpx_packet->data.frame.buf, vpx_packet->data.frame.sz);
        break;
      default:
        break;
    }
  }

  // Construct the VideoPacket message.
  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);
  packet->set_flags(VideoPacket::FIRST_PACKET | VideoPacket::LAST_PACKET |
                     VideoPacket::LAST_PARTITION);
  packet->mutable_format()->set_screen_width(capture_data->size().width());
  packet->mutable_format()->set_screen_height(capture_data->size().height());
  packet->set_capture_time_ms(capture_data->capture_time_ms());
  packet->set_client_sequence_number(capture_data->client_sequence_number());
  for (SkRegion::Iterator r(updated_region); !r.done(); r.next()) {
    Rect* rect = packet->add_dirty_rects();
    rect->set_x(r.rect().x());
    rect->set_y(r.rect().y());
    rect->set_width(r.rect().width());
    rect->set_height(r.rect().height());
  }

  data_available_callback.Run(packet.Pass());
}

}  // namespace remoting
