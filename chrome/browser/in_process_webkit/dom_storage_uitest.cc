// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_layout_test.h"
#include "chrome/test/ui_test_utils.h"

static const char* kRootFiles[] = {
  "clear.html",
//  "complex-keys.html",  // Output too big for a cookie.  crbug.com/33472
//  "complex-values.html",  // crbug.com/33472
//  "quota.html",  // FIXME(hans): We enforce quota on Session Storage now.
  "remove-item.html",
  "window-attributes-exist.html",
  NULL
};

static const char* kEventsFiles[] = {
//  "basic-body-attribute.html",  // crbug.com/33472
//  "basic.html",  // crbug.com/33472
//  "basic-setattribute.html",  // crbug.com/33472
  "case-sensitive.html",
  "documentURI.html",
  NULL
};

static const char* kStorageFiles[] = {
  "delete-removal.html",
  "enumerate-storage.html",
  "enumerate-with-length-and-key.html",
  "index-get-and-set.html",
  "simple-usage.html",
  "string-conversion.html",
//  "window-open.html", // TODO(jorlow): Fix
  NULL
};

class DOMStorageTest : public UILayoutTest {
 protected:
  DOMStorageTest()
      : UILayoutTest(),
        test_dir_(FilePath().
                  AppendASCII("storage").AppendASCII("domstorage")) {
  }

  virtual ~DOMStorageTest() { }

  virtual void SetUp() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
    UILayoutTest::SetUp();
  }

  // We require fast/js/resources for most of the DOM Storage layout tests.
  // Add those to the list to be copied.
  void AddJSTestResources() {
    // Add other paths our tests require.
    FilePath js_dir = FilePath().
                      AppendASCII("fast").AppendASCII("js");
    AddResourceForLayoutTest(js_dir, FilePath().AppendASCII("resources"));
  }

  // This is somewhat of a hack because we're running a real browser that
  // actually persists the LocalStorage state vs. DRT and TestShell which don't.
  // The correct fix is to fix the LayoutTests, but similar patches have been
  // rejected in the past.
  void ClearDOMStorage() {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    const FilePath dir(FILE_PATH_LITERAL("layout_tests"));
    const FilePath file(FILE_PATH_LITERAL("clear_dom_storage.html"));
    GURL url = ui_test_utils::GetTestUrl(dir, file);
    ASSERT_TRUE(tab->SetCookie(url, ""));
    ASSERT_TRUE(tab->NavigateToURL(url));

    WaitUntilCookieNonEmpty(tab.get(), url, "cleared", action_max_timeout_ms());
  }

  // Runs each test in an array of strings until it hits a NULL.
  void RunTests(const char** files) {
    while (*files) {
      ClearDOMStorage();
      RunLayoutTest(*files, kNoHttpPort);
      ++files;
    }
  }

  FilePath test_dir_;
};


TEST_F(DOMStorageTest, RootLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath(), kNoHttpPort);
  AddJSTestResources();
  AddResourceForLayoutTest(test_dir_, FilePath().AppendASCII("script-tests"));
  RunTests(kRootFiles);
}

TEST_F(DOMStorageTest, EventLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("events"),
                          kNoHttpPort);
  AddJSTestResources();
  AddResourceForLayoutTest(test_dir_, FilePath().AppendASCII("events").
                                      AppendASCII("resources"));
  AddResourceForLayoutTest(test_dir_, FilePath().AppendASCII("events").
                                      AppendASCII("script-tests"));
  RunTests(kEventsFiles);
}

TEST_F(DOMStorageTest, LocalStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("localstorage"),
                          kNoHttpPort);
  AddJSTestResources();
  AddResourceForLayoutTest(test_dir_, FilePath().AppendASCII("localstorage").
                                      AppendASCII("resources"));
  RunTests(kStorageFiles);
}

TEST_F(DOMStorageTest, SessionStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("sessionstorage"),
                          kNoHttpPort);
  AddJSTestResources();
  AddResourceForLayoutTest(test_dir_, FilePath().AppendASCII("sessionstorage").
                                      AppendASCII("resources"));
  RunTests(kStorageFiles);
}
