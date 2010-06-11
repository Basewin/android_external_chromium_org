// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_dialog_cloud_internal.h"

#include <functional>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"

namespace {

class TestData {
 public:
  TestData() {}

  char* GetTestData() {
    if (test_data_.empty()) {
      FilePath test_data_directory;
      PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
      FilePath test_file =
          test_data_directory.AppendASCII("printing/cloud_print_uitest.html");
      file_util::ReadFileToString(test_file, &test_data_);
    }
    return &test_data_[0];
  }
 private:
  std::string test_data_;
};

// A simple test URLRequestJob. We don't care what it does, only that
// whether it starts and finishes.
class SimpleTestJob : public URLRequestTestJob {
 public:
  explicit SimpleTestJob(URLRequest* request)
      : URLRequestTestJob(request, test_headers(),
                          Singleton<TestData>()->GetTestData(), true) {}

  virtual void GetResponseInfo(net::HttpResponseInfo* info) {
    URLRequestTestJob::GetResponseInfo(info);
    if (request_->url().SchemeIsSecure()) {
      // Make up a fake certificate for this response since we don't have
      // access to the real SSL info.
      const char* kCertIssuer = "Chrome Internal";
      const int kLifetimeDays = 100;

      info->ssl_info.cert =
          new net::X509Certificate(request_->url().GetWithEmptyPath().spec(),
                                   kCertIssuer,
                                   base::Time::Now(),
                                   base::Time::Now() +
                                   base::TimeDelta::FromDays(kLifetimeDays));
      info->ssl_info.cert_status = 0;
      info->ssl_info.security_bits = 0;
    }
  }

 private:
  ~SimpleTestJob() {}
};

class TestResult {
 public:
  TestResult() : result_(false) {}
  void SetResult(bool value) {
    result_ = value;
  }
  bool GetResult() {
    return result_;
  }
  void SetExpectedUrl(const GURL& url) {
    expected_url_ = url;
  }
  const GURL GetExpectedUrl() {
    return expected_url_;
  }
  void set_delegate(TestDelegate* delegate) {
    delegate_ = delegate;
  }
  TestDelegate* delegate() {
    return delegate_;
  }
 private:
  bool result_;
  GURL expected_url_;
  TestDelegate* delegate_;
};

}  // namespace

class PrintDialogCloudTest : public InProcessBrowserTest {
 public:
  PrintDialogCloudTest() : handler_added_(false) {
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
  }

  // Must be static for handing into AddHostnameHandler.
  static URLRequest::ProtocolFactory Factory;

  class PrintDialogCloudTestDelegate : public TestDelegate {
   public:
    PrintDialogCloudTestDelegate() {}

    virtual void OnResponseCompleted(URLRequest* request) {
      ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                             new MessageLoop::QuitTask());
    }
  };

  virtual void SetUp() {
    Singleton<TestResult>()->SetResult(false);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() {
    if (handler_added_) {
      URLRequestFilter* filter = URLRequestFilter::GetInstance();
      filter->RemoveHostnameHandler(scheme_, host_name_);
      handler_added_ = false;
      Singleton<TestResult>()->set_delegate(NULL);
    }
    InProcessBrowserTest::TearDown();
  }

  // Normally this is something I would expect could go into SetUp(),
  // but there seems to be some timing or ordering related issue with
  // the test harness that made that flaky.  Calling this from the
  // individual test functions seems to fix that.
  void AddTestHandlers() {
    if (!handler_added_) {
      URLRequestFilter* filter = URLRequestFilter::GetInstance();
      GURL cloud_print_service_url =
          internal_cloud_print_helpers::CloudPrintService(browser()->profile()).
          GetCloudPrintServiceURL();
      scheme_ = cloud_print_service_url.scheme();
      host_name_ = cloud_print_service_url.host();
      filter->AddHostnameHandler(scheme_, host_name_,
                                 &PrintDialogCloudTest::Factory);
      handler_added_ = true;

      GURL cloud_print_dialog_url =
          internal_cloud_print_helpers::CloudPrintService(browser()->profile()).
          GetCloudPrintServiceDialogURL();
      Singleton<TestResult>()->SetExpectedUrl(cloud_print_dialog_url);
      Singleton<TestResult>()->set_delegate(&delegate_);
    }
  }

  bool handler_added_;
  std::string scheme_;
  std::string host_name_;
  FilePath test_data_directory_;
  PrintDialogCloudTestDelegate delegate_;
};

URLRequestJob* PrintDialogCloudTest::Factory(URLRequest* request,
                                             const std::string& scheme) {
  request->set_delegate(Singleton<TestResult>()->delegate());
  if (request && (request->url() == Singleton<TestResult>()->GetExpectedUrl()))
    Singleton<TestResult>()->SetResult(true);
  return new SimpleTestJob(request);
}

IN_PROC_BROWSER_TEST_F(PrintDialogCloudTest, HandlersRegistered) {
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  AddTestHandlers();

  FilePath pdf_file =
      test_data_directory_.AppendASCII("printing/cloud_print_uitest.pdf");

  {
  PrintDialogCloud dialog (pdf_file);
  ui_test_utils::RunMessageLoop();
  }

  ASSERT_TRUE(Singleton<TestResult>()->GetResult());
}

#if defined(OS_CHROMEOS)
// Disabled until the extern URL is live so that the Print menu item
// can be enabled for Chromium OS.
IN_PROC_BROWSER_TEST_F(PrintDialogCloudTest, DISABLED_DialogGrabbed) {
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  AddTestHandlers();

  // This goes back one step further for the Chrome OS case, to making
  // sure 'window.print()' gets to the right place.
  ASSERT_TRUE(browser()->GetSelectedTabContents());
  ASSERT_TRUE(browser()->GetSelectedTabContents()->render_view_host());

  std::wstring window_print(L"window.print()");
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(std::wstring(), window_print);

  ui_test_utils::RunMessageLoop();

  ASSERT_TRUE(Singleton<TestResult>()->GetResult());
}
#endif
