// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"

class ExtensionHost;
class RenderViewHost;
class RenderWidgetHostViewGtk;

class ExtensionViewGtk {
 public:
  explicit ExtensionViewGtk(ExtensionHost* extension_host);

  gfx::NativeView native_view();

  bool is_toolstrip() const { return is_toolstrip_; }
  void set_is_toolstrip(bool is_toolstrip) { is_toolstrip_ = is_toolstrip; }

 private:
  RenderViewHost* render_view_host() const;

  void CreateWidgetHostView();

  // True if the contents are being displayed inside the extension shelf.
  bool is_toolstrip_;

  ExtensionHost* extension_host_;

  RenderWidgetHostViewGtk* render_widget_host_view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewGtk);
};

#endif  // CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_
