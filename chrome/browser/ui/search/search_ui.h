// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_

namespace chrome {
namespace search {

// The mininum height of content view to layout detached bookmark bar at bottom
// for |NTP| search mode, calculated from chrome/browser/resources/ntp_search/
// tile_page.js HEIGHT_FOR_BOTTOM_PANEL - TAB_BAR_HEIGHT - UPPER_SECTION_HEIGHT.
// TODO(kuan): change this when tile_page.js changes to use non-const
// UPPER_SECTION_HEIGHT,
extern const int kMinContentHeightForBottomBookmarkBar;

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_UI_H_
