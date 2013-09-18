// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_screenshot_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace content {

// This class keeps track of the RenderViewHost whose screenshot was captured.
class ScreenshotTracker : public WebContentsScreenshotManager {
 public:
  explicit ScreenshotTracker(NavigationControllerImpl* controller)
      : WebContentsScreenshotManager(controller),
        screenshot_taken_for_(NULL),
        waiting_for_screenshots_(0) {
  }

  virtual ~ScreenshotTracker() {
  }

  RenderViewHost* screenshot_taken_for() { return screenshot_taken_for_; }

  void Reset() {
    screenshot_taken_for_ = NULL;
    screenshot_set_.clear();
  }

  void SetScreenshotInterval(int interval_ms) {
    SetMinScreenshotIntervalMS(interval_ms);
  }

  void WaitUntilScreenshotIsReady() {
    if (!waiting_for_screenshots_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  bool ScreenshotSetForEntry(NavigationEntryImpl* entry) const {
    return screenshot_set_.count(entry) > 0;
  }

 private:
  // Overridden from WebContentsScreenshotManager:
  virtual void TakeScreenshotImpl(RenderViewHost* host,
                                  NavigationEntryImpl* entry) OVERRIDE {
    ++waiting_for_screenshots_;
    screenshot_taken_for_ = host;
    WebContentsScreenshotManager::TakeScreenshotImpl(host, entry);
  }

  virtual void OnScreenshotSet(NavigationEntryImpl* entry) OVERRIDE {
    --waiting_for_screenshots_;
    screenshot_set_[entry] = true;
    WebContentsScreenshotManager::OnScreenshotSet(entry);
    if (waiting_for_screenshots_ == 0 && message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  RenderViewHost* screenshot_taken_for_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  int waiting_for_screenshots_;
  std::map<NavigationEntryImpl*, bool> screenshot_set_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTracker);
};

class NavigationWatcher : public WebContentsObserver {
 public:
  explicit NavigationWatcher(WebContents* contents)
      : WebContentsObserver(contents),
        navigated_(false),
        should_quit_loop_(false) {
  }

  virtual ~NavigationWatcher() {}

  void WaitUntilNavigationStarts() {
    if (navigated_)
      return;
    should_quit_loop_ = true;
    base::MessageLoop::current()->Run();
  }

 private:
  // Overridden from WebContentsObserver:
  virtual void AboutToNavigateRenderView(RenderViewHost* host) OVERRIDE {
    navigated_ = true;
    if (should_quit_loop_)
      base::MessageLoop::current()->Quit();
  }

  bool navigated_;
  bool should_quit_loop_;

  DISALLOW_COPY_AND_ASSIGN(NavigationWatcher);
};

class WebContentsViewAuraTest : public ContentBrowserTest {
 public:
  WebContentsViewAuraTest()
      : screenshot_manager_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    // TODO(jbauman): Remove this. http://crbug.com/268644
    UseRealGLContexts();
    ContentBrowserTest::SetUp();
  }

  // Executes the javascript synchronously and makes sure the returned value is
  // freed properly.
  void ExecuteSyncJSFunction(RenderViewHost* rvh, const std::string& jscript) {
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(rvh, jscript);
  }

  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(test_server()->Start());
    GURL test_url(test_server()->GetURL(url));
    NavigateToURL(shell(), test_url);
    aura::Window* content =
        shell()->web_contents()->GetView()->GetContentNativeView();
    content->GetRootWindow()->SetHostSize(gfx::Size(800, 600));

    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    NavigationControllerImpl* controller = &web_contents->GetController();

    screenshot_manager_ = new ScreenshotTracker(controller);
    controller->SetScreenshotManager(screenshot_manager_);
  }

  void TestOverscrollNavigation(bool touch_handler) {
    ASSERT_NO_FATAL_FAILURE(
        StartTestWithPage("files/overscroll_navigation.html"));
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    NavigationController& controller = web_contents->GetController();
    RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
        web_contents->GetRenderViewHost());
    WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
        web_contents->GetView());
    view_aura->SetupOverlayWindowForTesting();

    EXPECT_FALSE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());
    int index = -1;
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(view_host, "get_current()");
    ASSERT_TRUE(value->GetAsInteger(&index));
    EXPECT_EQ(0, index);

    if (touch_handler)
      ExecuteSyncJSFunction(view_host, "install_touch_handler()");

    ExecuteSyncJSFunction(view_host, "navigate_next()");
    ExecuteSyncJSFunction(view_host, "navigate_next()");
    value = content::ExecuteScriptAndGetValue(view_host, "get_current()");
    ASSERT_TRUE(value->GetAsInteger(&index));
    EXPECT_EQ(2, index);
    EXPECT_TRUE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());

    aura::Window* content = web_contents->GetView()->GetContentNativeView();
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    aura::test::EventGenerator generator(content->GetRootWindow(), content);
    const int kScrollDurationMs = 20;
    const int kScrollSteps = 10;

    {
      // Do a swipe-right now. That should navigate backwards.
      string16 expected_title = ASCIIToUTF16("Title: #1");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(kScrollDurationMs),
          kScrollSteps);
      string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value = content::ExecuteScriptAndGetValue(view_host, "get_current()");
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(1, index);
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a fling-right now. That should navigate backwards.
      string16 expected_title = ASCIIToUTF16("Title:");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(kScrollDurationMs),
          kScrollSteps);
      string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value = content::ExecuteScriptAndGetValue(view_host, "get_current()");
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(0, index);
      EXPECT_FALSE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a swipe-left now. That should navigate forward.
      string16 expected_title = ASCIIToUTF16("Title: #1");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(kScrollDurationMs),
          kScrollSteps);
      string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value = content::ExecuteScriptAndGetValue(view_host, "get_current()");
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(1, index);
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }
  }

  int GetCurrentIndex() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
        web_contents->GetRenderViewHost());
    int index = -1;
    scoped_ptr<base::Value> value;
    value = content::ExecuteScriptAndGetValue(view_host, "get_current()");
    if (!value->GetAsInteger(&index))
      index = -1;
    return index;
  }

 protected:
  ScreenshotTracker* screenshot_manager() { return screenshot_manager_; }
  void set_min_screenshot_interval(int interval_ms) {
    screenshot_manager_->SetScreenshotInterval(interval_ms);
  }

 private:
  ScreenshotTracker* screenshot_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAuraTest);
};

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       OverscrollNavigation) {
  TestOverscrollNavigation(false);
}

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       OverscrollNavigationWithTouchHandler) {
  TestOverscrollNavigation(true);
}

// Disabled because the test always fails the first time it runs on the Win Aura
// bots, and usually but not always passes second-try (See crbug.com/179532).
#if defined(OS_WIN)
#define MAYBE_QuickOverscrollDirectionChange \
        DISABLED_QuickOverscrollDirectionChange
#else
#define MAYBE_QuickOverscrollDirectionChange QuickOverscrollDirectionChange
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_QuickOverscrollDirectionChange) {
  ASSERT_NO_FATAL_FAILURE(
      StartTestWithPage("files/overscroll_navigation.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
      web_contents->GetRenderViewHost());

  // This test triggers a large number of animations. Speed them up to ensure
  // the test completes within its time limit.
  ui::ScopedAnimationDurationScaleMode fast_duration_mode(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Make sure the page has both back/forward history.
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(2, GetCurrentIndex());
  web_contents->GetController().GoBack();
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetView()->GetContentNativeView();
  aura::RootWindow* root_window = content->GetRootWindow();
  gfx::Rect bounds = content->GetBoundsInRootWindow();

  base::TimeDelta timestamp;
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED,
      gfx::Point(bounds.x() + bounds.width() / 2, bounds.y() + 5),
      0, timestamp);
  root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);
  EXPECT_EQ(1, GetCurrentIndex());

  timestamp += base::TimeDelta::FromMilliseconds(10);
  ui::TouchEvent move1(ui::ET_TOUCH_MOVED,
      gfx::Point(bounds.right() - 10, bounds.y() + 5),
      0, timestamp);
  root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&move1);
  EXPECT_EQ(1, GetCurrentIndex());

  // Swipe back from the right edge, back to the left edge, back to the right
  // edge.

  for (int x = bounds.right() - 10; x >= bounds.x() + 10; x-= 10) {
    timestamp += base::TimeDelta::FromMilliseconds(10);
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED,
        gfx::Point(x, bounds.y() + 5),
        0, timestamp);
    root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&inc);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  for (int x = bounds.x() + 10; x <= bounds.width() - 10; x+= 10) {
    timestamp += base::TimeDelta::FromMilliseconds(10);
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED,
        gfx::Point(x, bounds.y() + 5),
        0, timestamp);
    root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&inc);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  for (int x = bounds.width() - 10; x >= bounds.x() + 10; x-= 10) {
    timestamp += base::TimeDelta::FromMilliseconds(10);
    ui::TouchEvent inc(ui::ET_TOUCH_MOVED,
        gfx::Point(x, bounds.y() + 5),
        0, timestamp);
    root_window->AsRootWindowHostDelegate()->OnHostTouchEvent(&inc);
    EXPECT_EQ(1, GetCurrentIndex());
  }

  // Do not end the overscroll sequence.
}

// Disable the test for windows as it keeps failing in XP bot, see
// http://crbug/294116.
#if defined(OS_WIN)
#define MAYBE_OverscrollScreenshot DISABLED_OverscrollScreenshot
#else
#define MAYBE_OverscrollScreenshot OverscrollScreenshot
#endif
// Tests that the page has has a screenshot when navigation happens:
//  - from within the page (from a JS function)
//  - interactively, when user does an overscroll gesture
//  - interactively, when user navigates in history without the overscroll
//    gesture.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_OverscrollScreenshot) {
  ASSERT_NO_FATAL_FAILURE(
      StartTestWithPage("files/overscroll_navigation.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
      web_contents->GetRenderViewHost());

  set_min_screenshot_interval(0);

  // Do a few navigations initiated by the page.
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(2, GetCurrentIndex());
  screenshot_manager()->WaitUntilScreenshotIsReady();

  // The current entry won't have any screenshots. But the entries in the
  // history should now have screenshots.
  NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
      web_contents->GetController().GetEntryAtIndex(2));
  EXPECT_FALSE(entry->screenshot().get());

  entry = NavigationEntryImpl::FromNavigationEntry(
      web_contents->GetController().GetEntryAtIndex(1));
  EXPECT_TRUE(screenshot_manager()->ScreenshotSetForEntry(entry));

  entry = NavigationEntryImpl::FromNavigationEntry(
      web_contents->GetController().GetEntryAtIndex(0));
  EXPECT_TRUE(screenshot_manager()->ScreenshotSetForEntry(entry));

  // Navigate again. Index 2 should now have a screenshot.
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(3, GetCurrentIndex());
  screenshot_manager()->WaitUntilScreenshotIsReady();

  entry = NavigationEntryImpl::FromNavigationEntry(
      web_contents->GetController().GetEntryAtIndex(2));
  EXPECT_TRUE(screenshot_manager()->ScreenshotSetForEntry(entry));

  entry = NavigationEntryImpl::FromNavigationEntry(
      web_contents->GetController().GetEntryAtIndex(3));
  EXPECT_FALSE(entry->screenshot().get());

  {
    // Now, swipe right to navigate backwards. This should navigate away from
    // index 3 to index 2, and index 3 should have a screenshot.
    string16 expected_title = ASCIIToUTF16("Title: #2");
    content::TitleWatcher title_watcher(web_contents, expected_title);
    aura::Window* content = web_contents->GetView()->GetContentNativeView();
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    aura::test::EventGenerator generator(content->GetRootWindow(), content);
    generator.GestureScrollSequence(
        gfx::Point(bounds.x() + 2, bounds.y() + 10),
        gfx::Point(bounds.right() - 10, bounds.y() + 10),
        base::TimeDelta::FromMilliseconds(20),
        1);
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
    EXPECT_EQ(2, GetCurrentIndex());
    screenshot_manager()->WaitUntilScreenshotIsReady();
    entry = NavigationEntryImpl::FromNavigationEntry(
        web_contents->GetController().GetEntryAtIndex(3));
    EXPECT_TRUE(screenshot_manager()->ScreenshotSetForEntry(entry));
  }

  // Navigate a couple more times.
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(3, GetCurrentIndex());
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(4, GetCurrentIndex());
  screenshot_manager()->WaitUntilScreenshotIsReady();
  entry = NavigationEntryImpl::FromNavigationEntry(
      web_contents->GetController().GetEntryAtIndex(4));
  EXPECT_FALSE(entry->screenshot().get());

  {
    // Navigate back in history.
    string16 expected_title = ASCIIToUTF16("Title: #3");
    content::TitleWatcher title_watcher(web_contents, expected_title);
    web_contents->GetController().GoBack();
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
    EXPECT_EQ(3, GetCurrentIndex());
    screenshot_manager()->WaitUntilScreenshotIsReady();
    entry = NavigationEntryImpl::FromNavigationEntry(
        web_contents->GetController().GetEntryAtIndex(4));
    EXPECT_TRUE(screenshot_manager()->ScreenshotSetForEntry(entry));
  }
}

// Tests that screenshot is taken correctly when navigation causes a
// RenderViewHost to be swapped out.
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       ScreenshotForSwappedOutRenderViews) {
  ASSERT_NO_FATAL_FAILURE(
      StartTestWithPage("files/overscroll_navigation.html"));
  // Create a new server with a different site.
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  set_min_screenshot_interval(0);

  struct {
    GURL url;
    int transition;
  } navigations[] = {
    { https_server.GetURL("files/title1.html"),
      PAGE_TRANSITION_TYPED | PAGE_TRANSITION_FROM_ADDRESS_BAR },
    { test_server()->GetURL("files/title2.html"),
      PAGE_TRANSITION_AUTO_BOOKMARK },
    { https_server.GetURL("files/title3.html"),
      PAGE_TRANSITION_TYPED | PAGE_TRANSITION_FROM_ADDRESS_BAR },
    { GURL(), 0 }
  };

  screenshot_manager()->Reset();
  for (int i = 0; !navigations[i].url.is_empty(); ++i) {
    // Navigate via the user initiating a navigation from the UI.
    NavigationController::LoadURLParams params(navigations[i].url);
    params.transition_type = PageTransitionFromInt(navigations[i].transition);

    RenderViewHost* old_host = web_contents->GetRenderViewHost();
    web_contents->GetController().LoadURLWithParams(params);
    WaitForLoadStop(web_contents);
    screenshot_manager()->WaitUntilScreenshotIsReady();

    EXPECT_NE(old_host, web_contents->GetRenderViewHost())
        << navigations[i].url.spec();
    EXPECT_EQ(old_host, screenshot_manager()->screenshot_taken_for());

    NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
        web_contents->GetController().GetEntryAtOffset(-1));
    EXPECT_TRUE(screenshot_manager()->ScreenshotSetForEntry(entry));

    entry = NavigationEntryImpl::FromNavigationEntry(
        web_contents->GetController().GetActiveEntry());
    EXPECT_FALSE(screenshot_manager()->ScreenshotSetForEntry(entry));
    EXPECT_FALSE(entry->screenshot().get());
    screenshot_manager()->Reset();
  }

  // Increase the minimum interval between taking screenshots.
  set_min_screenshot_interval(60000);

  // Navigate again. This should not take any screenshot because of the
  // increased screenshot interval.
  NavigationController::LoadURLParams params(navigations[0].url);
  params.transition_type = PageTransitionFromInt(navigations[0].transition);
  web_contents->GetController().LoadURLWithParams(params);
  WaitForLoadStop(web_contents);
  screenshot_manager()->WaitUntilScreenshotIsReady();

  EXPECT_EQ(NULL, screenshot_manager()->screenshot_taken_for());
}

// Failing on win7_aura trybot (see crbug.com/260983).
#if defined(OS_WIN)
#define MAYBE_ContentWindowReparent \
        DISABLED_ContentWindowReparent
#else
#define MAYBE_ContentWindowReparent ContentWindowReparent
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_ContentWindowReparent) {
  ASSERT_NO_FATAL_FAILURE(
      StartTestWithPage("files/overscroll_navigation.html"));

  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->Init(ui::LAYER_NOT_DRAWN);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ExecuteSyncJSFunction(web_contents->GetRenderViewHost(), "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetView()->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  aura::test::EventGenerator generator(content->GetRootWindow(), content);
  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(20),
      1);

  window->AddChild(shell()->web_contents()->GetView()->GetContentNativeView());
}

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       ContentWindowClose) {
  ASSERT_NO_FATAL_FAILURE(
      StartTestWithPage("files/overscroll_navigation.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  ExecuteSyncJSFunction(web_contents->GetRenderViewHost(), "navigate_next()");
  EXPECT_EQ(1, GetCurrentIndex());

  aura::Window* content = web_contents->GetView()->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  aura::test::EventGenerator generator(content->GetRootWindow(), content);
  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(20),
      1);

  delete web_contents->GetView()->GetContentNativeView();
}

IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       RepeatedQuickOverscrollGestures) {
  ASSERT_NO_FATAL_FAILURE(
      StartTestWithPage("files/overscroll_navigation.html"));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  NavigationController& controller = web_contents->GetController();
  RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
      web_contents->GetRenderViewHost());
  WebContentsViewAura* view_aura = static_cast<WebContentsViewAura*>(
      web_contents->GetView());
  view_aura->SetupOverlayWindowForTesting();
  ExecuteSyncJSFunction(view_host, "install_touch_handler()");

  // Navigate twice, then navigate back in history once.
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  ExecuteSyncJSFunction(view_host, "navigate_next()");
  EXPECT_EQ(2, GetCurrentIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  web_contents->GetController().GoBack();
  WaitForLoadStop(web_contents);
  EXPECT_EQ(1, GetCurrentIndex());
  EXPECT_EQ(base::ASCIIToUTF16("Title: #1"), web_contents->GetTitle());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());

  aura::Window* content = web_contents->GetView()->GetContentNativeView();
  gfx::Rect bounds = content->GetBoundsInRootWindow();
  aura::test::EventGenerator generator(content->GetRootWindow(), content);

  // Do a swipe left to start a forward navigation. Then quickly do a swipe
  // right.
  string16 expected_title = ASCIIToUTF16("Title: #2");
  content::TitleWatcher title_watcher(web_contents, expected_title);
  NavigationWatcher nav_watcher(web_contents);

  generator.GestureScrollSequence(
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(2000),
      10);
  nav_watcher.WaitUntilNavigationStarts();

  generator.GestureScrollSequence(
      gfx::Point(bounds.x() + 2, bounds.y() + 10),
      gfx::Point(bounds.right() - 10, bounds.y() + 10),
      base::TimeDelta::FromMilliseconds(2000),
      10);
  string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);

  EXPECT_EQ(2, GetCurrentIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

}  // namespace content
