// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/infobar_gradient_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class InfoBarGradientViewTest : public PlatformTest {
 public:
  InfoBarGradientViewTest() {
    NSRect frame = NSMakeRect(0, 0, 100, 30);
    view_.reset([[InfoBarGradientView alloc] initWithFrame:frame]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<InfoBarGradientView> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(InfoBarGradientViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(InfoBarGradientViewTest, Display) {
  [view_ display];
}

// Assert that the view is non-opaque, because otherwise we will end
// up with findbar painting issues.
TEST_F(InfoBarGradientViewTest, AssertViewNonOpaque) {
  EXPECT_FALSE([view_ isOpaque]);
}

}  // namespace
