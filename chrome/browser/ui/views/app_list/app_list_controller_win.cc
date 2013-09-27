// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dwmapi.h>
#include <sstream>

#include "apps/pref_names.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_service_win.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/apps/app_metro_infobar_delegate_win.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/views/app_list/keep_alive_service_impl.h"
#include "chrome/browser/ui/views/app_list/win/activation_tracker.h"
#include "chrome/browser/ui/views/app_list/win/app_list_shower.h"
#include "chrome/browser/ui/views/app_list/win/app_list_view_factory.h"
#include "chrome/browser/ui/views/app_list/win/app_list_view_win.h"
#include "chrome/browser/ui/views/app_list/win/app_list_view_win_impl.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "net/base/url_util.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"
#include "win8/util/win8_util.h"

#if defined(USE_AURA)
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace {

// Offset from the cursor to the point of the bubble arrow. It looks weird
// if the arrow comes up right on top of the cursor, so it is offset by this
// amount.
static const int kAnchorOffset = 25;

// Migrate chrome::kAppLauncherIsEnabled pref to
// chrome::kAppLauncherHasBeenEnabled pref.
void MigrateAppLauncherEnabledPref() {
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(apps::prefs::kAppLauncherIsEnabled)) {
    prefs->SetBoolean(apps::prefs::kAppLauncherHasBeenEnabled,
                      prefs->GetBoolean(apps::prefs::kAppLauncherIsEnabled));
    prefs->ClearPref(apps::prefs::kAppLauncherIsEnabled);
  }
}

// Icons are added to the resources of the DLL using icon names. The icon index
// for the app list icon is named IDR_X_APP_LIST or (for official builds)
// IDR_X_APP_LIST_SXS for Chrome Canary. Creating shortcuts needs to specify a
// resource index, which are different to icon names.  They are 0 based and
// contiguous. As Google Chrome builds have extra icons the icon for Google
// Chrome builds need to be higher. Unfortunately these indexes are not in any
// generated header file.
int GetAppListIconIndex() {
  const int kAppListIconIndex = 5;
  const int kAppListIconIndexSxS = 6;
  const int kAppListIconIndexChromium = 1;
#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess())
    return kAppListIconIndexSxS;
  return kAppListIconIndex;
#else
  return kAppListIconIndexChromium;
#endif
}

string16 GetAppListIconPath() {
  base::FilePath icon_path;
  if (!PathService::Get(base::FILE_EXE, &icon_path)) {
    NOTREACHED();
    return string16();
  }

  std::stringstream ss;
  ss << "," << GetAppListIconIndex();
  string16 result = icon_path.value();
  result.append(UTF8ToUTF16(ss.str()));
  return result;
}

string16 GetAppListShortcutName() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY)
    return l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME_CANARY);
  return l10n_util::GetStringUTF16(IDS_APP_LIST_SHORTCUT_NAME);
}

CommandLine GetAppListCommandLine() {
  const char* const kSwitchesToCopy[] = { switches::kUserDataDir };
  CommandLine* current = CommandLine::ForCurrentProcess();
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
     NOTREACHED();
     return CommandLine(CommandLine::NO_PROGRAM);
  }
  CommandLine command_line(chrome_exe);
  command_line.CopySwitchesFrom(*current, kSwitchesToCopy,
                                arraysize(kSwitchesToCopy));
  command_line.AppendSwitch(switches::kShowAppList);
  return command_line;
}

string16 GetAppModelId() {
  // The AppModelId should be the same for all profiles in a user data directory
  // but different for different user data directories, so base it on the
  // initial profile in the current user data directory.
  base::FilePath initial_profile_path;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    initial_profile_path =
        command_line->GetSwitchValuePath(switches::kUserDataDir).AppendASCII(
            chrome::kInitialProfile);
  }
  return ShellIntegration::GetAppListAppModelIdForProfile(initial_profile_path);
}

void SetDidRunForNDayActiveStats() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path)) {
    NOTREACHED();
    return;
  }
  bool system_install =
      !InstallUtil::IsPerUserInstall(exe_path.value().c_str());
  // Using Chrome Binary dist: Chrome dist may not exist for the legacy
  // App Launcher, and App Launcher dist may be "shadow", which does not
  // contain the information needed to determine multi-install.
  // Edge case involving Canary: crbug/239163.
  BrowserDistribution* chrome_binaries_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES);
  if (chrome_binaries_dist &&
      InstallUtil::IsMultiInstall(chrome_binaries_dist, system_install)) {
    BrowserDistribution* app_launcher_dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_APP_HOST);
    GoogleUpdateSettings::UpdateDidRunStateForDistribution(
        app_launcher_dist,
        true /* did_run */,
        system_install);
  }
}

// The start menu shortcut is created on first run by users that are
// upgrading. The desktop and taskbar shortcuts are created the first time the
// user enables the app list. The taskbar shortcut is created in
// |user_data_dir| and will use a Windows Application Model Id of
// |app_model_id|. This runs on the FILE thread and not in the blocking IO
// thread pool as there are other tasks running (also on the FILE thread)
// which fiddle with shortcut icons
// (ShellIntegration::MigrateWin7ShortcutsOnPath). Having different threads
// fiddle with the same shortcuts could cause race issues.
void CreateAppListShortcuts(
    const base::FilePath& user_data_dir,
    const string16& app_model_id,
    const ShellIntegration::ShortcutLocations& creation_locations) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Shortcut paths under which to create shortcuts.
  std::vector<base::FilePath> shortcut_paths =
      web_app::internals::GetShortcutPaths(creation_locations);

  bool pin_to_taskbar = creation_locations.in_quick_launch_bar &&
                        (base::win::GetVersion() >= base::win::VERSION_WIN7);

  // Create a shortcut in the |user_data_dir| for taskbar pinning.
  if (pin_to_taskbar)
    shortcut_paths.push_back(user_data_dir);
  bool success = true;

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }

  string16 app_list_shortcut_name = GetAppListShortcutName();

  string16 wide_switches(GetAppListCommandLine().GetArgumentsString());

  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(chrome_exe);
  shortcut_properties.set_working_dir(chrome_exe.DirName());
  shortcut_properties.set_arguments(wide_switches);
  shortcut_properties.set_description(app_list_shortcut_name);
  shortcut_properties.set_icon(chrome_exe, GetAppListIconIndex());
  shortcut_properties.set_app_id(app_model_id);

  for (size_t i = 0; i < shortcut_paths.size(); ++i) {
    base::FilePath shortcut_file =
        shortcut_paths[i].Append(app_list_shortcut_name).
            AddExtension(installer::kLnkExt);
    if (!base::PathExists(shortcut_file.DirName()) &&
        !file_util::CreateDirectory(shortcut_file.DirName())) {
      NOTREACHED();
      return;
    }
    success = success && base::win::CreateOrUpdateShortcutLink(
        shortcut_file, shortcut_properties,
        base::win::SHORTCUT_CREATE_ALWAYS);
  }

  if (success && pin_to_taskbar) {
    base::FilePath shortcut_to_pin =
        user_data_dir.Append(app_list_shortcut_name).
            AddExtension(installer::kLnkExt);
    success = base::win::TaskbarPinShortcutLink(
        shortcut_to_pin.value().c_str()) && success;
  }
}

class AppListControllerDelegateWin : public AppListControllerDelegate {
 public:
  AppListControllerDelegateWin();
  virtual ~AppListControllerDelegateWin();

 private:
  // AppListController overrides:
  virtual void DismissView() OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool CanPin() OVERRIDE;
  virtual void OnShowExtensionPrompt() OVERRIDE;
  virtual void OnCloseExtensionPrompt() OVERRIDE;
  virtual bool CanDoCreateShortcutsFlow(bool is_platform_app) OVERRIDE;
  virtual void DoCreateShortcutsFlow(Profile* profile,
                                     const std::string& extension_id) OVERRIDE;
  virtual void CreateNewWindow(Profile* profile, bool incognito) OVERRIDE;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           AppListSource source,
                           int event_flags) OVERRIDE;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         AppListSource source,
                         int event_flags) OVERRIDE;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateWin);
};

// Customizes the app list |hwnd| for Windows (eg: disable aero peek, set up
// restart params).
void SetWindowAttributes(HWND hwnd) {
  // Vista and lower do not offer pinning to the taskbar, which makes any
  // presence on the taskbar useless. So, hide the window on the taskbar
  // for these versions of Windows.
  if (base::win::GetVersion() <= base::win::VERSION_VISTA) {
    LONG_PTR ex_styles = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex_styles |= WS_EX_TOOLWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_styles);
  }

  if (base::win::GetVersion() > base::win::VERSION_VISTA) {
    // Disable aero peek. Without this, hovering over the taskbar popup puts
    // Windows into a mode for switching between windows in the same
    // application. The app list has just one window, so it is just distracting.
    BOOL disable_value = TRUE;
    ::DwmSetWindowAttribute(hwnd,
                            DWMWA_DISALLOW_PEEK,
                            &disable_value,
                            sizeof(disable_value));
  }

  ui::win::SetAppIdForWindow(GetAppModelId(), hwnd);
  CommandLine relaunch = GetAppListCommandLine();
  string16 app_name(GetAppListShortcutName());
  ui::win::SetRelaunchDetailsForWindow(
      relaunch.GetCommandLineString(), app_name, hwnd);
  ::SetWindowText(hwnd, app_name.c_str());
  string16 icon_path = GetAppListIconPath();
  ui::win::SetAppIconForWindow(icon_path, hwnd);
}

class AppListViewFactoryImpl : public AppListViewFactory {
 public:
  AppListViewFactoryImpl() {}
  virtual ~AppListViewFactoryImpl() {}

  virtual AppListViewWin* CreateAppListView(
      Profile* profile,
      app_list::PaginationModel* pagination_model,
      const base::Closure& on_should_dismiss) OVERRIDE {
    // The controller will be owned by the view delegate, and the delegate is
    // owned by the app list view. The app list view manages it's own lifetime.
    // TODO(koz): Make AppListViewDelegate take a scoped_ptr.
    AppListViewDelegate* view_delegate = new AppListViewDelegate(
        new AppListControllerDelegateWin, profile);
    app_list::AppListView* view = new app_list::AppListView(view_delegate);
    gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
    view->InitAsBubbleAtFixedLocation(NULL,
                                      pagination_model,
                                      cursor,
                                      views::BubbleBorder::FLOAT,
                                      false /* border_accepts_events */);
    SetWindowAttributes(view->GetHWND());
    return new AppListViewWinImpl(view, on_should_dismiss);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListViewFactoryImpl);
};

// The AppListController class manages global resources needed for the app
// list to operate, and controls when the app list is opened and closed.
// TODO(tapted): Rename this class to AppListServiceWin and move entire file to
// chrome/browser/ui/app_list/app_list_service_win.cc after removing
// chrome/browser/ui/views dependency.
class AppListController : public AppListServiceWin {
 public:
  virtual ~AppListController();

  static AppListController* GetInstance() {
    return Singleton<AppListController,
                     LeakySingletonTraits<AppListController> >::get();
  }

  void set_can_close(bool can_close) {
    shower_->set_can_close(can_close);
  }

  void OnAppListClosing();

  // AppListService overrides:
  virtual void SetAppListNextPaintCallback(
      const base::Closure& callback) OVERRIDE;
  virtual void HandleFirstRun() OVERRIDE;
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual void CreateForProfile(Profile* requested_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual AppListControllerDelegate* CreateControllerDelegate() OVERRIDE;

  // AppListServiceImpl overrides:
  virtual void CreateShortcut() OVERRIDE;

  // AppListServiceWin overrides:
  virtual app_list::AppListModel* GetAppListModelForTesting() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListController>;

  AppListController();

  bool IsWarmupNeeded();
  void ScheduleWarmup();

  // Loads the profile last used with the app list and populates the view from
  // it without showing it so that the next show is faster. Does nothing if the
  // view already exists, or another profile is in the middle of being loaded to
  // be shown.
  void LoadProfileForWarmup();
  void OnLoadProfileForWarmup(Profile* initial_profile);

  // Responsible for putting views on the screen.
  scoped_ptr<AppListShower> shower_;

  bool enable_app_list_on_next_init_;

  base::WeakPtrFactory<AppListController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListController);
};

AppListControllerDelegateWin::AppListControllerDelegateWin() {}

AppListControllerDelegateWin::~AppListControllerDelegateWin() {}

void AppListControllerDelegateWin::DismissView() {
  AppListController::GetInstance()->DismissAppList();
}

void AppListControllerDelegateWin::ViewClosing() {
  AppListController::GetInstance()->OnAppListClosing();
}

gfx::NativeWindow AppListControllerDelegateWin::GetAppListWindow() {
  return AppListController::GetInstance()->GetAppListWindow();
}

gfx::ImageSkia AppListControllerDelegateWin::GetWindowIcon() {
  gfx::ImageSkia* resource = ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(chrome::GetAppListIconResourceId());
  return *resource;
}

bool AppListControllerDelegateWin::CanPin() {
  return false;
}

void AppListControllerDelegateWin::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  AppListService* service = AppListController::GetInstance();
  service->SetProfilePath(profile_path);
  service->Show();
}

void AppListControllerDelegateWin::OnShowExtensionPrompt() {
  AppListController::GetInstance()->set_can_close(false);
}

void AppListControllerDelegateWin::OnCloseExtensionPrompt() {
  AppListController::GetInstance()->set_can_close(true);
}

bool AppListControllerDelegateWin::CanDoCreateShortcutsFlow(
    bool is_platform_app) {
  return true;
}

void AppListControllerDelegateWin::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_id);
  DCHECK(extension);

  gfx::NativeWindow parent_hwnd = GetAppListWindow();
  if (!parent_hwnd)
    return;
  OnShowExtensionPrompt();
  chrome::ShowCreateChromeAppShortcutsDialog(
      parent_hwnd, profile, extension,
      base::Bind(&AppListControllerDelegateWin::OnCloseExtensionPrompt,
                 base::Unretained(this)));
}

void AppListControllerDelegateWin::CreateNewWindow(Profile* profile,
                                                   bool incognito) {
  Profile* window_profile = incognito ?
      profile->GetOffTheRecordProfile() : profile;
  chrome::NewEmptyWindow(window_profile, chrome::GetActiveDesktop());
}

void AppListControllerDelegateWin::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  LaunchApp(profile, extension, source, event_flags);
}

void AppListControllerDelegateWin::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  AppListServiceImpl::RecordAppListAppLaunch();

  AppLaunchParams params(profile, extension, NEW_FOREGROUND_TAB);

  if (source != LAUNCH_FROM_UNKNOWN &&
      extension->id() == extension_misc::kWebStoreAppId) {
    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url,
        extension_urls::kWebstoreSourceField,
        AppListSourceToString(source));
  }

  OpenApplication(params);
}

AppListController::AppListController()
    : enable_app_list_on_next_init_(false),
      shower_(new AppListShower(make_scoped_ptr(new AppListViewFactoryImpl),
                                make_scoped_ptr(new KeepAliveServiceImpl))),
      weak_factory_(this) {}

AppListController::~AppListController() {
}

gfx::NativeWindow AppListController::GetAppListWindow() {
  return shower_->GetWindow();
}

AppListControllerDelegate* AppListController::CreateControllerDelegate() {
  return new AppListControllerDelegateWin();
}

app_list::AppListModel* AppListController::GetAppListModelForTesting() {
  return static_cast<AppListViewWinImpl*>(shower_->view())->model();
}

void AppListController::ShowForProfile(Profile* requested_profile) {
  DCHECK(requested_profile);
  if (requested_profile->IsManaged())
    return;

  ScopedKeepAlive show_app_list_keepalive;

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(SetDidRunForNDayActiveStats));

  if (win8::IsSingleWindowMetroMode()) {
    // This request came from Windows 8 in desktop mode, but chrome is currently
    // running in Metro mode.
    AppMetroInfoBarDelegateWin::Create(
        requested_profile, AppMetroInfoBarDelegateWin::SHOW_APP_LIST,
        std::string());
    return;
  }

  InvalidatePendingProfileLoads();
  // TODO(koz): Investigate making SetProfile() call SetProfilePath() itself.
  SetProfilePath(requested_profile->GetPath());
  SetProfile(requested_profile);
  shower_->ShowForProfile(requested_profile);
  RecordAppListLaunch();
}

void AppListController::DismissAppList() {
  shower_->DismissAppList();
}

void AppListController::OnAppListClosing() {
  shower_->CloseAppList();
  SetProfile(NULL);
}

void AppListController::OnLoadProfileForWarmup(Profile* initial_profile) {
  if (!IsWarmupNeeded())
    return;

  base::Time before_warmup(base::Time::Now());
  shower_->WarmupForProfile(initial_profile);
  UMA_HISTOGRAM_TIMES("Apps.AppListWarmupDuration",
                      base::Time::Now() - before_warmup);
}

void AppListController::SetAppListNextPaintCallback(
    const base::Closure& callback) {
  app_list::AppListView::SetNextPaintCallback(callback);
}

void AppListController::HandleFirstRun() {
  PrefService* local_state = g_browser_process->local_state();
  // If the app list is already enabled during first run, then the user had
  // opted in to the app launcher before uninstalling, so we re-enable to
  // restore shortcuts to the app list.
  // Note we can't directly create the shortcuts here because the IO thread
  // hasn't been created yet.
  enable_app_list_on_next_init_ = local_state->GetBoolean(
      apps::prefs::kAppLauncherHasBeenEnabled);
}

void AppListController::Init(Profile* initial_profile) {
  // In non-Ash metro mode, we can not show the app list for this process, so do
  // not bother performing Init tasks.
  if (win8::IsSingleWindowMetroMode())
    return;

  if (enable_app_list_on_next_init_) {
    enable_app_list_on_next_init_ = false;
    EnableAppList(initial_profile);
    CreateShortcut();
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kRestartWithAppList) &&
      prefs->GetBoolean(prefs::kRestartWithAppList)) {
    prefs->SetBoolean(prefs::kRestartWithAppList, false);
    // If we are restarting in Metro mode we will lose focus straight away. We
    // need to reacquire focus when that happens.
    shower_->ShowAndReacquireFocus(initial_profile);
  }

  // Migrate from legacy app launcher if we are on a non-canary and non-chromium
  // build.
#if defined(GOOGLE_CHROME_BUILD)
  if (!InstallUtil::IsChromeSxSProcess() &&
      !chrome_launcher_support::GetAnyAppHostPath().empty()) {
    chrome_launcher_support::InstallationState state =
        chrome_launcher_support::GetAppLauncherInstallationState();
    if (state == chrome_launcher_support::NOT_INSTALLED) {
      // If app_host.exe is found but can't be located in the registry,
      // skip the migration as this is likely a developer build.
      return;
    } else if (state == chrome_launcher_support::INSTALLED_AT_SYSTEM_LEVEL) {
      chrome_launcher_support::UninstallLegacyAppLauncher(
          chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION);
    } else if (state == chrome_launcher_support::INSTALLED_AT_USER_LEVEL) {
      chrome_launcher_support::UninstallLegacyAppLauncher(
          chrome_launcher_support::USER_LEVEL_INSTALLATION);
    }
    EnableAppList(initial_profile);
    CreateShortcut();
  }
#endif

  // Instantiate AppListController so it listens for profile deletions.
  AppListController::GetInstance();

  ScheduleWarmup();

  MigrateAppLauncherEnabledPref();
  HandleCommandLineFlags(initial_profile);
}

void AppListController::CreateForProfile(Profile* profile) {
  shower_->CreateViewForProfile(profile);
}

bool AppListController::IsAppListVisible() const {
  return shower_->IsAppListVisible();
}

void AppListController::CreateShortcut() {
  // Check if the app launcher shortcuts have ever been created before.
  // Shortcuts should only be created once. If the user unpins the taskbar
  // shortcut, they can restore it by pinning the start menu or desktop
  // shortcut.
  ShellIntegration::ShortcutLocations shortcut_locations;
  shortcut_locations.on_desktop = true;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.in_applications_menu = true;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  shortcut_locations.applications_menu_subdir =
      dist->GetStartMenuShortcutSubfolder(
          BrowserDistribution::SUBFOLDER_CHROME);
  base::FilePath user_data_dir(
      g_browser_process->profile_manager()->user_data_dir());

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateAppListShortcuts,
                 user_data_dir, GetAppModelId(), shortcut_locations));
}

void AppListController::ScheduleWarmup() {
  // Post a task to create the app list. This is posted to not impact startup
  // time.
  const int kInitWindowDelay = 30;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AppListController::LoadProfileForWarmup,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitWindowDelay));
}

bool AppListController::IsWarmupNeeded() {
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return false;

  // We only need to initialize the view if there's no view already created and
  // there's no profile loading to be shown.
  return !shower_->HasView() && !profile_loader().IsAnyProfileLoading();
}

void AppListController::LoadProfileForWarmup() {
  if (!IsWarmupNeeded())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path(GetProfilePath(profile_manager->user_data_dir()));

  profile_loader().LoadProfileInvalidatingOtherLoads(
      profile_path,
      base::Bind(&AppListController::OnLoadProfileForWarmup,
                 weak_factory_.GetWeakPtr()));
}

}  // namespace

namespace chrome {

AppListService* GetAppListServiceWin() {
  return AppListController::GetInstance();
}

}  // namespace chrome
