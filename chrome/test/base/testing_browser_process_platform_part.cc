// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process_platform_part.h"

TestingBrowserProcessPlatformPart::TestingBrowserProcessPlatformPart() {
}

TestingBrowserProcessPlatformPart::~TestingBrowserProcessPlatformPart() {
}

#if defined(OS_CHROMEOS)
chromeos::OomPriorityManager*
    TestingBrowserProcessPlatformPart::oom_priority_manager() {
  return NULL;
}
#endif
