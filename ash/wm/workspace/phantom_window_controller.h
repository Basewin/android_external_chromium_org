// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// PhantomWindowController is responsible for showing a phantom representation
// of a window. It's used to show a preview of how snapping or docking a window
// will affect the window's bounds.
class ASH_EXPORT PhantomWindowController {
 public:
  explicit PhantomWindowController(aura::Window* window);

  // Hides the phantom window without any animation.
  virtual ~PhantomWindowController();

  // Animates the phantom window towards |bounds_in_screen|.
  // Creates two (if start bounds intersect any root window other than the
  // root window that matches the target bounds) or one (otherwise) phantom
  // widgets to display animated rectangle in each root.
  // This does not immediately show the window.
  void Show(const gfx::Rect& bounds_in_screen);

  // If set, the phantom window is stacked below this window, otherwise it
  // is stacked above the window passed to the constructor.
  void set_phantom_below_window(aura::Window* phantom_below_window) {
    phantom_below_window_ = phantom_below_window;
  }

 private:
  friend class PhantomWindowControllerTest;

  // Creates, shows and returns a phantom widget at |bounds|
  // with kShellWindowId_ShelfContainer in |root_window| as a parent.
  scoped_ptr<views::Widget> CreatePhantomWidget(
      aura::Window* root_window,
      const gfx::Rect& bounds_in_screen);

  // Window the phantom is placed beneath.
  aura::Window* window_;

  // If set, the phantom window should get stacked below this window.
  aura::Window* phantom_below_window_;

  // Target bounds of the animation in screen coordinates.
  gfx::Rect target_bounds_in_screen_;

  // Phantom representation of the window which is in the root window matching
  // |target_bounds_in_screen_|.
  scoped_ptr<views::Widget> phantom_widget_in_target_root_;

  // Phantom representation of the window which is in the root window matching
  // the window's initial bounds. This allows animations to progress from one
  // display to the other. NULL if the phantom window starts and ends in the
  // same root window.
  scoped_ptr<views::Widget> phantom_widget_in_start_root_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
