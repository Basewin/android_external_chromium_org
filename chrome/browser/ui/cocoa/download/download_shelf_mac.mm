// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/download/download_shelf_mac.h"

#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#include "chrome/browser/ui/cocoa/download/download_item_mac.h"

DownloadShelfMac::DownloadShelfMac(Browser* browser,
                                   DownloadShelfController* controller)
    : browser_(browser),
      shelf_controller_(controller) {
}

void DownloadShelfMac::DoAddDownload(content::DownloadItem* download) {
  [shelf_controller_ addDownloadItem:download];
}

bool DownloadShelfMac::IsShowing() const {
  return [shelf_controller_ isVisible] == YES;
}

bool DownloadShelfMac::IsClosing() const {
  // TODO(estade): This is never called. For now just return false.
  return false;
}

void DownloadShelfMac::DoShow() {
  [shelf_controller_ showDownloadShelf:YES
                          isUserAction:NO];
  browser_->UpdateDownloadShelfVisibility(true);
}

void DownloadShelfMac::DoClose(CloseReason reason) {
  [shelf_controller_ showDownloadShelf:NO
                          isUserAction:reason == USER_ACTION];
  browser_->UpdateDownloadShelfVisibility(false);
}

Browser* DownloadShelfMac::browser() const {
  return browser_;
}
