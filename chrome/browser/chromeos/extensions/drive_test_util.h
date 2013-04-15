// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DRIVE_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DRIVE_TEST_UTIL_H_

class Profile;

namespace drive_test_util {

// Waits until Drive mount point for |profile| is added. Drive mount point is
// added by the browser but tests should use this function to ensure that the
// Drive mount point is added before accessing Drive.
void WaitUntilDriveMountPointIsAdded(Profile* profile);

}  // namespace drive_test_util

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DRIVE_TEST_UTIL_H_
