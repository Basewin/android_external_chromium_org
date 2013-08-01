// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_H_
#define CC_LAYERS_SCROLLBAR_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/contents_scaling_layer.h"
#include "cc/layers/scrollbar_theme_painter.h"
#include "cc/resources/layer_updater.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {
class ScrollbarThemeComposite;

class CC_EXPORT ScrollbarLayer : public ContentsScalingLayer {
 public:
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  static scoped_refptr<ScrollbarLayer> Create(
      scoped_ptr<Scrollbar> scrollbar,
      int scroll_layer_id);

  int scroll_layer_id() const { return scroll_layer_id_; }
  void SetScrollLayerId(int id);

  virtual bool OpacityCanAnimateOnImplThread() const OVERRIDE;

  ScrollbarOrientation Orientation() const;

  // Layer interface
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion) OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE;

  virtual ScrollbarLayer* ToScrollbarLayer() OVERRIDE;

 protected:
  ScrollbarLayer(scoped_ptr<Scrollbar> scrollbar,
                 int scroll_layer_id);
  virtual ~ScrollbarLayer();

 private:
  gfx::Rect ScrollbarLayerRectToContentRect(gfx::Rect layer_rect) const;
  gfx::Rect OriginThumbRect() const;

  int MaxTextureSize();
  float ClampScaleToMaxTextureSize(float scale);

  scoped_refptr<UIResourceBitmap> RasterizeTrack();
  scoped_refptr<UIResourceBitmap> RasterizeThumb();

  scoped_ptr<Scrollbar> scrollbar_;

  int thumb_thickness_;
  int thumb_length_;
  gfx::Rect track_rect_;
  gfx::Rect thumb_rect_;
  int scroll_layer_id_;

  scoped_ptr<ScopedUIResource> track_resource_;
  scoped_ptr<ScopedUIResource> thumb_resource_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_LAYER_H_
