// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

namespace {

static const int kMouseRadius = 20;

uint8_t RandUint8(uint8_t min, uint8_t max) {
  uint64_t r = rand();
  uint8_t result = static_cast<uint8_t>(r * (max - min + 1) / RAND_MAX) + min;
  return result;
}

uint32_t MakeColor(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t a = 255;
  PP_ImageDataFormat format = pp::ImageData::GetNativeImageDataFormat();
  if (format == PP_IMAGEDATAFORMAT_BGRA_PREMUL) {
    return (a << 24) | (r << 16) | (g << 8) | b;
  } else {
    return (a << 24) | (b << 16) | (g << 8) | r;
  }
}

}  // namespace


class Graphics2DInstance : public pp::Instance {
 public:
  explicit Graphics2DInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        mouse_down_(false),
        buffer_(NULL) {}

  ~Graphics2DInstance() {
    delete[] buffer_;
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);

    unsigned int seed = 1;
    srand(seed);
    CreatePalette();
    return true;
  }

  virtual void DidChangeView(const pp::View& view) {
    pp::Size new_size = view.GetRect().size();
    bool had_context = !context_.is_null();

    if (!CreateContext(new_size)) {
      // failed.
      return;
    }

    if (!had_context) {
      MainLoop(0);
    }
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    if (!buffer_)
      return true;

    if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN ||
        event.GetType() == PP_INPUTEVENT_TYPE_MOUSEMOVE) {
      pp::MouseInputEvent mouse_event(event);

      if (mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_NONE)
        return true;

      mouse_ = mouse_event.GetPosition();
      mouse_down_ = true;
    }

    if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEUP) {
      mouse_down_ = false;
    }

    return true;
  }

 private:
  void CreatePalette() {
    for (int i = 0; i < 64; ++i) {
      // Black -> Red
      palette_[i] = MakeColor(i * 2, 0, 0);
      palette_[i + 64] = MakeColor(128 + i * 2, 0, 0);
      // Red -> Yellow
      palette_[i + 128] = MakeColor(255, i * 4, 0);
      // Yellow -> White
      palette_[i + 192] = MakeColor(255, 255, i * 4);
    }
  }

  bool CreateContext(const pp::Size& new_size) {
    const bool kIsAlwaysOpaque = true;
    context_ = pp::Graphics2D(this, new_size, kIsAlwaysOpaque);
    if (!BindGraphics(context_)) {
      fprintf(stderr, "Unable to bind 2d context!\n");
      context_ = pp::Graphics2D();
      return false;
    }

    // Create an ImageData object we can draw to that is the same size as the
    // Graphics2D context.
    const bool kInitToZero = true;
    PP_ImageDataFormat format = pp::ImageData::GetNativeImageDataFormat();
    image_data_ = pp::ImageData(this, format, new_size, kInitToZero);

    // Allocate a buffer of palette entries of the same size.
    buffer_ = new uint8_t[new_size.width() * new_size.height()];

    return true;
  }

  void Update() {
    // Old-school fire technique cribbed from
    // http://ionicsolutions.net/2011/12/30/demo-fire-effect/
    UpdateCoals();
    DrawMouse();
    UpdateFlames();
  }

  void UpdateCoals() {
    const pp::Size& size = image_data_.size();
    int width = size.width();
    int height = size.height();
    size_t span = 0;

    // Draw two rows of random values at the bottom.
    for (int y = height - 2; y < height; ++y) {
      size_t offset = y * width;
      for (int x = 0; x < width; ++x) {
        // On a random chance, draw some longer strips of brighter colors.
        if (span || RandUint8(1, 4) == 1) {
          if (!span)
            span = RandUint8(10, 20);
          buffer_[offset + x] = RandUint8(128, 255);
          span--;
        } else {
          buffer_[offset + x] = RandUint8(32, 96);
        }
      }
    }
  }

  void UpdateFlames() {
    const pp::Size& size = image_data_.size();
    int width = size.width();
    int height = size.height();
    for (int y = 1; y < height - 1; ++y) {
      size_t offset = y * width;
      for (int x = 1; x < width - 1; ++x) {
        int sum = 0;
        sum += buffer_[offset - width + x - 1];
        sum += buffer_[offset - width + x + 1];
        sum += buffer_[offset         + x - 1];
        sum += buffer_[offset         + x + 1];
        sum += buffer_[offset + width + x - 1];
        sum += buffer_[offset + width + x];
        sum += buffer_[offset + width + x + 1];
        buffer_[offset - width + x] = sum / 7;
      }
    }
  }

  void DrawMouse() {
    if (!mouse_down_)
      return;

    const pp::Size& size = image_data_.size();
    int width = size.width();
    int height = size.height();

    // Draw a circle at the mouse position.
    int radius = kMouseRadius;
    int cx = mouse_.x();
    int cy = mouse_.y();
    int minx = cx - radius <= 0 ? 1 : cx - radius;
    int maxx = cx + radius >= width ? width - 1 : cx + radius;
    int miny = cy - radius <= 0 ? 1 : cy - radius;
    int maxy = cy + radius >= height ? height - 1 : cy + radius;
    for (int y = miny; y < maxy; ++y) {
      for (int x = minx; x < maxx; ++x) {
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < radius * radius) {
          buffer_[y * width + x] = RandUint8(192, 255);
        }
      }
    }
  }

  void Paint() {
    const pp::Size& size = image_data_.size();
    uint32_t* data = static_cast<uint32_t*>(image_data_.data());
    uint32_t num_pixels = size.width() * size.height();
    size_t offset = 0;
    for (uint32_t i = 0; i < num_pixels; ++i) {
      data[offset] = palette_[buffer_[offset]];
      offset++;
    }
    context_.PaintImageData(image_data_, pp::Point());
  }

  void MainLoop(int32_t) {
    Update();
    Paint();
    context_.Flush(
        callback_factory_.NewCallback(&Graphics2DInstance::MainLoop));
  }

  pp::CompletionCallbackFactory<Graphics2DInstance> callback_factory_;
  pp::Graphics2D context_;
  pp::ImageData image_data_;
  pp::Point mouse_;
  bool mouse_down_;
  uint8_t* buffer_;
  uint32_t palette_[256];
};

class Graphics2DModule : public pp::Module {
 public:
  Graphics2DModule() : pp::Module() {}
  virtual ~Graphics2DModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Graphics2DInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new Graphics2DModule(); }
}  // namespace pp
