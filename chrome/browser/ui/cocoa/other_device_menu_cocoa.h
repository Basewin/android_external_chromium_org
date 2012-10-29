// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OTHER_DEVICE_MENU_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_OTHER_DEVICE_MENU_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/search/other_device_menu_controller.h"

namespace gfx{
class Point;
}

namespace ui {
class SimpleMenuModel;
}

class OtherDeviceMenuCocoa : public OtherDeviceMenu {
 public:
  explicit OtherDeviceMenuCocoa(ui::SimpleMenuModel* menu_model);
  virtual ~OtherDeviceMenuCocoa();

  virtual void ShowMenu(
      gfx::NativeWindow window, const gfx::Point& location) OVERRIDE;

 private:
  ui::SimpleMenuModel* menu_model_;  // Owned by the controller.

  DISALLOW_COPY_AND_ASSIGN(OtherDeviceMenuCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_OTHER_DEVICE_MENU_COCOA_H_
