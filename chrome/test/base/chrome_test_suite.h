// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
#define CHROME_TEST_BASE_CHROME_TEST_SUITE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/test/content_test_suite_base.h"

namespace base {
class StatsTable;
}

// Test suite for unit and browser tests. Creates services needed by both.
// See also ChromeUnitTestSuite for additional services created for unit tests.
class ChromeTestSuite : public content::ContentTestSuiteBase {
 public:
  ChromeTestSuite(int argc, char** argv);
  virtual ~ChromeTestSuite();

 protected:
  // base::TestSuite overrides:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  virtual content::ContentClient* CreateClientForInitialization() OVERRIDE;

  // Should return true if the test suite will be running the full chrome
  // browser, false if the test suite is unit tests.
  virtual bool IsBrowserTestSuite() = 0;

  void SetBrowserDirectory(const base::FilePath& browser_dir) {
    browser_dir_ = browser_dir;
  }

  // Alternative path to browser binaries.
  base::FilePath browser_dir_;

  std::string stats_filename_;
  scoped_ptr<base::StatsTable> stats_table_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTestSuite);
};

#endif  // CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
