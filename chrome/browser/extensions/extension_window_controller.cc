// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_window_controller.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_window_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/base_window.h"
#include "ui/gfx/rect.h"

///////////////////////////////////////////////////////////////////////////////
// ExtensionWindowController

ExtensionWindowController::ExtensionWindowController(BaseWindow* window,
                                                     Profile* profile) :
    window_(window),
    profile_(profile) {
  ExtensionWindowList::GetInstance()->AddExtensionWindow(this);
}

ExtensionWindowController::~ExtensionWindowController() {
  ExtensionWindowList::GetInstance()->RemoveExtensionWindow(this);
}

bool ExtensionWindowController::MatchesProfile(
    Profile* match_profile,
    ProfileMatchType match_type) const {
  return ((profile_ == match_profile) ||
          (match_type == MATCH_INCOGNITO &&
           (match_profile->HasOffTheRecordProfile() &&
            match_profile->GetOffTheRecordProfile() == profile_)));
}

namespace keys = extension_tabs_module_constants;

base::DictionaryValue* ExtensionWindowController::CreateWindowValue() const {
  DictionaryValue* result = new DictionaryValue();

  result->SetInteger(keys::kIdKey, GetSessionId().id());
  result->SetBoolean(keys::kFocusedKey, window()->IsActive());
  result->SetBoolean(keys::kIncognitoKey, profile_->IsOffTheRecord());

  gfx::Rect bounds;
  if (window()->IsMinimized())
    bounds = window()->GetRestoredBounds();
  else
    bounds = window()->GetBounds();
  result->SetInteger(keys::kLeftKey, bounds.x());
  result->SetInteger(keys::kTopKey, bounds.y());
  result->SetInteger(keys::kWidthKey, bounds.width());
  result->SetInteger(keys::kHeightKey, bounds.height());

  return result;
}
