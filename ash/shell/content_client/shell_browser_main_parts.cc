// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_browser_main_parts.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/window_watcher.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_browser_context.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_module.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/compositor_test_support.h"
#include "ui/gfx/screen.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/test/test_views_delegate.h"

#if defined(ENABLE_MESSAGE_CENTER)
#include "ui/message_center/message_center.h"
#endif

#if defined(OS_LINUX)
#include "ui/base/touch/touch_factory_x11.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

namespace ash {
namespace shell {
void InitWindowTypeLauncher();

namespace {
class ShellViewsDelegate : public views::TestViewsDelegate {
 public:
  ShellViewsDelegate() {}
  virtual ~ShellViewsDelegate() {}

  // Overridden from views::TestViewsDelegate:
  virtual views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    return ash::Shell::GetInstance()->CreateDefaultNonClientFrameView(widget);
  }
  virtual bool UseTransparentWindows() const OVERRIDE {
    // Ash uses transparent window frames.
    return true;
  }
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE {
    if (params->native_widget)
      return;

    if (!params->parent && !params->context && params->top_level)
      params->context = Shell::GetPrimaryRootWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(),
      delegate_(NULL) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
}

#if !defined(OS_MACOSX)
void ShellBrowserMainParts::PreMainMessageLoopStart() {
#if defined(OS_LINUX)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif
}
#endif

void ShellBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Initialize();
#endif
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_.reset(new content::ShellBrowserContext(false));

  // A ViewsDelegate is required.
  if (!views::ViewsDelegate::views_delegate)
    views::ViewsDelegate::views_delegate = new ShellViewsDelegate;

  delegate_ = new ash::shell::ShellDelegateImpl;
#if defined(ENABLE_MESSAGE_CENTER)
  // The global message center state must be initialized absent
  // g_browser_process.
  message_center::MessageCenter::Initialize();
#endif
  ash::Shell::CreateInstance(delegate_);
  ash::Shell::GetInstance()->set_browser_context(browser_context_.get());

  window_watcher_.reset(new ash::shell::WindowWatcher);
  gfx::Screen* screen = Shell::GetInstance()->GetScreen();
  screen->AddObserver(window_watcher_.get());
  delegate_->SetWatcher(window_watcher_.get());

  ash::shell::InitWindowTypeLauncher();

  DesktopBackgroundController* controller =
      Shell::GetInstance()->desktop_background_controller();
  if (controller->GetAppropriateResolution() == WALLPAPER_RESOLUTION_LARGE)
    controller->SetDefaultWallpaper(kDefaultLargeWallpaper);
  else
    controller->SetDefaultWallpaper(kDefaultSmallWallpaper);

  ash::Shell::GetPrimaryRootWindow()->ShowRootWindow();
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  gfx::Screen* screen = Shell::GetInstance()->GetScreen();
  screen->RemoveObserver(window_watcher_.get());

  window_watcher_.reset();
  delegate_->SetWatcher(NULL);
  delegate_ = NULL;
  ash::Shell::DeleteInstance();
#if defined(ENABLE_MESSAGE_CENTER)
  // The global message center state must be shutdown absent
  // g_browser_process.
  message_center::MessageCenter::Shutdown();
#endif
  aura::Env::DeleteInstance();

  // The keyboard may have created a WebContents. The WebContents is destroyed
  // with the UI, and it needs the BrowserContext to be alive during its
  // destruction. So destroy all of the UI elements before destroying the
  // browser context.
  browser_context_.reset();
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::MessageLoopForUI::current()->Run();
  return true;
}

}  // namespace shell
}  // namespace ash
