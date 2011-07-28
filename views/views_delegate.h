// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_VIEWS_DELEGATE_H_
#define VIEWS_VIEWS_DELEGATE_H_
#pragma once

#include <string>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include "ui/base/accessibility/accessibility_types.h"
#include "views/views_api.h"

namespace gfx {
class Rect;
}

namespace ui {
class Clipboard;
}

namespace views {

class View;
class Widget;

// ViewsDelegate is an interface implemented by an object using the views
// framework. It is used to obtain various high level application utilities
// and perform some actions such as window placement saving.
//
// The embedding app must set views_delegate to assign its ViewsDelegate
// implementation.
class VIEWS_API ViewsDelegate {
 public:
  virtual ~ViewsDelegate() {}

  // Gets the clipboard.
  virtual ui::Clipboard* GetClipboard() const = 0;

  // Returns the View that all synthetic widgets created without a specified
  // parent will be parented to if they do not specify a parent in their
  // InitParams, or NULL if they should have no parent.
  // TODO(beng): perhaps this should be a Widget.
  virtual View* GetDefaultParentView() = 0;

  // Saves the position, size and maximized state for the window with the
  // specified name.
  virtual void SaveWindowPlacement(const Widget* widget,
                                   const std::wstring& window_name,
                                   const gfx::Rect& bounds,
                                   bool maximized) = 0;

  // Retrieves the saved position and size for the window with the specified
  // name.
  virtual bool GetSavedWindowBounds(const std::wstring& window_name,
                                    gfx::Rect* bounds) const = 0;

  // Retrieves the saved maximized state for the window with the specified
  // name.
  virtual bool GetSavedMaximizedState(const std::wstring& window_name,
                                      bool* maximized) const = 0;

  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AccessibilityTypes::Event event_type) = 0;

  // For accessibility, notify the delegate that a menu item was focused
  // so that alternate feedback (speech / magnified text) can be provided.
  virtual void NotifyMenuItemFocused(
      const std::wstring& menu_name,
      const std::wstring& menu_item_name,
      int item_index,
      int item_count,
      bool has_submenu) = 0;

#if defined(OS_WIN)
  // Retrieves the default window icon to use for windows if none is specified.
  virtual HICON GetDefaultWindowIcon() const = 0;
#endif

  // AddRef/ReleaseRef are invoked while a menu is visible. They are used to
  // ensure we don't attempt to exit while a menu is showing.
  virtual void AddRef() = 0;
  virtual void ReleaseRef() = 0;

  // Converts views::Event::flags to a WindowOpenDisposition.
  virtual int GetDispositionForEvent(int event_flags) = 0;

  // The active ViewsDelegate used by the views system.
  static ViewsDelegate* views_delegate;
};

}  // namespace views

#endif  // VIEWS_VIEWS_DELEGATE_H_
