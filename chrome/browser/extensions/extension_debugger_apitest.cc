// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// TODO(pfeldman): uncomment after the WebKit roll.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Debugger) {
  ASSERT_TRUE(RunExtensionTest("debugger")) << message_;
}
