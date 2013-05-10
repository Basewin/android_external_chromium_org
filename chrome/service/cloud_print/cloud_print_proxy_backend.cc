// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/service/cloud_print/cloud_print_auth.h"
#include "chrome/service/cloud_print/cloud_print_connector.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/cloud_print/connector_settings.h"
#include "chrome/service/gaia/service_gaia_authenticator.h"
#include "chrome/service/net/service_url_request_context.h"
#include "chrome/service/service_process.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_client.h"
#include "jingle/notifier/listener/push_client_observer.h"
#include "ui/base/l10n/l10n_util.h"

namespace cloud_print {

// The real guts of CloudPrintProxyBackend, to keep the public client API clean.
class CloudPrintProxyBackend::Core
    : public base::RefCountedThreadSafe<CloudPrintProxyBackend::Core>,
      public CloudPrintAuth::Client,
      public CloudPrintConnector::Client,
      public notifier::PushClientObserver {
 public:
  // It is OK for print_server_url to be empty. In this case system should
  // use system default (local) print server.
  Core(CloudPrintProxyBackend* backend,
       const ConnectorSettings& settings,
       const gaia::OAuthClientInfo& oauth_client_info,
       bool enable_job_poll);

  // Note:
  //
  // The Do* methods are the various entry points from CloudPrintProxyBackend
  // It calls us on a dedicated thread to actually perform synchronous
  // (and potentially blocking) operations.
  //
  // Called on the CloudPrintProxyBackend core_thread_ to perform
  // initialization. When we are passed in an LSID we authenticate using that
  // and retrieve new auth tokens.
  void DoInitializeWithLsid(const std::string& lsid,
                            const std::string& last_robot_refresh_token,
                            const std::string& last_robot_email,
                            const std::string& last_user_email);

  void DoInitializeWithToken(const std::string& cloud_print_token);
  void DoInitializeWithRobotToken(const std::string& robot_oauth_refresh_token,
                                  const std::string& robot_email);
  void DoInitializeWithRobotAuthCode(const std::string& robot_oauth_auth_code,
                                     const std::string& robot_email);

  // Called on the CloudPrintProxyBackend core_thread_ to perform
  // shutdown.
  void DoShutdown();
  void DoRegisterSelectedPrinters(
      const printing::PrinterList& printer_list);
  void DoUnregisterPrinters();

  // CloudPrintAuth::Client implementation.
  virtual void OnAuthenticationComplete(
      const std::string& access_token,
      const std::string& robot_oauth_refresh_token,
      const std::string& robot_email,
      const std::string& user_email) OVERRIDE;
  virtual void OnInvalidCredentials() OVERRIDE;

  // CloudPrintConnector::Client implementation.
  virtual void OnAuthFailed() OVERRIDE;

  // notifier::PushClientObserver implementation.
  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      notifier::NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const notifier::Notification& notification) OVERRIDE;
  virtual void OnPingResponse() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;

  virtual ~Core() {}

  void CreateAuthAndConnector();
  void DestroyAuthAndConnector();

  // NotifyXXX is how the Core communicates with the frontend across
  // threads.
  void NotifyPrinterListAvailable(
      const printing::PrinterList& printer_list);
  void NotifyAuthenticated(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email);
  void NotifyAuthenticationFailed();
  void NotifyPrintSystemUnavailable();
  void NotifyUnregisterPrinters(const std::string& auth_token,
                                const std::list<std::string>& printer_ids);

  // Init XMPP channel
  void InitNotifications(const std::string& robot_email,
                         const std::string& access_token);

  void HandlePrinterNotification(const std::string& printer_id);
  void PollForJobs();
  // Schedules a task to poll for jobs. Does nothing if a task is already
  // scheduled.
  void ScheduleJobPoll();
  void PingXmppServer();
  void ScheduleXmppPing();
  void CheckXmppPingStatus();

  CloudPrintTokenStore* GetTokenStore();

  // Our parent CloudPrintProxyBackend
  CloudPrintProxyBackend* backend_;

  // Cloud Print authenticator.
  scoped_refptr<CloudPrintAuth> auth_;

  // Cloud Print connector.
  scoped_refptr<CloudPrintConnector> connector_;

  // OAuth client info.
  gaia::OAuthClientInfo oauth_client_info_;
  // Notification (xmpp) handler.
  scoped_ptr<notifier::PushClient> push_client_;
  // Indicates whether XMPP notifications are currently enabled.
  bool notifications_enabled_;
  // The time when notifications were enabled. Valid only when
  // notifications_enabled_ is true.
  base::TimeTicks notifications_enabled_since_;
  // Indicates whether a task to poll for jobs has been scheduled.
  bool job_poll_scheduled_;
  // Indicates whether we should poll for jobs when we lose XMPP connection.
  bool enable_job_poll_;
  // Indicates whether a task to ping xmpp server has been scheduled.
  bool xmpp_ping_scheduled_;
  // Number of XMPP pings pending reply from the server.
  int pending_xmpp_pings_;
  // Connector settings.
  ConnectorSettings settings_;
  std::string robot_email_;
  scoped_ptr<CloudPrintTokenStore> token_store_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

CloudPrintProxyBackend::CloudPrintProxyBackend(
    CloudPrintProxyFrontend* frontend,
    const ConnectorSettings& settings,
    const gaia::OAuthClientInfo& oauth_client_info,
    bool enable_job_poll)
    : core_thread_("Chrome_CloudPrintProxyCoreThread"),
      frontend_loop_(base::MessageLoop::current()),
      frontend_(frontend) {
  DCHECK(frontend_);
  core_ = new Core(this, settings, oauth_client_info, enable_job_poll);
}

CloudPrintProxyBackend::~CloudPrintProxyBackend() {
  DCHECK(!core_);
}

bool CloudPrintProxyBackend::InitializeWithLsid(
    const std::string& lsid,
    const std::string& last_robot_refresh_token,
    const std::string& last_robot_email,
    const std::string& last_user_email) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithLsid,
                 core_.get(), lsid, last_robot_refresh_token, last_robot_email,
                 last_user_email));
  return true;
}

bool CloudPrintProxyBackend::InitializeWithToken(
    const std::string& cloud_print_token) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithToken,
                 core_.get(), cloud_print_token));
  return true;
}

bool CloudPrintProxyBackend::InitializeWithRobotToken(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithRobotToken,
                 core_.get(), robot_oauth_refresh_token, robot_email));
  return true;
}

bool CloudPrintProxyBackend::InitializeWithRobotAuthCode(
    const std::string& robot_oauth_auth_code,
    const std::string& robot_email) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithRobotAuthCode,
                 core_.get(), robot_oauth_auth_code, robot_email));
  return true;
}

void CloudPrintProxyBackend::Shutdown() {
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoShutdown, core_.get()));
  core_thread_.Stop();
  core_ = NULL;  // Releases reference to core_.
}

void CloudPrintProxyBackend::UnregisterPrinters() {
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoUnregisterPrinters,
                 core_.get()));
}

CloudPrintProxyBackend::Core::Core(
    CloudPrintProxyBackend* backend,
    const ConnectorSettings& settings,
    const gaia::OAuthClientInfo& oauth_client_info,
    bool enable_job_poll)
      : backend_(backend),
        oauth_client_info_(oauth_client_info),
        notifications_enabled_(false),
        job_poll_scheduled_(false),
        enable_job_poll_(enable_job_poll),
        xmpp_ping_scheduled_(false),
        pending_xmpp_pings_(0) {
  settings_.CopyFrom(settings);
}

void CloudPrintProxyBackend::Core::CreateAuthAndConnector() {
  if (!auth_.get()) {
    auth_ = new CloudPrintAuth(this, settings_.server_url(), oauth_client_info_,
                               settings_.proxy_id());
  }

  if (!connector_.get()) {
    connector_ = new CloudPrintConnector(this, settings_);
  }
}

void CloudPrintProxyBackend::Core::DestroyAuthAndConnector() {
  auth_ = NULL;
  connector_ = NULL;
}

void CloudPrintProxyBackend::Core::DoInitializeWithLsid(
    const std::string& lsid,
    const std::string& last_robot_refresh_token,
    const std::string& last_robot_email,
    const std::string& last_user_email) {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  // Note: The GAIA login is synchronous but that should be OK because we are in
  // the CloudPrintProxyCoreThread and we cannot really do anything else until
  // the GAIA signin is successful.
  auth_->AuthenticateWithLsid(lsid, last_robot_refresh_token,
                              last_robot_email, last_user_email);
}

void CloudPrintProxyBackend::Core::DoInitializeWithToken(
    const std::string& cloud_print_token) {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  auth_->AuthenticateWithToken(cloud_print_token);
}

void CloudPrintProxyBackend::Core::DoInitializeWithRobotToken(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email) {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  auth_->AuthenticateWithRobotToken(robot_oauth_refresh_token, robot_email);
}

void CloudPrintProxyBackend::Core::DoInitializeWithRobotAuthCode(
    const std::string& robot_oauth_auth_code,
    const std::string& robot_email) {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  auth_->AuthenticateWithRobotAuthCode(robot_oauth_auth_code, robot_email);
}

void CloudPrintProxyBackend::Core::OnAuthenticationComplete(
    const std::string& access_token,
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email) {
  CloudPrintTokenStore* token_store  = GetTokenStore();
  bool first_time = token_store->token().empty();
  token_store->SetToken(access_token);
  robot_email_ = robot_email;
  // Let the frontend know that we have authenticated.
  backend_->frontend_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Core::NotifyAuthenticated, this, robot_oauth_refresh_token,
                 robot_email, user_email));
  if (first_time) {
    InitNotifications(robot_email, access_token);
  } else {
    // If we are refreshing a token, update the XMPP token too.
    DCHECK(push_client_.get());
    push_client_->UpdateCredentials(robot_email, access_token);
  }
  // Start cloud print connector if needed.
  if (!connector_->IsRunning()) {
    if (!connector_->Start()) {
      // Let the frontend know that we do not have a print system.
      backend_->frontend_loop_->PostTask(
          FROM_HERE, base::Bind(&Core::NotifyPrintSystemUnavailable, this));
    }
  }
}

void CloudPrintProxyBackend::Core::OnInvalidCredentials() {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Auth Error";
  backend_->frontend_loop_->PostTask(
      FROM_HERE, base::Bind(&Core::NotifyAuthenticationFailed, this));
}

void CloudPrintProxyBackend::Core::OnAuthFailed() {
  VLOG(1) << "CP_CONNECTOR: Authentication failed in connector.";
  // Let's stop connecter and refresh token. We'll restart connecter once
  // new token available.
  if (connector_->IsRunning())
    connector_->Stop();

  // Refresh Auth token.
  auth_->RefreshAccessToken();
}

void CloudPrintProxyBackend::Core::InitNotifications(
    const std::string& robot_email,
    const std::string& access_token) {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());

  pending_xmpp_pings_ = 0;
  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter =
      g_service_process->GetServiceURLRequestContextGetter();
  notifier_options.auth_mechanism = "X-OAUTH2";
  notifier_options.try_ssltcp_first = true;
  push_client_ = notifier::PushClient::CreateDefault(notifier_options);
  push_client_->AddObserver(this);
  notifier::Subscription subscription;
  subscription.channel = kCloudPrintPushNotificationsSource;
  subscription.from = kCloudPrintPushNotificationsSource;
  push_client_->UpdateSubscriptions(
      notifier::SubscriptionList(1, subscription));
  push_client_->UpdateCredentials(robot_email, access_token);
}

void CloudPrintProxyBackend::Core::DoShutdown() {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Shutdown connector, id: " << settings_.proxy_id();

  if (connector_->IsRunning())
    connector_->Stop();

  // Important to delete the PushClient on this thread.
  if (push_client_.get()) {
    push_client_->RemoveObserver(this);
  }
  push_client_.reset();
  notifications_enabled_ = false;
  notifications_enabled_since_ = base::TimeTicks();
  token_store_.reset();

  DestroyAuthAndConnector();
}

void CloudPrintProxyBackend::Core::DoUnregisterPrinters() {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());

  std::string access_token = GetTokenStore()->token();

  std::list<std::string> printer_ids;
  connector_->GetPrinterIds(&printer_ids);

  backend_->frontend_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Core::NotifyUnregisterPrinters,
                 this, access_token, printer_ids));
}

void CloudPrintProxyBackend::Core::HandlePrinterNotification(
    const std::string& printer_id) {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Handle printer notification, id: " << printer_id;
  connector_->CheckForJobs(kJobFetchReasonNotified, printer_id);
}

void CloudPrintProxyBackend::Core::PollForJobs() {
  VLOG(1) << "CP_CONNECTOR: Polling for jobs.";
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  // Check all printers for jobs.
  connector_->CheckForJobs(kJobFetchReasonPoll, std::string());

  job_poll_scheduled_ = false;
  // If we don't have notifications and job polling is enabled, poll again
  // after a while.
  if (!notifications_enabled_ && enable_job_poll_)
    ScheduleJobPoll();
}

void CloudPrintProxyBackend::Core::ScheduleJobPoll() {
  if (!job_poll_scheduled_) {
    base::TimeDelta interval = base::TimeDelta::FromSeconds(
        base::RandInt(kMinJobPollIntervalSecs, kMaxJobPollIntervalSecs));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloudPrintProxyBackend::Core::PollForJobs, this),
        interval);
    job_poll_scheduled_ = true;
  }
}

void CloudPrintProxyBackend::Core::PingXmppServer() {
  xmpp_ping_scheduled_ = false;

  if (!push_client_.get())
    return;

  push_client_->SendPing();

  pending_xmpp_pings_++;
  if (pending_xmpp_pings_ >= kMaxFailedXmppPings) {
    // Check ping status when we close to the limit.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloudPrintProxyBackend::Core::CheckXmppPingStatus, this),
        base::TimeDelta::FromSeconds(kXmppPingCheckIntervalSecs));
  }

  // Schedule next ping if needed.
  if (notifications_enabled_)
    ScheduleXmppPing();
}

void CloudPrintProxyBackend::Core::ScheduleXmppPing() {
  if (!settings_.xmpp_ping_enabled())
    return;

  if (!xmpp_ping_scheduled_) {
    base::TimeDelta interval = base::TimeDelta::FromSeconds(
      base::RandInt(settings_.xmpp_ping_timeout_sec() * 0.9,
                    settings_.xmpp_ping_timeout_sec() * 1.1));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloudPrintProxyBackend::Core::PingXmppServer, this),
        interval);
    xmpp_ping_scheduled_ = true;
  }
}

void CloudPrintProxyBackend::Core::CheckXmppPingStatus() {
  if (pending_xmpp_pings_ >= kMaxFailedXmppPings) {
    // Reconnect to XMPP.
    pending_xmpp_pings_ = 0;
    push_client_.reset();
    InitNotifications(robot_email_, GetTokenStore()->token());
  }
}

CloudPrintTokenStore* CloudPrintProxyBackend::Core::GetTokenStore() {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  if (!token_store_.get())
    token_store_.reset(new CloudPrintTokenStore);
  return token_store_.get();
}

void CloudPrintProxyBackend::Core::NotifyAuthenticated(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email) {
  DCHECK(base::MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_
      ->OnAuthenticated(robot_oauth_refresh_token, robot_email, user_email);
}

void CloudPrintProxyBackend::Core::NotifyAuthenticationFailed() {
  DCHECK(base::MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnAuthenticationFailed();
}

void CloudPrintProxyBackend::Core::NotifyPrintSystemUnavailable() {
  DCHECK(base::MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnPrintSystemUnavailable();
}

void CloudPrintProxyBackend::Core::NotifyUnregisterPrinters(
    const std::string& auth_token,
    const std::list<std::string>& printer_ids) {
  DCHECK(base::MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnUnregisterPrinters(auth_token, printer_ids);
}

void CloudPrintProxyBackend::Core::OnNotificationsEnabled() {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  notifications_enabled_ = true;
  notifications_enabled_since_ = base::TimeTicks::Now();
  VLOG(1) << "Notifications for connector " << settings_.proxy_id()
          << " were enabled at "
          << notifications_enabled_since_.ToInternalValue();
  // Notifications just got re-enabled. In this case we want to schedule
  // a poll once for jobs we might have missed when we were dark.
  // Note that ScheduleJobPoll will not schedule again if a job poll task is
  // already scheduled.
  ScheduleJobPoll();

  // Schedule periodic ping for XMPP notification channel.
  ScheduleXmppPing();
}

void CloudPrintProxyBackend::Core::OnNotificationsDisabled(
    notifier::NotificationsDisabledReason reason) {
  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  notifications_enabled_ = false;
  LOG(ERROR) << "Notifications for connector " << settings_.proxy_id()
             << " disabled.";
  notifications_enabled_since_ = base::TimeTicks();
  // We just lost notifications. This this case we want to schedule a
  // job poll if enable_job_poll_ is true.
  if (enable_job_poll_)
    ScheduleJobPoll();
}


void CloudPrintProxyBackend::Core::OnIncomingNotification(
    const notifier::Notification& notification) {
  // Since we got some notification from the server,
  // reset pending ping counter to 0.
  pending_xmpp_pings_ = 0;

  DCHECK(base::MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Incoming notification.";
  if (0 == base::strcasecmp(kCloudPrintPushNotificationsSource,
                            notification.channel.c_str()))
    HandlePrinterNotification(notification.data);
}

void CloudPrintProxyBackend::Core::OnPingResponse() {
  pending_xmpp_pings_ = 0;
  VLOG(1) << "CP_CONNECTOR: Ping response received.";
}

}  // namespace cloud_print
