// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_extension_window_controller.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

BrowserExtensionWindowController::BrowserExtensionWindowController(
    Browser* browser)
    : ExtensionWindowController(browser->window(), browser->profile()),
      browser_(browser) {
}

const SessionID& BrowserExtensionWindowController::GetSessionId() const {
  return browser_->session_id();
}

namespace keys = extension_tabs_module_constants;

base::DictionaryValue*
BrowserExtensionWindowController::CreateWindowValue() const {
  DictionaryValue* result = ExtensionWindowController::CreateWindowValue();

  result->SetString(keys::kWindowTypeKey,
                    ExtensionTabUtil::GetWindowTypeText(browser_));
  result->SetString(keys::kShowStateKey,
                    ExtensionTabUtil::GetWindowShowStateText(browser_));

  return result;
}

base::DictionaryValue*
BrowserExtensionWindowController::CreateWindowValueWithTabs() const {
  DictionaryValue* result = CreateWindowValue();

  result->Set(keys::kTabsKey, ExtensionTabUtil::CreateTabList(browser_));

  return result;
}

bool BrowserExtensionWindowController::CanClose(
    ExtensionWindowController::Reason* reason) const {
  // Don't let an extension remove the window if the user is dragging tabs
  // in that window.
  if (!browser_->IsTabStripEditable()) {
    *reason = ExtensionWindowController::REASON_TAB_STRIP_NOT_EDITABLE;
    return false;
  }
  return true;
}

void BrowserExtensionWindowController::SetFullscreenMode(
    bool is_fullscreen,
    const GURL& extension_url) const {
  if (browser_->window()->IsFullscreen() != is_fullscreen)
    browser_->ToggleFullscreenModeWithExtension(extension_url);
}
