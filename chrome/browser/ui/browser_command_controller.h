// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_

#include "base/prefs/public/pref_change_registrar.h"
#include "chrome/browser/api/sync/profile_sync_service_observer.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class BrowserWindow;
class Profile;
class ProfileManager;

namespace content {
struct NativeWebKeyboardEvent;
}

namespace chrome {

class BrowserCommandController : public CommandUpdaterDelegate,
                                 public content::NotificationObserver,
                                 public ProfileInfoCacheObserver,
                                 public TabStripModelObserver,
                                 public TabRestoreServiceObserver {
 public:
  BrowserCommandController(Browser* browser, ProfileManager* profile_manager);
  virtual ~BrowserCommandController();

  CommandUpdater* command_updater() { return &command_updater_; }
  bool block_command_execution() const { return block_command_execution_; }

  // Returns true if |command_id| is a reserved command whose keyboard shortcuts
  // should not be sent to the renderer or |event| was triggered by a key that
  // we never want to send to the renderer.
  bool IsReservedCommandOrKey(int command_id,
                              const content::NativeWebKeyboardEvent& event);

  // Sets if command execution shall be blocked. If |block| is true then
  // following calls to ExecuteCommand() or ExecuteCommandWithDisposition()
  // method will not execute the command, and the last blocked command will be
  // recorded for retrieval.
  void SetBlockCommandExecution(bool block);

  // Gets the last blocked command after calling SetBlockCommandExecution(true).
  // Returns the command id or -1 if there is no command blocked. The
  // disposition type of the command will be stored in |*disposition| if it's
  // not NULL.
  int GetLastBlockedCommand(WindowOpenDisposition* disposition);

  // Notifies the controller that state has changed in one of the following
  // areas and it should update command states.
  void TabStateChanged();
  void ContentRestrictionsChanged();
  void FullscreenStateChanged();
  void PrintingStateChanged();
  void LoadingStateChanged(bool is_loading, bool force);

  // Shared state updating: these functions are static and public to share with
  // outside code.

  // Updates the open-file state.
  static void UpdateOpenFileState(CommandUpdater* command_updater);

  // Update commands whose state depends on incognito mode availability and that
  // only depend on the profile.
  static void UpdateSharedCommandsForIncognitoAvailability(
      CommandUpdater* command_updater,
      Profile* profile);

 private:
  enum FullScreenMode {
    // Not in fullscreen mode.
    FULLSCREEN_DISABLED,

    // Fullscreen mode, occupying the whole screen.
    FULLSCREEN_NORMAL,

    // Fullscreen mode for metro snap, occupying the full height and 20% of
    // the screen width.
    FULLSCREEN_METRO_SNAP,
  };

  // Overridden from CommandUpdaterDelegate:
  virtual void ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition disposition) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ProfileInfoCacheObserver:
  virtual void OnProfileAdded(const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWillBeRemoved(
      const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(const base::FilePath& profile_path,
                                   const string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(const base::FilePath& profile_path,
                                    const string16& old_profile_name) OVERRIDE;
  virtual void OnProfileAvatarChanged(const base::FilePath& profile_path) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(content::WebContents* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabDetachedAt(content::WebContents* contents,
                             int index) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) OVERRIDE;
  virtual void TabBlockedStateChanged(content::WebContents* contents,
                                      int index) OVERRIDE;

  // Overridden from TabRestoreServiceObserver:
  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE;
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE;

  // Returns true if the regular Chrome UI (not the fullscreen one and
  // not the single-tab one) is shown. Used for updating window command states
  // only. Consider using SupportsWindowFeature if you need the mentioned
  // functionality anywhere else.
  bool IsShowingMainUI(bool is_fullscreen);

  // Initialize state for all browser commands.
  void InitCommandState();

  // Update commands whose state depends on incognito mode availability.
  void UpdateCommandsForIncognitoAvailability();

  // Update commands whose state depends on the tab's state.
  void UpdateCommandsForTabState();

  // Updates commands when the content's restrictions change.
  void UpdateCommandsForContentRestrictionState();

  // Updates commands for enabling developer tools.
  void UpdateCommandsForDevTools();

  // Updates commands for bookmark editing.
  void UpdateCommandsForBookmarkEditing();

  // Updates commands that affect the bookmark bar.
  void UpdateCommandsForBookmarkBar();

  // Updates commands that affect file selection dialogs in aggregate,
  // namely the save-page-as state and the open-file state.
  void UpdateCommandsForFileSelectionDialogs();

  // Update commands whose state depends on the type of fullscreen mode the
  // window is in.
  void UpdateCommandsForFullscreenMode(FullScreenMode fullscreen_mode);

  // Update commands whose state depends on whether multiple profiles are
  // allowed.
  void UpdateCommandsForMultipleProfiles();

  // Updates the printing command state.
  void UpdatePrintingState();

  // Updates the save-page-as command state.
  void UpdateSaveAsState();

  // Ask the Reload/Stop button to change its icon, and update the Stop command
  // state.  |is_loading| is true if the current WebContents is loading.
  // |force| is true if the button should change its icon immediately.
  void UpdateReloadStopState(bool is_loading, bool force);

  // Updates commands for find.
  void UpdateCommandsForFind();

  // Add/remove observers for interstitial attachment/detachment from
  // |contents|.
  void AddInterstitialObservers(content::WebContents* contents);
  void RemoveInterstitialObservers(content::WebContents* contents);

  inline BrowserWindow* window();
  inline Profile* profile();

  Browser* browser_;

  ProfileManager* profile_manager_;

  // The CommandUpdater that manages the browser window commands.
  CommandUpdater command_updater_;

  // Indicates if command execution is blocked.
  bool block_command_execution_;

  // Stores the last blocked command id when |block_command_execution_| is true.
  int last_blocked_command_id_;

  // Stores the disposition type of the last blocked command.
  WindowOpenDisposition last_blocked_command_disposition_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar profile_pref_registrar_;
  PrefChangeRegistrar local_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCommandController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_
