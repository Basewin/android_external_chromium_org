// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/file_handler_info.h"

namespace web_app {

// Returns the full path of the .app shim that would be created by
// CreateShortcuts().
base::FilePath GetAppInstallPath(const ShortcutInfo& shortcut_info);

// If necessary, launch the shortcut for an app.
void MaybeLaunchShortcut(const ShortcutInfo& shortcut_info);

// Creates a shortcut for a web application. The shortcut is a stub app
// that simply loads the browser framework and runs the given app.
class WebAppShortcutCreator {
 public:
  // Creates a new shortcut based on information in |shortcut_info|.
  // A copy of the shortcut is placed in |app_data_dir|.
  // |chrome_bundle_id| is the CFBundleIdentifier of the Chrome browser bundle.
  WebAppShortcutCreator(const base::FilePath& app_data_dir,
                        const ShortcutInfo& shortcut_info,
                        const extensions::FileHandlersInfo& file_handlers_info);

  virtual ~WebAppShortcutCreator();

  // Returns the base name for the shortcut.
  base::FilePath GetShortcutBasename() const;

  // Returns a path to the Chrome Apps folder in the relevant applications
  // folder. E.g. ~/Applications or /Applications.
  virtual base::FilePath GetApplicationsDirname() const;

  // The full path to the app bundle under the relevant Applications folder.
  base::FilePath GetApplicationsShortcutPath() const;

  // The full path to the app bundle under the profile folder.
  base::FilePath GetInternalShortcutPath() const;

  bool CreateShortcuts(ShortcutCreationReason creation_reason,
                       ShortcutLocations creation_locations);
  void DeleteShortcuts();
  bool UpdateShortcuts();

 protected:
  // Returns a path to an app bundle with the given id. Or an empty path if no
  // matching bundle was found.
  // Protected and virtual so it can be mocked out for testing.
  virtual base::FilePath GetAppBundleById(const std::string& bundle_id) const;

  // Show the bundle we just generated in the Finder.
  virtual void RevealAppShimInFinder() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, DeleteShortcuts);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateIcon);
  FRIEND_TEST_ALL_PREFIXES(WebAppShortcutCreatorTest, UpdateShortcuts);

  // Returns the bundle identifier to use for this app bundle.
  std::string GetBundleIdentifier() const;

  // Returns the bundle identifier for the internal copy of the bundle.
  std::string GetInternalBundleIdentifier() const;

  // Copies the app loader template into a temporary directory and fills in all
  // relevant information.
  bool BuildShortcut(const base::FilePath& staging_path) const;

  // Builds a shortcut and copies it into the given destination folders.
  // Returns with the number of successful copies. Returns on the first failure.
  size_t CreateShortcutsIn(const std::vector<base::FilePath>& folders) const;

  // Updates the InfoPlist.string inside |app_path| with the display name for
  // the app.
  bool UpdateDisplayName(const base::FilePath& app_path) const;

  // Updates the bundle id of the internal copy of the app shim bundle.
  bool UpdateInternalBundleIdentifier() const;

  // Updates the plist inside |app_path| with information about the app.
  bool UpdatePlist(const base::FilePath& app_path) const;

  // Updates the icon for the shortcut.
  bool UpdateIcon(const base::FilePath& app_path) const;

  // Path to the data directory for this app. For example:
  // ~/Library/Application Support/Chromium/Default/Web Applications/_crx_abc/
  base::FilePath app_data_dir_;

  // Information about the app.
  ShortcutInfo info_;

  // The app's file handlers.
  extensions::FileHandlersInfo file_handlers_info_;

  DISALLOW_COPY_AND_ASSIGN(WebAppShortcutCreator);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MAC_H_
