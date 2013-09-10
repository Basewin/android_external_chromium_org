// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

class PlatformAppUrlRedirectorBrowserTest : public PlatformAppBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

 protected:
  // Performs the following sequence:
  // - installs the app |handler| (a relative path under the platform_apps
  // subdirectory);
  // - navigates the current tab to the HTML page |lancher_page| (ditto);
  // - then waits for |handler| to launch and send back a |handler_ack_message|;
  // - finally checks that the resulting shell window count is as expected.
  // The |launcher_page| is supposed to trigger a navigation matching one of the
  // url_handlers in the |handler|'s manifest, and thereby launch the |handler|.
  void TestNavigationInTab(const char* launcher_page,
                           const char* handler,
                           const char* handler_start_message);

  // The same as above, but does not expect the |handler| to launch. Verifies
  // that it didn't, and that the navigation has opened in a new tab instead.
  void TestMismatchingNavigationInTab(const char* launcher_page,
                                      const char* success_tab_title,
                                      const char* handler);

  // - installs the app |handler|;
  // - opens the page |xhr_opening_page| in the current tab;
  // - the page sends an XHR to a local resouce, whose URL matches one of the
  //   url_handlers in |handler|;
  // - waits until |xhr_opening_page| gets a response back and changes the tab's
  //   title to a value indicating success/failure of the XHR;
  // - verifies that no shell windows have been opened, i.e. |handler| wasn't
  //   launched even though its url_handlers match the URL.
  void TestNegativeXhrInTab(const char* xhr_opening_page,
                            const char* success_tab_title,
                            const char* failure_tab_title,
                            const char* handler);

  // Performs the following sequence:
  // - installs the app |handler| (a relative path under the platform_apps
  // subdirectory);
  // - loads and launches the app |launcher| (ditto);
  // - waits for the |launcher| to launch and send back a |launcher_ack_message|
  //   (to make sure it's not the failing entity, if the test fails overall);
  // - waits for the |handler| to launch and send back a |handler_ack_message|;
  // - finally checks that the resulting shell window count is as expected.
  // The |launcher| is supposed to trigger a navigation matching one of the
  // url_handlers in the |handler|'s manifest, and thereby launch the |handler|.
  void TestNavigationInApp(const char* launcher,
                           const char* launcher_done_message,
                           const char* handler,
                           const char* handler_start_message);

  // The same as above, but does not expect the |handler| to launch. Verifies
  // that it didn't, and that the navigation has opened in a new tab instead.
  void TestMismatchingNavigationInApp(const char* launcher,
                                      const char* launcher_done_message,
                                      const char* handler);

  // - installs the |handler| app;
  // - loads and launches the |launcher| app;
  // - waits until the |launcher| sends back a |launcher_done_message|;
  // - the launcher performs a navigation to a URL that mismatches the
  //   |handler|'s url_handlers;
  // - verifies that the |handler| hasn't been launched as a result of the
  //   navigation.
  void TestNegativeNavigationInApp(const char* launcher,
                                   const char* launcher_done_message,
                                   const char* handler);

  // - installs the app |handler|;
  // - navigates the current tab to the HTML page |matching_target_page| with
  //   page transition |transition|;
  // - waits for |handler| to launch and send back a |handler_start_message|;
  // - finally checks that the resulting shell window count is as expected.
  void TestNavigationInBrowser(const char* matching_target_page,
                               content::PageTransition transition,
                               const char* handler,
                               const char* handler_start_message);

  // Same as above, but does not expect |handler| to launch. This is used, e.g.
  // for form submissions, where the URL would normally match the url_handlers
  // but should not launch it.
  void TestNegativeNavigationInBrowser(const char* matching_target_page,
                                       content::PageTransition transition,
                                       const char* handler);

  // Same as above, but expects the |mismatching_target_page| should not match
  // any of the |handler|'s url_handlers, and therefor not launch the app.
  void TestMismatchingNavigationInBrowser(const char* mismatching_target_page,
                                          content::PageTransition transition,
                                          const char* handler);
};


void PlatformAppUrlRedirectorBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  PlatformAppBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(::switches::kDisablePopupBlocking);
}

// TODO(sergeygs): Factor out common functionality from TestXyz,
// TestNegativeXyz, and TestMismatchingXyz versions.

// TODO(sergeys): Return testing::AssertionErrors from these methods to
// preserve line numbers and (if applicable) failure messages.

void PlatformAppUrlRedirectorBrowserTest::TestNavigationInTab(
    const char* launcher_page,
    const char* handler,
    const char* handler_start_message) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ExtensionTestMessageListener handler_listener(handler_start_message, false);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
          "/extensions/platform_apps/%s", launcher_page)),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_TRUE(handler_listener.WaitUntilSatisfied());

  ASSERT_EQ(1U, GetShellWindowCount());
}

void PlatformAppUrlRedirectorBrowserTest::TestMismatchingNavigationInTab(
    const char* launcher_page,
    const char* success_tab_title,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  const string16 success_title = ASCIIToUTF16(success_tab_title);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, success_title);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
          "/extensions/platform_apps/%s", launcher_page)),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(success_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0U, GetShellWindowCount());
}

void PlatformAppUrlRedirectorBrowserTest::TestNegativeXhrInTab(
    const char* launcher_page,
    const char* success_tab_title,
    const char* failure_tab_title,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  const string16 success_title = ASCIIToUTF16(success_tab_title);
  const string16 failure_title = ASCIIToUTF16(failure_tab_title);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, success_title);
  title_watcher.AlsoWaitForTitle(failure_title);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
          "/extensions/platform_apps/%s", launcher_page)),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(success_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0U, GetShellWindowCount());
}

void PlatformAppUrlRedirectorBrowserTest::TestNavigationInApp(
    const char* launcher,
    const char* launcher_done_message,
    const char* handler,
    const char* handler_start_message) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ExtensionTestMessageListener launcher_listener(launcher_done_message, false);
  ExtensionTestMessageListener handler_listener(handler_start_message, false);

  LoadAndLaunchPlatformApp(launcher);

  ASSERT_TRUE(launcher_listener.WaitUntilSatisfied());
  ASSERT_TRUE(handler_listener.WaitUntilSatisfied());

  ASSERT_EQ(2U, GetShellWindowCount());
}

void PlatformAppUrlRedirectorBrowserTest::TestNegativeNavigationInApp(
    const char* launcher,
    const char* launcher_done_message,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser()));

  ExtensionTestMessageListener launcher_done_listener(launcher_done_message,
                                                      false);
  LoadAndLaunchPlatformApp(launcher);

  ASSERT_TRUE(launcher_done_listener.WaitUntilSatisfied());

  observer.Wait();

  ASSERT_EQ(1U, GetShellWindowCount());
}

void PlatformAppUrlRedirectorBrowserTest::TestMismatchingNavigationInApp(
    const char* launcher,
    const char* launcher_done_message,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser()));

  ExtensionTestMessageListener launcher_listener(launcher_done_message, false);
  LoadAndLaunchPlatformApp(launcher);

  ASSERT_TRUE(launcher_listener.WaitUntilSatisfied());

  observer.Wait();
  ASSERT_EQ(1U, GetShellWindowCount());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

void PlatformAppUrlRedirectorBrowserTest::TestNavigationInBrowser(
    const char* matching_target_page,
    content::PageTransition transition,
    const char* handler,
    const char* handler_start_message) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ExtensionTestMessageListener handler_listener(handler_start_message, false);

  chrome::NavigateParams params(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
           "/extensions/platform_apps/%s", matching_target_page)),
      transition);
  ui_test_utils::NavigateToURL(&params);

  ASSERT_TRUE(handler_listener.WaitUntilSatisfied());

  ASSERT_EQ(1U, GetShellWindowCount());
}

void PlatformAppUrlRedirectorBrowserTest::TestNegativeNavigationInBrowser(
    const char* matching_target_page,
    content::PageTransition transition,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  chrome::NavigateParams params(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
           "/extensions/platform_apps/%s", matching_target_page)),
      transition);
  ui_test_utils::NavigateToURL(&params);

  ASSERT_EQ(0U, GetShellWindowCount());
}

void PlatformAppUrlRedirectorBrowserTest::TestMismatchingNavigationInBrowser(
    const char* mismatching_target_page,
    content::PageTransition transition,
    const char* handler) {
  TestNegativeNavigationInBrowser(mismatching_target_page, transition, handler);
}

// Test that a click on a regular link in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       ClickInTabIntercepted) {
  TestNavigationInTab(
      "url_handlers/launching_pages/click_link.html",
      "url_handlers/handlers/simple",
      "Handler launched");
}

// Test that a click on a target='_blank' link in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       BlankClickInTabIntercepted) {
  TestNavigationInTab(
      "url_handlers/launching_pages/click_blank_link.html",
      "url_handlers/handlers/simple",
      "Handler launched");
}

// Test that a call to window.open() in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       WindowOpenInTabIntercepted) {
  TestNavigationInTab(
      "url_handlers/launching_pages/call_window_open.html",
      "url_handlers/handlers/simple",
      "Handler launched");
}

// Test that a click on a regular link in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       MismatchingClickInTabNotIntercepted) {
  TestMismatchingNavigationInTab(
      "url_handlers/launching_pages/click_mismatching_link.html",
      "Mismatching link target loaded",
      "url_handlers/handlers/simple");
}

// Test that a click on target='_blank' link in an app's window launches
// another app that has matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       BlankClickInAppIntercepted) {
  TestNavigationInApp(
      "url_handlers/launchers/click_blank_link",
      "Launcher done",
      "url_handlers/handlers/simple",
      "Handler launched");
}

// Test that a call to window.open() in the app's foreground page launches
// another app that has matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       WindowOpenInAppIntercepted) {
  TestNavigationInApp(
      "url_handlers/launchers/call_window_open",
      "Launcher done",
      "url_handlers/handlers/simple",
      "Handler launched");
}

// Test that an app with url_handlers does not intercept a mismatching
// click on a target='_blank' link in another app's window.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       MismatchingWindowOpenInAppNotIntercepted) {
  TestMismatchingNavigationInApp(
      "url_handlers/launchers/call_mismatching_window_open",
      "Launcher done",
      "url_handlers/handlers/simple");
}

// Test that a webview in an app can be navigated to a URL without interception
// even when there are other (or the same) apps that have matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       WebviewNavigationNotIntercepted) {
  // The launcher clicks on a link, which gets intercepted and launches the
  // handler. The handler also redirects an embedded webview to the URL. The
  // webview should just navigate without creating an endless loop of
  // navigate-intercept-launch sequences with multiplying handler's windows.
  // There should be 2 windows only: launcher's and handler's.
  TestNavigationInApp(
      "url_handlers/launchers/click_blank_link",
      "Launcher done",
      "url_handlers/handlers/navigate_webview_to_url",
      "Handler launched");
}

// Test that a webview in an app can be navigated to a URL without interception
// even when there are other (or the same) apps that have matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       MismatchingBlankClickInAppNotIntercepted) {
  // The launcher clicks on a link, which gets intercepted and launches the
  // handler. The handler also redirects an embedded webview to the URL. The
  // webview should just navigate without creating an endless loop of
  // navigate-intercept-launch sequences with multiplying handler's windows.
  // There should be 2 windows only: launcher's and handler's.
  TestMismatchingNavigationInApp(
      "url_handlers/launchers/click_mismatching_blank_link",
      "Launcher done",
      "url_handlers/handlers/simple");
}

// Test that a URL entry in the omnibar launches an app that has matching
// url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       EntryInOmnibarIntercepted) {
  TestNavigationInBrowser(
      "url_handlers/common/target.html",
      content::PAGE_TRANSITION_TYPED,
      "url_handlers/handlers/simple",
      "Handler launched");
}

// Test that an app with url_handlers does not intercept a mismatching
// URL entry in the omnibar.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       MismatchingEntryInOmnibarNotIntercepted) {
  TestMismatchingNavigationInBrowser(
      "url_handlers/common/mismatching_target.html",
      content::PAGE_TRANSITION_TYPED,
      "url_handlers/handlers/simple");
}

// Test that a form submission in a page is never subject to interception
// by apps even with matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       FormSubmissionInTabNotIntercepted) {
  TestNegativeNavigationInBrowser(
      "url_handlers/common/target.html",
      content::PAGE_TRANSITION_FORM_SUBMIT,
      "url_handlers/handlers/simple");
}

// Test that a form submission in a page is never subject to interception
// by apps even with matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppUrlRedirectorBrowserTest,
                       XhrInTabNotIntercepted) {
  TestNegativeXhrInTab(
      "url_handlers/xhr_downloader/main.html",
      "XHR succeeded",
      "XHR failed",
      "url_handlers/handlers/steal_xhr_target");
}

}  // namespace extensions
