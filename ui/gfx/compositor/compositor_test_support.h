// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_TEST_SUPPORT_H_
#define UI_GFX_COMPOSITOR_TEST_SUPPORT_H_
#pragma once

namespace ui {

class CompositorTestSupport {
 public:
  static void Initialize();
  static void Terminate();
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_TEST_SUPPORT_H_
