// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_LAYER_QUAD_H_
#define CC_LAYER_QUAD_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "ui/gfx/point_f.h"

namespace gfx {
class QuadF;
}

static const float kAntiAliasingInflateDistance = 0.5f;

namespace cc {

class CC_EXPORT LayerQuad {
 public:
  class Edge {
   public:
    Edge() : x_(0), y_(0), z_(0) {}
    Edge(gfx::PointF p, gfx::PointF q);

    float x() const { return x_; }
    float y() const { return y_; }
    float z() const { return z_; }

    void set_x(float x) { x_ = x; }
    void set_y(float y) { y_ = y; }
    void set_z(float z) { z_ = z; }
    void set(float x, float y, float z) {
      x_ = x;
      y_ = y;
      z_ = z;
    }

    void move_x(float dx) { x_ += dx; }
    void move_y(float dy) { y_ += dy; }
    void move_z(float dz) { z_ += dz; }
    void move(float dx, float dy, float dz) {
      x_ += dx;
      y_ += dy;
      z_ += dz;
    }

    void scale_x(float sx) { x_ *= sx; }
    void scale_y(float sy) { y_ *= sy; }
    void scale_z(float sz) { z_ *= sz; }
    void scale(float sx, float sy, float sz) {
      x_ *= sx;
      y_ *= sy;
      z_ *= sz;
    }
    void scale(float s) { scale(s, s, s); }

    gfx::PointF Intersect(const Edge& e) const {
      return gfx::PointF(
          (y() * e.z() - e.y() * z()) / (x() * e.y() - e.x() * y()),
          (x() * e.z() - e.x() * z()) / (e.x() * y() - x() * e.y()));
    }

   private:
    float x_;
    float y_;
    float z_;
  };

  LayerQuad(const Edge& left,
            const Edge& top,
            const Edge& right,
            const Edge& bottom);
  LayerQuad(const gfx::QuadF& quad);

  Edge left() const { return left_; }
  Edge top() const { return top_; }
  Edge right() const { return right_; }
  Edge bottom() const { return bottom_; }

  void InflateX(float dx) { left_.move_z(dx); right_.move_z(dx); }
  void InflateY(float dy) { top_.move_z(dy); bottom_.move_z(dy); }
  void Inflate(float d) { InflateX(d); InflateY(d); }
  void InflateAntiAliasingDistance() {
    Inflate(kAntiAliasingInflateDistance);
  }

  gfx::QuadF ToQuadF() const;

  void ToFloatArray(float flattened[12]) const;

 private:
  Edge left_;
  Edge top_;
  Edge right_;
  Edge bottom_;

  DISALLOW_COPY_AND_ASSIGN(LayerQuad);
};

}

#endif  // CC_LAYER_QUAD_H_
