// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_FRAME_MAXIMIZE_BUTTON_H_
#define ASH_WM_WORKSPACE_FRAME_MAXIMIZE_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

namespace internal {
class PhantomWindowController;
}

// Button used for the maximize control on the frame. Handles snapping logic.
class ASH_EXPORT FrameMaximizeButton : public views::ImageButton {
 public:
  explicit FrameMaximizeButton(views::ButtonListener* listener);
  virtual ~FrameMaximizeButton();

  // ImageButton overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

 protected:
  // ImageButton overrides:
  virtual SkBitmap GetImageToPaint() OVERRIDE;

 private:
  // Where to snap to.
  enum SnapType {
    SNAP_LEFT,
    SNAP_RIGHT,
    SNAP_MAXIMIZE,
    SNAP_MINIMIZE,
    SNAP_NONE
  };

  // Updates |snap_type_| based on a mouse drag. The parameters are relative to
  // the mouse pressed location.
  void UpdateSnap(int delta_x, int delta_y);

  // Returns the type of snap based on the specified coordaintes (relative to
  // the press location).
  SnapType SnapTypeForDelta(int delta_x, int delta_y) const;

  // Returns the bounds of the resulting window for the specified type.
  gfx::Rect BoundsForType(SnapType type) const;

  // Snaps the window to the current snap position.
  void Snap();

  // Renders the snap position.
  scoped_ptr<internal::PhantomWindowController> phantom_window_;

  // Is snapping enabled? Set on press so that in drag we know whether we
  // should show the snap locations.
  bool is_snap_enabled_;

  // Did the user drag far enough to trigger snapping?
  bool exceeded_drag_threshold_;

  // Location of the press.
  gfx::Point press_location_;

  // Current snap type.
  SnapType snap_type_;

  DISALLOW_COPY_AND_ASSIGN(FrameMaximizeButton);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_FRAME_MAXIMIZE_BUTTON_H_
