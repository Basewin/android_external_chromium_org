// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_PROTECTOR_H_
#define CHROME_BROWSER_PROTECTOR_PROTECTOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/protector/setting_change.h"
#include "chrome/browser/protector/settings_change_global_error_delegate.h"

class GURL;
class Profile;
class TemplateURLService;

namespace protector {

class SettingsChangeGlobalError;

// Presents a SettingChange to user and handles possible user actions.
// Deletes itself after a user action is taken or timeout expires.
class Protector : public SettingsChangeGlobalErrorDelegate {
 public:
  explicit Protector(Profile* profile);

  // Opens a tab with specified URL in the browser window we've shown error
  // bubble for.
  void OpenTab(const GURL& url);

  // Returns TemplateURLService for the profile we've shown error bubble
  // for.
  TemplateURLService* GetTemplateURLService();

  // Shows global error about the specified change. Ownership of the change
  // is passed to the GlobalError object.
  void ShowChange(SettingChange* change);

  // SettingsChangeGlobalErrorDelegate implementation.
  virtual void OnApplyChange() OVERRIDE;
  virtual void OnDiscardChange() OVERRIDE;
  virtual void OnDecisionTimeout() OVERRIDE;
  virtual void OnRemovedFromProfile() OVERRIDE;

 private:
  friend class DeleteTask<Protector>;

  // The object can only be allocated and destroyed on heap.
  virtual ~Protector();

  // Performs the initial action on settings change and shows it. This is run
  // asynchronously on UI thread because |ShowChange| may be called in the
  // middle of some operations on settings that have changed.
  void InitAndShowChange(SettingChange* change);

  // Pointer to error bubble controller. Indicates if we're showing change
  // notification to user. Owns itself.
  scoped_ptr<SettingsChangeGlobalError> error_;

  // Profile which settings we are protecting.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(Protector);
};

// Signs string value with protector's key.
std::string SignSetting(const std::string& value);

// Returns true if the signature is valid for the specified key.
bool IsSettingValid(const std::string& value, const std::string& signature);

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_PROTECTOR_H_
