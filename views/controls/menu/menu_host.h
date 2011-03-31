// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace views {

class NativeMenuHost;
class SubmenuView;
class View;

// SubmenuView uses a MenuHost to house the SubmenuView. MenuHost typically
// extends the native Widget type, but is defined here for clarity of what
// methods SubmenuView uses.
//
// SubmenuView owns the MenuHost. When SubmenuView is done with the MenuHost
// |DestroyMenuHost| is invoked. The one exception to this is if the native
// OS destroys the widget out from under us, in which case |MenuHostDestroyed|
// is invoked back on the SubmenuView and the SubmenuView then drops references
// to the MenuHost.
class MenuHost {
 public:
  explicit MenuHost(SubmenuView* submenu);
  virtual ~MenuHost();

  // Initializes and shows the MenuHost.
  void InitMenuHost(gfx::NativeWindow parent,
                    const gfx::Rect& bounds,
                    View* contents_view,
                    bool do_capture);

  // Returns true if the menu host is visible.
  bool IsMenuHostVisible();

  // Shows the menu host. If |do_capture| is true the menu host should do a
  // mouse grab.
  void ShowMenuHost(bool do_capture);

  // Hides the menu host.
  void HideMenuHost();

  // Destroys and deletes the menu host.
  void DestroyMenuHost();

  // Sets the bounds of the menu host.
  void SetMenuHostBounds(const gfx::Rect& bounds);

  // Releases a mouse grab installed by |ShowMenuHost|.
  void ReleaseMenuHostCapture();

  // Returns the native window of the MenuHost.
  gfx::NativeWindow GetMenuHostWindow();

 private:
  NativeMenuHost* native_menu_host_;

  DISALLOW_COPY_AND_ASSIGN(MenuHost);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_H_
