// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/audio/audio_observer.h"
#include "ash/system/brightness/brightness_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/power/clock_observer.h"
#include "ash/system/power/power_status_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/user/update_observer.h"
#include "ash/system/user/user_observer.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

ash::NetworkIconInfo CreateNetworkIconInfo(const Network* network,
                                           NetworkMenuIcon* network_icon) {
  ash::NetworkIconInfo info;
  info.name = UTF8ToUTF16(network->name());
  info.image = network_icon->GetBitmap(network, NetworkMenuIcon::SIZE_SMALL);
  info.service_path = network->service_path();
  return info;
}

class SystemTrayDelegate : public ash::SystemTrayDelegate,
                           public AudioHandler::VolumeObserver,
                           public PowerManagerClient::Observer,
                           public NetworkMenuIcon::Delegate,
                           public NetworkMenu::Delegate,
                           public NetworkLibrary::NetworkManagerObserver,
                           public NetworkLibrary::NetworkObserver,
                           public NetworkLibrary::CellularDataPlanObserver,
                           public content::NotificationObserver,
                           public system::TimezoneSettings::Observer,
                           public SystemKeyEventListener::CapsLockObserver {
 public:
  explicit SystemTrayDelegate(ash::SystemTray* tray)
      : tray_(tray),
        network_icon_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_icon_large_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_menu_(ALLOW_THIS_IN_INITIALIZER_LIST(new NetworkMenu(this))),
        clock_type_(base::k24HourClock) {
    AudioHandler::GetInstance()->AddVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate(
        PowerManagerClient::UPDATE_INITIAL);

    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->AddNetworkManagerObserver(this);
    OnNetworkManagerChanged(crosnet);
    crosnet->AddCellularDataPlanObserver(this);

    system::TimezoneSettings::GetInstance()->AddObserver(this);

    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->AddCapsLockObserver(this);

    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources());

    SetProfile(ProfileManager::GetDefaultProfile());

    network_icon_large_->SetResourceSize(NetworkMenuIcon::SIZE_LARGE);

    accessibility_enabled_.Init(prefs::kSpokenFeedbackEnabled,
                                g_browser_process->local_state(), this);
  }

  virtual ~SystemTrayDelegate() {
    AudioHandler* audiohandler = AudioHandler::GetInstance();
    if (audiohandler)
      audiohandler->RemoveVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
    system::TimezoneSettings::GetInstance()->RemoveObserver(this);
    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->RemoveCapsLockObserver(this);
  }

  // Overridden from ash::SystemTrayDelegate:
  virtual const std::string GetUserDisplayName() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().GetDisplayName();
  }

  virtual const std::string GetUserEmail() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().email();
  }

  virtual const SkBitmap& GetUserImage() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().image();
  }

  virtual ash::user::LoginStatus GetUserLoginStatus() const OVERRIDE {
    UserManager* manager = UserManager::Get();
    if (!manager->IsUserLoggedIn())
      return ash::user::LOGGED_IN_NONE;
    if (manager->IsCurrentUserOwner())
      return ash::user::LOGGED_IN_OWNER;
    if (manager->IsLoggedInAsGuest())
      return ash::user::LOGGED_IN_GUEST;
    if (manager->IsLoggedInAsDemoUser())
      return ash::user::LOGGED_IN_KIOSK;
    return ash::user::LOGGED_IN_USER;
  }

  virtual bool SystemShouldUpgrade() const OVERRIDE {
    return UpgradeDetector::GetInstance()->notify_upgrade();
  }

  virtual int GetSystemUpdateIconResource() const OVERRIDE {
    return UpgradeDetector::GetInstance()->GetIconResourceID(
        UpgradeDetector::UPGRADE_ICON_TYPE_MENU_ICON);
  }

  virtual base::HourClockType GetHourClockType() const OVERRIDE {
    return clock_type_;
  }

  virtual PowerSupplyStatus GetPowerSupplyStatus() const OVERRIDE {
    // Explicitly query the power status.
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate(
        PowerManagerClient::UPDATE_USER);
    return power_supply_status_;
  }

  virtual void ShowSettings() OVERRIDE {
    GetAppropriateBrowser()->OpenOptionsDialog();
  }

  virtual void ShowDateSettings() OVERRIDE {
    GetAppropriateBrowser()->OpenAdvancedOptionsDialog();
  }

  virtual void ShowNetworkSettings() OVERRIDE {
    GetAppropriateBrowser()->OpenInternetOptionsDialog();
  }

  virtual void ShowHelp() OVERRIDE {
    GetAppropriateBrowser()->ShowHelpTab();
  }

  virtual bool IsAudioMuted() const OVERRIDE {
    return AudioHandler::GetInstance()->IsMuted();
  }

  virtual void SetAudioMuted(bool muted) OVERRIDE {
    return AudioHandler::GetInstance()->SetMuted(muted);
  }

  virtual float GetVolumeLevel() const OVERRIDE {
    return AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
  }

  virtual void SetVolumeLevel(float level) OVERRIDE {
    AudioHandler::GetInstance()->SetVolumePercent(level * 100.f);
  }

  virtual bool IsCapsLockOn() const OVERRIDE {
    input_method::InputMethodManager* ime_manager =
        input_method::InputMethodManager::GetInstance();
    return ime_manager->GetXKeyboard()->CapsLockIsEnabled();
  }

  virtual bool IsInAccessibilityMode() const OVERRIDE {
    return accessibility_enabled_.GetValue();
  }

  virtual void ShutDown() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
  }

  virtual void SignOut() OVERRIDE {
    BrowserList::AttemptUserExit();
  }

  virtual void RequestLockScreen() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyScreenLockRequested();
  }

  virtual ash::NetworkIconInfo GetMostRelevantNetworkIcon(bool large) OVERRIDE {
    ash::NetworkIconInfo info;
    info.image = !large ? network_icon_->GetIconAndText(&info.description) :
        network_icon_large_->GetIconAndText(&info.description);
    return info;
  }

  virtual void GetAvailableNetworks(
      std::vector<ash::NetworkIconInfo>* list) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();

    // Ethernet.
    if (crosnet->ethernet_available() && crosnet->ethernet_enabled()) {
      const EthernetNetwork* ethernet_network = crosnet->ethernet_network();
      if (ethernet_network) {
        ash::NetworkIconInfo info;
        info.image = network_icon_->GetBitmap(ethernet_network,
                                              NetworkMenuIcon::SIZE_SMALL);
        if (!ethernet_network->name().empty())
          info.name = UTF8ToUTF16(ethernet_network->name());
        else
          info.name =
              l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
        info.service_path = ethernet_network->service_path();
        list->push_back(info);
      }
    }

    // Wifi.
    if (crosnet->wifi_available() && crosnet->wifi_enabled()) {
      const WifiNetworkVector& wifi = crosnet->wifi_networks();
      for (size_t i = 0; i < wifi.size(); ++i)
        list->push_back(CreateNetworkIconInfo(wifi[i], network_icon_.get()));
    }

    // Cellular.
    if (crosnet->cellular_available() && crosnet->cellular_enabled()) {
      // TODO(sad): There are different cases for cellular networks, e.g.
      // de-activated networks, active networks that support data plan info,
      // networks with top-up URLs etc. All of these need to be handled
      // properly.
      const CellularNetworkVector& cell = crosnet->cellular_networks();
      for (size_t i = 0; i < cell.size(); ++i)
        list->push_back(CreateNetworkIconInfo(cell[i], network_icon_.get()));
    }

    // VPN (only if logged in).
    if (GetUserLoginStatus() == ash::user::LOGGED_IN_NONE)
      return;
    if (crosnet->connected_network() || crosnet->virtual_network_connected()) {
      const VirtualNetworkVector& vpns = crosnet->virtual_networks();
      for (size_t i = 0; i < vpns.size(); ++i)
        list->push_back(CreateNetworkIconInfo(vpns[i], network_icon_.get()));
    }
  }

  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    Network* network = crosnet->FindNetworkByPath(network_id);
    if (network)
      network_menu_->ConnectToNetwork(network);
  }

  virtual void ToggleAirplaneMode() OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->EnableOfflineMode(!crosnet->offline_mode());
  }

  virtual void ToggleWifi() OVERRIDE {
    network_menu_->ToggleWifi();
  }

  virtual void ToggleCellular() OVERRIDE {
    network_menu_->ToggleCellular();
  }

  virtual bool GetWifiAvailable() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->wifi_available();
  }

  virtual bool GetCellularAvailable() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->cellular_available();
  }

  virtual bool GetWifiEnabled() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->wifi_enabled();
  }

  virtual bool GetCellularEnabled() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->cellular_enabled();
  }

  virtual void ChangeProxySettings() OVERRIDE {
    CHECK(GetUserLoginStatus() == ash::user::LOGGED_IN_NONE);
    BaseLoginDisplayHost::default_host()->OpenProxySettings();
  }

 private:
  // Returns the last active browser. If there is no such browser, creates a new
  // browser window with an empty tab and returns it.
  Browser* GetAppropriateBrowser() {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      browser = Browser::NewEmptyWindow(ProfileManager::GetDefaultProfile());
    return browser;
  }

  void SetProfile(Profile* profile) {
    pref_registrar_.reset(new PrefChangeRegistrar);
    pref_registrar_->Init(profile->GetPrefs());
    pref_registrar_->Add(prefs::kUse24HourClock, this);
    UpdateClockType(profile->GetPrefs());
  }

  void UpdateClockType(PrefService* service) {
    clock_type_ = service->GetBoolean(prefs::kUse24HourClock) ?
        base::k24HourClock : base::k12HourClock;
    ash::ClockObserver* observer =
        ash::Shell::GetInstance()->tray()->clock_observer();
    clock_type_ = service->GetBoolean(prefs::kUse24HourClock) ?
        base::k24HourClock : base::k12HourClock;
    if (observer)
      observer->OnDateFormatChanged();
  }

  void NotifyRefreshClock() {
    ash::ClockObserver* observer =
        ash::Shell::GetInstance()->tray()->clock_observer();
    if (observer)
      observer->Refresh();
  }

  void NotifyRefreshNetwork() {
    ash::NetworkObserver* observer =
        ash::Shell::GetInstance()->tray()->network_observer();
    if (observer) {
      ash::NetworkIconInfo info;
      info.image = network_icon_->GetIconAndText(&info.description);
      observer->OnNetworkRefresh(info);
    }
  }

  void RefreshNetworkObserver(NetworkLibrary* crosnet) {
    const Network* network = crosnet->active_network();
    std::string new_path = network ? network->service_path() : std::string();
    if (active_network_path_ != new_path) {
      if (!active_network_path_.empty())
        crosnet->RemoveNetworkObserver(active_network_path_, this);
      if (!new_path.empty())
        crosnet->AddNetworkObserver(new_path, this);
      active_network_path_ = new_path;
    }
  }

  void RefreshNetworkDeviceObserver(NetworkLibrary* crosnet) {
    const NetworkDevice* cellular = crosnet->FindCellularDevice();
    std::string new_cellular_device_path = cellular ?
        cellular->device_path() : std::string();
    if (cellular_device_path_ != new_cellular_device_path)
      cellular_device_path_ = new_cellular_device_path;
  }

  // Overridden from AudioHandler::VolumeObserver.
  virtual void OnVolumeChanged() OVERRIDE {
    float level = AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
    ash::Shell::GetInstance()->tray()->audio_observer()->
        OnVolumeChanged(level);
  }

  // Overridden from PowerManagerClient::Observer.
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE {
    ash::Shell::GetInstance()->tray()->brightness_observer()->
        OnBrightnessChanged(level / 100.f, user_initiated);
  }

  virtual void PowerChanged(const PowerSupplyStatus& power_status) OVERRIDE {
    power_supply_status_ = power_status;
    ash::PowerStatusObserver* observer =
        ash::Shell::GetInstance()->tray()->power_status_observer();
    if (observer)
      observer->OnPowerStatusChanged(power_status);
  }

  virtual void SystemResumed() OVERRIDE {
    NotifyRefreshClock();
  }

  virtual void LockScreen() OVERRIDE {
  }

  virtual void UnlockScreen() OVERRIDE {
  }

  virtual void UnlockScreenFailed() OVERRIDE {
  }

  // TODO(sad): Override more from PowerManagerClient::Observer here (e.g.
  // PowerButtonStateChanged etc.).

  // Overridden from NetworkMenuIcon::Delegate.
  virtual void NetworkMenuIconChanged() OVERRIDE {
    NotifyRefreshNetwork();
  }

  // Overridden from NetworkMenu::Delegate.
  virtual views::MenuButton* GetMenuButton() OVERRIDE {
    return NULL;
  }

  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE {
    return ash::Shell::GetInstance()->GetContainer(
        GetUserLoginStatus() == ash::user::LOGGED_IN_NONE ?
            ash::internal::kShellWindowId_LockSystemModalContainer :
            ash::internal::kShellWindowId_SystemModalContainer);
  }

  virtual void OpenButtonOptions() OVERRIDE {
  }

  virtual bool ShouldOpenButtonOptions() const OVERRIDE {
    return false;
  }

  // Overridden from NetworkLibrary::NetworkManagerObserver.
  virtual void OnNetworkManagerChanged(NetworkLibrary* crosnet) OVERRIDE {
    RefreshNetworkObserver(crosnet);
    RefreshNetworkDeviceObserver(crosnet);

    // TODO: ShowOptionalMobileDataPromoNotification?

    NotifyRefreshNetwork();
  }

  // Overridden from NetworkLibrary::NetworkObserver.
  virtual void OnNetworkChanged(NetworkLibrary* crosnet,
      const Network* network) OVERRIDE {
    NotifyRefreshNetwork();
  }

  // Overridden from NetworkLibrary::CellularDataPlanObserver.
  virtual void OnCellularDataPlanChanged(NetworkLibrary* crosnet) OVERRIDE {
    NotifyRefreshNetwork();
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
        tray_->UpdateAfterLoginStatusChange(GetUserLoginStatus());
        break;
      }
      case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
        ash::UpdateObserver* observer =
            ash::Shell::GetInstance()->tray()->update_observer();
        if (observer)
          observer->OnUpdateRecommended();
        break;
      }
      case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED: {
        ash::UserObserver* observer =
            ash::Shell::GetInstance()->tray()->user_observer();
        if (observer)
          observer->OnUserUpdate();
        break;
      }
      case chrome::NOTIFICATION_PREF_CHANGED: {
        std::string pref = *content::Details<std::string>(details).ptr();
        PrefService* service = content::Source<PrefService>(source).ptr();
        if (pref == prefs::kUse24HourClock) {
          UpdateClockType(service);
        } else if (pref == prefs::kSpokenFeedbackEnabled) {
          ash::AccessibilityObserver* observer =
              ash::Shell::GetInstance()->tray()->accessibility_observer();
          if (observer) {
            observer->OnAccessibilityModeChanged(
                service->GetBoolean(prefs::kSpokenFeedbackEnabled));
          }
        } else {
          NOTREACHED();
        }
        break;
      }
      case chrome::NOTIFICATION_SESSION_STARTED: {
        SetProfile(ProfileManager::GetDefaultProfile());
        break;
      }
      default:
        NOTREACHED();
    }
  }

  // Overridden from system::TimezoneSettings::Observer.
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE {
    NotifyRefreshClock();
  }

  // Overridden from SystemKeyEventListener::CapsLockObserver.
  virtual void OnCapsLockChange(bool enabled) OVERRIDE {
    ash::CapsLockObserver* observer =
      ash::Shell::GetInstance()->tray()->caps_lock_observer();
    if (observer)
      observer->OnCapsLockChanged(enabled);
  }

  ash::SystemTray* tray_;
  scoped_ptr<NetworkMenuIcon> network_icon_;
  scoped_ptr<NetworkMenuIcon> network_icon_large_;
  scoped_ptr<NetworkMenu> network_menu_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_registrar_;
  std::string cellular_device_path_;
  std::string active_network_path_;
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;
  PowerSupplyStatus power_supply_status_;
  base::HourClockType clock_type_;

  BooleanPrefMember accessibility_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegate);
};

}  // namespace

ash::SystemTrayDelegate* CreateSystemTrayDelegate(ash::SystemTray* tray) {
  return new chromeos::SystemTrayDelegate(tray);
}

}  // namespace chromeos
