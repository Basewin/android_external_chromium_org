// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_launcher.h"

#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

std::string GetAppUserNameFromAppId(const std::string& app_id) {
  return app_id + "@" + UserManager::kKioskAppUserDomain;
}

}  // namespace

// static
KioskAppLauncher* KioskAppLauncher::running_instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// KioskAppLauncher::CryptohomedChecker ensures cryptohome daemon is up
// and running by issuing an IsMounted call. If the call does not go through
// and chromeos::DBUS_METHOD_CALL_SUCCESS is not returned, it will retry after
// some time out and at the maximum five times before it gives up. Upon
// success, it resumes the launch by calling KioskAppLauncher::StartMount.

class KioskAppLauncher::CryptohomedChecker
    : public base::SupportsWeakPtr<CryptohomedChecker> {
 public:
  explicit CryptohomedChecker(KioskAppLauncher* launcher)
      : launcher_(launcher),
        retry_count_(0) {
  }
  ~CryptohomedChecker() {}

  void StartCheck() {
    chromeos::DBusThreadManager::Get()->GetCryptohomeClient()->IsMounted(
        base::Bind(&CryptohomedChecker::OnCryptohomeIsMounted,
                   AsWeakPtr()));
  }

 private:
  void OnCryptohomeIsMounted(chromeos::DBusMethodCallStatus call_status,
                             bool is_mounted) {
    if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
      const int kMaxRetryTimes = 5;
      ++retry_count_;
      if (retry_count_ > kMaxRetryTimes) {
        LOG(ERROR) << "Could not talk to cryptohomed for launching kiosk app.";
        ReportCheckResult(false);
        return;
      }

      const int retry_delay_in_milliseconds = 500 * (1 << retry_count_);
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&CryptohomedChecker::StartCheck, AsWeakPtr()),
          base::TimeDelta::FromMilliseconds(retry_delay_in_milliseconds));
      return;
    }

    if (is_mounted)
      LOG(ERROR) << "Cryptohome is mounted before launching kiosk app.";

    // Proceed only when cryptohome is not mounded or running on dev box.
    ReportCheckResult(!is_mounted || !base::chromeos::IsRunningOnChromeOS());
    return;
  }

  void ReportCheckResult(bool success) {
    if (success)
      launcher_->StartMount();
    else
      launcher_->ReportLaunchResult(false);
  }

  KioskAppLauncher* launcher_;
  int retry_count_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomedChecker);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppLauncher::ProfileLoader creates or loads the app profile.

class KioskAppLauncher::ProfileLoader : public LoginUtils::Delegate {
 public:
  explicit ProfileLoader(KioskAppLauncher* launcher)
      : launcher_(launcher) {
  }

  virtual ~ProfileLoader() {
    LoginUtils::Get()->DelegateDeleted(this);
  }

  void Start() {
    LoginUtils::Get()->PrepareProfile(
        GetAppUserNameFromAppId(launcher_->app_id_),
        std::string(),  // display email
        std::string(),  // password
        false,  // using_oauth
        false,  // has_cookies
        this);
  }

 private:
  // LoginUtils::Delegate overrides:
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE {
    launcher_->OnProfilePrepared(profile);
  }

  KioskAppLauncher* launcher_;
  DISALLOW_COPY_AND_ASSIGN(ProfileLoader);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppLauncher

KioskAppLauncher::KioskAppLauncher(const std::string& app_id,
                                   const LaunchCallback& callback)
    : app_id_(app_id),
      callback_(callback),
      success_(false),
      remove_attempted_(false) {
}

KioskAppLauncher::~KioskAppLauncher() {}

void KioskAppLauncher::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (running_instance_) {
    LOG(WARNING) << "Unable to launch " << app_id_ << "with a pending launch.";
    ReportLaunchResult(false);
    return;
  }

  running_instance_ = this;  // Reset in ReportLaunchResult.

  // Check cryptohomed. If all goes good, flow goes to StartMount. Otherwise
  // it goes to ReportLaunchResult with failure.
  crytohomed_checker.reset(new CryptohomedChecker(this));
  crytohomed_checker->StartCheck();
}

void KioskAppLauncher::ReportLaunchResult(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  running_instance_ = NULL;
  success_ = success;

  if (!callback_.is_null())
    callback_.Run(success_);
}

void KioskAppLauncher::StartMount() {
  const std::string token =
      CrosLibrary::Get()->GetCertLibrary()->EncryptWithSystemSalt(app_id_);

  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMount(
      app_id_,
      token,
      cryptohome::CREATE_IF_MISSING,
      base::Bind(&KioskAppLauncher::MountCallback,
                 base::Unretained(this)));
}

void KioskAppLauncher::MountCallback(bool mount_success,
                                     cryptohome::MountError mount_error) {
  if (mount_success) {
    profile_loader_.reset(new ProfileLoader(this));
    profile_loader_->Start();
    return;
  }

  if (!remove_attempted_) {
    LOG(ERROR) << "Attempt to remove app cryptohome because of mount failure"
               << ", mount error=" << mount_error;

    remove_attempted_ = true;
    AttemptRemove();
    return;
  }

  LOG(ERROR) << "Failed to mount app cryptohome, error=" << mount_error;
  ReportLaunchResult(false);
}

void KioskAppLauncher::AttemptRemove() {
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      app_id_,
      base::Bind(&KioskAppLauncher::RemoveCallback,
                 base::Unretained(this)));
}

void KioskAppLauncher::RemoveCallback(bool success,
                                      cryptohome::MountError return_code) {
  if (success) {
    StartMount();
    return;
  }

  LOG(ERROR) << "Failed to remove app cryptohome, erro=" << return_code;
  ReportLaunchResult(false);
}

void KioskAppLauncher::OnProfilePrepared(Profile* profile) {
  // StartupAppLauncher deletes itself when done.
  (new chromeos::StartupAppLauncher(profile, app_id_))->Start();

  BaseLoginDisplayHost::default_host()->OnSessionStart();
  UserManager::Get()->SessionStarted();

  ReportLaunchResult(true);
}

}  // namespace chromeos
