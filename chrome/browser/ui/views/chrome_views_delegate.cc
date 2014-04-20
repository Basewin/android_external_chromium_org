// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/accessibility/accessibility_event_router_views.h"
#include "chrome/browser/ui/views/accessibility/automation_manager_views.h"
#include "chrome/common/pref_names.h"
#include "grit/chrome_unscaled_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include <dwmapi.h>
#include "base/win/windows_version.h"
#include "chrome/browser/app_icon_win.h"
#include "ui/base/win/shell.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window_event_dispatcher.h"
#endif

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/host_desktop.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/views/linux_ui/linux_ui.h"
#endif

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/ash/ash_util.h"
#endif

namespace {

Profile* GetProfileForWindow(const views::Widget* window) {
  if (!window)
    return NULL;
  return reinterpret_cast<Profile*>(
      window->GetNativeWindowProperty(Profile::kProfileKey));
}

// If the given window has a profile associated with it, use that profile's
// preference service. Otherwise, store and retrieve the data from Local State.
// This function may return NULL if the necessary pref service has not yet
// been initialized.
// TODO(mirandac): This function will also separate windows by profile in a
// multi-profile environment.
PrefService* GetPrefsForWindow(const views::Widget* window) {
  Profile* profile = GetProfileForWindow(window);
  if (!profile) {
    // Use local state for windows that have no explicit profile.
    return g_browser_process->local_state();
  }
  return profile->GetPrefs();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ChromeViewsDelegate, views::ViewsDelegate implementation:

void ChromeViewsDelegate::SaveWindowPlacement(const views::Widget* window,
                                              const std::string& window_name,
                                              const gfx::Rect& bounds,
                                              ui::WindowShowState show_state) {
  PrefService* prefs = GetPrefsForWindow(window);
  if (!prefs)
    return;

  DCHECK(prefs->FindPreference(window_name.c_str()));
  DictionaryPrefUpdate update(prefs, window_name.c_str());
  base::DictionaryValue* window_preferences = update.Get();
  window_preferences->SetInteger("left", bounds.x());
  window_preferences->SetInteger("top", bounds.y());
  window_preferences->SetInteger("right", bounds.right());
  window_preferences->SetInteger("bottom", bounds.bottom());
  window_preferences->SetBoolean("maximized",
                                 show_state == ui::SHOW_STATE_MAXIMIZED);
  gfx::Rect work_area(gfx::Screen::GetScreenFor(window->GetNativeView())->
      GetDisplayNearestWindow(window->GetNativeView()).work_area());
  window_preferences->SetInteger("work_area_left", work_area.x());
  window_preferences->SetInteger("work_area_top", work_area.y());
  window_preferences->SetInteger("work_area_right", work_area.right());
  window_preferences->SetInteger("work_area_bottom", work_area.bottom());
}

bool ChromeViewsDelegate::GetSavedWindowPlacement(
    const views::Widget* widget,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  PrefService* prefs = g_browser_process->local_state();
  if (!prefs)
    return false;

  DCHECK(prefs->FindPreference(window_name.c_str()));
  const base::DictionaryValue* dictionary =
      prefs->GetDictionary(window_name.c_str());
  int left, top, right, bottom;
  if (!dictionary || !dictionary->GetInteger("left", &left) ||
      !dictionary->GetInteger("top", &top) ||
      !dictionary->GetInteger("right", &right) ||
      !dictionary->GetInteger("bottom", &bottom))
    return false;

  bounds->SetRect(left, top, right - left, bottom - top);

  bool maximized = false;
  if (dictionary)
    dictionary->GetBoolean("maximized", &maximized);
  *show_state = maximized ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;

#if defined(USE_ASH)
  // On Ash environment, a window won't span across displays.  Adjust
  // the bounds to fit the work area.
  gfx::NativeView window = widget->GetNativeView();
  if (chrome::GetHostDesktopTypeForNativeView(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH) {
    gfx::Display display = gfx::Screen::GetScreenFor(window)->
        GetDisplayMatching(*bounds);
    bounds->AdjustToFit(display.work_area());
    ash::wm::GetWindowState(window)->set_minimum_visibility(true);
  }
#endif
  return true;
}

void ChromeViewsDelegate::NotifyAccessibilityEvent(
    views::View* view, ui::AXEvent event_type) {
  AccessibilityEventRouterViews::GetInstance()->HandleAccessibilityEvent(
      view, event_type);

  AutomationManagerViews::GetInstance()->HandleEvent(
      GetProfileForWindow(view->GetWidget()), view, event_type);
}

void ChromeViewsDelegate::NotifyMenuItemFocused(
    const base::string16& menu_name,
    const base::string16& menu_item_name,
    int item_index,
    int item_count,
    bool has_submenu) {
  AccessibilityEventRouterViews::GetInstance()->HandleMenuItemFocused(
      menu_name, menu_item_name, item_index, item_count, has_submenu);
}

#if defined(OS_WIN)
HICON ChromeViewsDelegate::GetDefaultWindowIcon() const {
  return GetAppIcon();
}

bool ChromeViewsDelegate::IsWindowInMetro(gfx::NativeWindow window) const {
  return chrome::IsNativeViewInAsh(window);
}

#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
gfx::ImageSkia* ChromeViewsDelegate::GetDefaultWindowIcon() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_64);
}
#endif

views::NonClientFrameView* ChromeViewsDelegate::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
#if defined(USE_ASH)
  if (chrome::IsNativeViewInAsh(widget->GetNativeView()))
    return ash::Shell::GetInstance()->CreateDefaultNonClientFrameView(widget);
#endif
  return NULL;
}

void ChromeViewsDelegate::AddRef() {
  g_browser_process->AddRefModule();
}

void ChromeViewsDelegate::ReleaseRef() {
  g_browser_process->ReleaseModule();
}

content::WebContents* ChromeViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return NULL;
}

void ChromeViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // We need to determine opacity if it's not already specified.
  if (params->opacity == views::Widget::InitParams::INFER_OPACITY)
    params->opacity = GetOpacityForInitParams(*params);

  // If we already have a native_widget, we don't have to try to come
  // up with one.
  if (params->native_widget)
    return;

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  bool use_non_toplevel_window =
      params->parent && params->type != views::Widget::InitParams::TYPE_MENU;

#if defined(OS_WIN)
  // On desktop Linux Chrome must run in an environment that supports a variety
  // of window managers, some of which do not play nicely with parts of our UI
  // that have specific expectations about window sizing and placement. For this
  // reason windows opened as top level (params.top_level) are always
  // constrained by the browser frame, so we can position them correctly. This
  // has some negative side effects, like dialogs being clipped by the browser
  // frame, but the side effects are not as bad as the poor window manager
  // interactions. On Windows however these WM interactions are not an issue, so
  // we open windows requested as top_level as actual top level windows on the
  // desktop.
  use_non_toplevel_window = use_non_toplevel_window &&
      (!params->top_level ||
       chrome::GetHostDesktopTypeForNativeView(params->parent) !=
          chrome::HOST_DESKTOP_TYPE_NATIVE);

  if (!ui::win::IsAeroGlassEnabled()) {
    // If we don't have composition (either because Glass is not enabled or
    // because it was disabled at the command line), anything that requires
    // transparency will be broken with a toplevel window, so force the use of
    // a non toplevel window.
    if (params->opacity == views::Widget::InitParams::TRANSLUCENT_WINDOW &&
        params->type != views::Widget::InitParams::TYPE_MENU)
      use_non_toplevel_window = true;
  } else {
    // If we're on Vista+ with composition enabled, then we can use toplevel
    // windows for most things (they get blended via WS_EX_COMPOSITED, which
    // allows for animation effects, but also exceeding the bounds of the parent
    // window).
    if (chrome::GetActiveDesktop() != chrome::HOST_DESKTOP_TYPE_ASH &&
        params->parent &&
        params->type != views::Widget::InitParams::TYPE_CONTROL &&
        params->type != views::Widget::InitParams::TYPE_WINDOW) {
      // When we set this to false, we get a DesktopNativeWidgetAura from the
      // default case (not handled in this function).
      use_non_toplevel_window = false;
    }
  }
#endif  // OS_WIN
#endif  // USE_AURA

#if defined(OS_CHROMEOS)
  // When we are doing straight chromeos builds, we still need to handle the
  // toplevel window case.
  // There may be a few remaining widgets in Chrome OS that are not top level,
  // but have neither a context nor a parent. Provide a fallback context so
  // users don't crash. Developers will hit the DCHECK and should provide a
  // context.
  if (params->context)
    params->context = params->context->GetRootWindow();
  DCHECK(params->parent || params->context || params->top_level)
      << "Please provide a parent or context for this widget.";
  if (!params->parent && !params->context)
    params->context = ash::Shell::GetPrimaryRootWindow();
#elif defined(USE_AURA)
  // While the majority of the time, context wasn't plumbed through due to the
  // existence of a global WindowTreeClient, if this window is a toplevel, it's
  // possible that there is no contextual state that we can use.
  if (params->parent == NULL && params->context == NULL && params->top_level) {
    // We need to make a decision about where to place this window based on the
    // desktop type.
    switch (chrome::GetActiveDesktop()) {
      case chrome::HOST_DESKTOP_TYPE_NATIVE:
        // If we're native, we should give this window its own toplevel desktop
        // widget.
        params->native_widget = new views::DesktopNativeWidgetAura(delegate);
        break;
#if defined(USE_ASH)
      case chrome::HOST_DESKTOP_TYPE_ASH:
        // If we're in ash, give this window the context of the main monitor.
        params->context = ash::Shell::GetPrimaryRootWindow();
        break;
#endif
      default:
        NOTREACHED();
    }
  } else if (use_non_toplevel_window) {
    params->native_widget = new views::NativeWidgetAura(delegate);
  } else if (params->type != views::Widget::InitParams::TYPE_TOOLTIP) {
    // TODO(erg): Once we've threaded context to everywhere that needs it, we
    // should remove this check here.
    gfx::NativeView to_check =
        params->context ? params->context : params->parent;
    if (chrome::GetHostDesktopTypeForNativeView(to_check) ==
        chrome::HOST_DESKTOP_TYPE_NATIVE) {
      params->native_widget = new views::DesktopNativeWidgetAura(delegate);
    }
  }
#endif
}

base::TimeDelta
ChromeViewsDelegate::GetDefaultTextfieldObscuredRevealDuration() {
  return base::TimeDelta();
}

bool ChromeViewsDelegate::WindowManagerProvidesTitleBar(bool maximized) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // On Ubuntu Unity, the system always provides a title bar for maximized
  // windows.
  views::LinuxUI* ui = views::LinuxUI::instance();
  return maximized && ui && ui->UnityIsRunning();
#endif

  return false;
}

#if !defined(USE_AURA) && !defined(USE_CHROMEOS)
views::Widget::InitParams::WindowOpacity
ChromeViewsDelegate::GetOpacityForInitParams(
    const views::Widget::InitParams& params) {
  return views::Widget::InitParams::OPAQUE_WINDOW;
}
#endif
