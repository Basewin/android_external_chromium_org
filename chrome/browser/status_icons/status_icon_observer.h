// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_OBSERVER_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_OBSERVER_H_
#pragma once

class StatusIconObserver {
 public:
  // Called when the user clicks on the system tray icon. Clicks that result
  // in the context menu being displayed will not be passed to this observer
  // (i.e. if there's a context menu set on this status icon, and the user
  // right clicks on the icon to display the context menu, OnStatusIconClicked()
  // will not be called).
  // Note: Chrome OS displays the context menu on left button clicks.
  // This will only be fired for this platform if no context menu is present.
  virtual void OnStatusIconClicked() = 0;

 protected:
  virtual ~StatusIconObserver() {}
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_OBSERVER_H_
