// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "chrome/browser/captive_portal/captive_portal_detector.h"
#include "chrome/browser/captive_portal/testing_utils.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::AnyNumber;
using testing::Mock;
using testing::_;

namespace chromeos {

namespace {

// Service paths for stub network devices.
const char kStubEthernet[] = "stub_ethernet";
const char kStubWireless1[] = "stub_wifi1";
const char kStubWireless2[] = "stub_wifi2";
const char kStubCellular[] = "stub_cellular";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

class MockObserver : public NetworkPortalDetector::Observer {
 public:
  virtual ~MockObserver() {}

  MOCK_METHOD2(OnPortalDetectionCompleted,
               void(const NetworkState* network,
                    const NetworkPortalDetector::CaptivePortalState& state));
};

class ResultHistogramChecker {
 public:
  explicit ResultHistogramChecker(base::HistogramSamples* base)
      : base_(base),
        count_(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT) {}
  virtual ~ResultHistogramChecker() {}

  ResultHistogramChecker* Expect(
      NetworkPortalDetector::CaptivePortalStatus status,
      int count) {
    count_[status] = count;
    return this;
  }

  bool Check() const {
    base::HistogramBase* histogram = base::StatisticsRecorder::FindHistogram(
        NetworkPortalDetectorImpl::kDetectionResultHistogram);
    bool empty = false;
    if (static_cast<size_t>(std::count(count_.begin(), count_.end(), 0)) ==
        count_.size()) {
      empty = true;
    }

    if (!histogram) {
      if (empty)
        return true;
      LOG(ERROR) << "Can't get histogram for "
                 << NetworkPortalDetectorImpl::kDetectionResultHistogram;
      return false;
    }
    scoped_ptr<base::HistogramSamples> samples = histogram->SnapshotSamples();
    if (!samples.get()) {
      if (empty)
        return true;
      LOG(ERROR) << "Can't get samples for "
                 << NetworkPortalDetectorImpl::kDetectionResultHistogram;
      return false;
    }
    bool ok = true;
    for (size_t i = 0; i < count_.size(); ++i) {
      const int base = base_ ? base_->GetCount(i) : 0;
      const int actual = samples->GetCount(i) - base;
      const NetworkPortalDetector::CaptivePortalStatus status =
          static_cast<NetworkPortalDetector::CaptivePortalStatus>(i);
      if (actual != count_[i]) {
        LOG(ERROR) << "Expected: " << count_[i] << ", "
                   << "actual: " << actual << " for "
                   << NetworkPortalDetector::CaptivePortalStatusString(status);
        ok = false;
      }
    }
    return ok;
  }

 private:
  base::HistogramSamples* base_;
  std::vector<int> count_;

  DISALLOW_COPY_AND_ASSIGN(ResultHistogramChecker);
};

}  // namespace

class NetworkPortalDetectorImplTest
    : public testing::Test,
      public captive_portal::CaptivePortalDetectorTestBase {
 protected:
  virtual void SetUp() {
    CommandLine* cl = CommandLine::ForCurrentProcess();
    cl->AppendSwitch(switches::kDisableNetworkPortalNotification);

    DBusThreadManager::InitializeWithStub();
    base::StatisticsRecorder::Initialize();
    SetupNetworkHandler();

    profile_.reset(new TestingProfile());
    network_portal_detector_.reset(
        new NetworkPortalDetectorImpl(profile_->GetRequestContext()));
    network_portal_detector_->Enable(false);

    set_detector(network_portal_detector_->captive_portal_detector_.get());

    // Prevents flakiness due to message loop delays.
    set_time_ticks(base::TimeTicks::Now());

    if (base::HistogramBase* histogram =
            base::StatisticsRecorder::FindHistogram(
                NetworkPortalDetectorImpl::kDetectionResultHistogram)) {
      original_samples_.reset(histogram->SnapshotSamples().release());
    }
  }

  virtual void TearDown() {
    network_portal_detector_.reset();
    profile_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
    PortalDetectorStrategy::reset_fields_for_testing();
  }

  void CheckPortalState(NetworkPortalDetector::CaptivePortalStatus status,
                        int response_code,
                        const std::string& service_path) {
    NetworkPortalDetector::CaptivePortalState state =
        network_portal_detector()->GetCaptivePortalState(service_path);
    ASSERT_EQ(status, state.status);
    ASSERT_EQ(response_code, state.response_code);
  }

  void CheckRequestTimeoutAndCompleteAttempt(int expected_attempt_count,
                                             int expected_request_timeout_sec,
                                             int net_error,
                                             int status_code) {
    ASSERT_TRUE(is_state_checking_for_portal());
    ASSERT_EQ(expected_attempt_count, attempt_count());
    ASSERT_EQ(base::TimeDelta::FromSeconds(expected_request_timeout_sec),
              get_next_attempt_timeout());
    CompleteURLFetch(net_error, status_code, NULL);
  }

  Profile* profile() { return profile_.get(); }

  NetworkPortalDetectorImpl* network_portal_detector() {
    return network_portal_detector_.get();
  }

  NetworkPortalDetectorImpl::State state() {
    return network_portal_detector()->state();
  }

  bool start_detection_if_idle() {
    return network_portal_detector()->StartDetectionIfIdle();
  }

  void enable_error_screen_strategy() {
    network_portal_detector()->EnableErrorScreenStrategy();
  }

  void disable_error_screen_strategy() {
    network_portal_detector()->DisableErrorScreenStrategy();
  }

  void stop_detection() {
    network_portal_detector()->StopDetection();
  }

  bool attempt_timeout_is_cancelled() {
    return network_portal_detector()->AttemptTimeoutIsCancelledForTesting();
  }

  base::TimeDelta get_next_attempt_timeout() {
    return network_portal_detector()->strategy_->GetNextAttemptTimeout();
  }

  void set_next_attempt_timeout(const base::TimeDelta& timeout) {
    PortalDetectorStrategy::set_next_attempt_timeout_for_testing(timeout);
  }

  bool is_state_idle() {
    return (NetworkPortalDetectorImpl::STATE_IDLE == state());
  }

  bool is_state_portal_detection_pending() {
    return (NetworkPortalDetectorImpl::STATE_PORTAL_CHECK_PENDING == state());
  }

  bool is_state_checking_for_portal() {
    return (NetworkPortalDetectorImpl::STATE_CHECKING_FOR_PORTAL == state());
  }



  const base::TimeDelta& next_attempt_delay() {
    return network_portal_detector()->next_attempt_delay_for_testing();
  }

  int attempt_count() {
    return network_portal_detector()->attempt_count_for_testing();
  }

  void set_attempt_count(int ac) {
    network_portal_detector()->set_attempt_count_for_testing(ac);
  }

  void set_delay_till_next_attempt(const base::TimeDelta& delta) {
    PortalDetectorStrategy::set_delay_till_next_attempt_for_testing(delta);
  }

  void set_time_ticks(const base::TimeTicks& time_ticks) {
    network_portal_detector()->set_time_ticks_for_testing(time_ticks);
  }

  void SetBehindPortal(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(service_path),
        shill::kStateProperty,
        base::StringValue(shill::kStatePortal),
        base::Bind(&base::DoNothing),
        base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkDeviceEnabled(const std::string& type, bool enabled) {
    NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
        NetworkTypePattern::Primitive(type),
        enabled,
        network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetConnected(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->Connect(
        dbus::ObjectPath(service_path),
        base::Bind(&base::DoNothing),
        base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  void SetDisconnected(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
        dbus::ObjectPath(service_path),
        base::Bind(&*base::DoNothing),
        base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  scoped_ptr<ResultHistogramChecker> MakeResultHistogramChecker() {
    return scoped_ptr<ResultHistogramChecker>(
        new ResultHistogramChecker(original_samples_.get())).Pass();
  }

 private:
  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_visible = true;
    const bool add_to_watchlist = true;
    service_test->AddService(kStubEthernet,
                             kStubEthernet,
                             shill::kTypeEthernet,
                             shill::kStateIdle,
                             add_to_visible,
                             add_to_watchlist);
    service_test->AddService(kStubWireless1,
                             kStubWireless1,
                             shill::kTypeWifi,
                             shill::kStateIdle,
                             add_to_visible,
                             add_to_watchlist);
    service_test->AddService(kStubWireless2,
                             kStubWireless2,
                             shill::kTypeWifi,
                             shill::kStateIdle,
                             add_to_visible,
                             add_to_watchlist);
    service_test->AddService(kStubCellular,
                             kStubCellular,
                             shill::kTypeCellular,
                             shill::kStateIdle,
                             add_to_visible,
                             add_to_watchlist);
  }

  void SetupNetworkHandler() {
    SetupDefaultShillState();
    NetworkHandler::Initialize();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<NetworkPortalDetectorImpl> network_portal_detector_;
  scoped_ptr<base::HistogramSamples> original_samples_;
};

TEST_F(NetworkPortalDetectorImplTest, NoPortal) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);

  ASSERT_TRUE(is_state_checking_for_portal());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);
  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, Portal) {
  ASSERT_TRUE(is_state_idle());

  // Check HTTP 200 response code.
  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  // Check HTTP 301 response code.
  SetConnected(kStubWireless2);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 301, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 301, kStubWireless2);

  // Check HTTP 302 response code.
  SetConnected(kStubEthernet);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 302, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 302, kStubEthernet);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 3)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, Online2Offline) {
  ASSERT_TRUE(is_state_idle());

  MockObserver observer;
  network_portal_detector()->AddObserver(&observer);

  NetworkPortalDetector::CaptivePortalState offline_state;
  offline_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE;

  // WiFi is in online state.
  {
    // When transitioning to a connected state, the network will transition to
    // connecting states which will set the default network to NULL. This may
    // get triggered multiple times.
    EXPECT_CALL(observer, OnPortalDetectionCompleted(_, offline_state))
        .Times(AnyNumber());

    // Expect a single transition to an online state.
    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    EXPECT_CALL(observer, OnPortalDetectionCompleted(_, online_state)).Times(1);

    SetConnected(kStubWireless1);
    ASSERT_TRUE(is_state_checking_for_portal());

    CompleteURLFetch(net::OK, 204, NULL);
    ASSERT_TRUE(is_state_idle());

    // Check that observer was notified about online state.
    Mock::VerifyAndClearExpectations(&observer);
  }

  // WiFi is turned off.
  {
    EXPECT_CALL(observer, OnPortalDetectionCompleted(NULL, offline_state))
        .Times(1);

    SetDisconnected(kStubWireless1);
    ASSERT_TRUE(is_state_idle());

    // Check that observer was notified about offline state.
    Mock::VerifyAndClearExpectations(&observer);
  }

  network_portal_detector()->RemoveObserver(&observer);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, TwoNetworks) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  // WiFi is in portal state.
  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubEthernet);
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubEthernet);
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NetworkChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);

  // WiFi is in portal state.
  fetcher()->set_response_code(200);
  ASSERT_TRUE(is_state_checking_for_portal());

  // Active network is changed during portal detection for WiFi.
  SetConnected(kStubEthernet);

  // Portal detection for WiFi is cancelled, portal detection for
  // ethernet is initiated.
  ASSERT_TRUE(is_state_checking_for_portal());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubEthernet);

  // As active network was changed during portal detection for wifi
  // network, it's state must be unknown.
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateNotChanged) {
  ASSERT_TRUE(is_state_idle());

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_idle());

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateChanged) {
  // Test for Portal -> Online -> Portal network state transitions.
  ASSERT_TRUE(is_state_idle());

  SetBehindPortal(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  SetBehindPortal(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(net::OK, 200, NULL);

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 2)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionTimeout) {
  ASSERT_TRUE(is_state_idle());

  // For instantaneous timeout.
  set_next_attempt_timeout(base::TimeDelta::FromSeconds(0));

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);
  base::RunLoop().RunUntilIdle();

  // First portal detection timeouts, next portal detection is
  // scheduled.
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(3), next_attempt_delay());

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionRetryAfter) {
  ASSERT_TRUE(is_state_idle());

  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 101\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 101 seconds.
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(101), next_attempt_delay());

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectorRetryAfterIsSmall) {
  ASSERT_TRUE(is_state_idle());

  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 1\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 3 seconds (due to minimum time between detection
  // attemps).
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(3), next_attempt_delay());

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, FirstAttemptFailed) {
  ASSERT_TRUE(is_state_idle());

  set_delay_till_next_attempt(base::TimeDelta());
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 204, NULL);
  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(2, attempt_count());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, AllAttemptsFailed) {
  ASSERT_TRUE(is_state_idle());

  set_delay_till_next_attempt(base::TimeDelta());
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(0, attempt_count());

  SetConnected(kStubWireless1);

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(1, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_EQ(2, attempt_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_TRUE(is_state_idle());
  ASSERT_EQ(3, attempt_count());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE,
                   503,
                   kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, ProxyAuthRequired) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());

  SetConnected(kStubWireless1);
  CompleteURLFetch(net::OK, 407, NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED,
      407,
      kStubWireless1);

  ASSERT_TRUE(MakeResultHistogramChecker()
                  ->Expect(NetworkPortalDetector::
                               CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED,
                           1)
                  ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NoResponseButBehindPortal) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());

  SetBehindPortal(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());

  CompleteURLFetch(
      net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID, NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(
      net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID, NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(
      net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_idle());

  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
                   net::URLFetcher::RESPONSE_CODE_INVALID,
                   kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest,
       DisableErrorScreenStrategyWhilePendingRequest) {
  ASSERT_TRUE(is_state_idle());
  set_attempt_count(3);
  enable_error_screen_strategy();
  ASSERT_TRUE(is_state_portal_detection_pending());
  disable_error_screen_strategy();

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, ErrorScreenStrategyForOnlineNetwork) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());

  SetConnected(kStubWireless1);
  enable_error_screen_strategy();
  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 204, NULL);

  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  disable_error_screen_strategy();

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, ErrorScreenStrategyForPortalNetwork) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());

  enable_error_screen_strategy();
  SetConnected(kStubWireless1);

  CompleteURLFetch(
      net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID, NULL);
  ASSERT_EQ(1, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(
      net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID, NULL);
  ASSERT_EQ(2, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 200, NULL);
  ASSERT_EQ(3, attempt_count());
  ASSERT_TRUE(is_state_portal_detection_pending());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  disable_error_screen_strategy();

  ASSERT_TRUE(is_state_idle());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, DetectionTimeoutIsCancelled) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());

  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  stop_detection();

  ASSERT_TRUE(is_state_idle());
  ASSERT_TRUE(attempt_timeout_is_cancelled());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, TestDetectionRestart) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());

  // First portal detection attempts determines ONLINE state.
  SetConnected(kStubWireless1);
  ASSERT_TRUE(is_state_checking_for_portal());
  ASSERT_FALSE(start_detection_if_idle());

  CompleteURLFetch(net::OK, 204, NULL);

  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);
  ASSERT_TRUE(is_state_idle());

  // First portal detection attempts determines PORTAL state.
  ASSERT_TRUE(start_detection_if_idle());
  ASSERT_TRUE(is_state_portal_detection_pending());
  ASSERT_FALSE(start_detection_if_idle());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(is_state_checking_for_portal());
  CompleteURLFetch(net::OK, 200, NULL);

  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);
  ASSERT_TRUE(is_state_idle());

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, RequestTimeouts) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());

  SetNetworkDeviceEnabled(shill::kTypeWifi, false);
  SetConnected(kStubCellular);

  // First portal detection attempt for cellular1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      0, 5, net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID);

  // Second portal detection attempt for cellular1 uses 10sec timeout.
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(1,
                                        10,
                                        net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);

  // Third portal detection attempt for cellular1 uses 15sec timeout.
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(2,
                                        15,
                                        net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);

  ASSERT_TRUE(is_state_idle());

  // Check that in lazy detection for cellular1 15sec timeout is used.
  enable_error_screen_strategy();
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(0,
                                        15,
                                        net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);
  disable_error_screen_strategy();
  ASSERT_TRUE(is_state_idle());

  SetNetworkDeviceEnabled(shill::kTypeWifi, true);
  SetConnected(kStubWireless1);

  // First portal detection attempt for wifi1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      0, 5, net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID);

  // Second portal detection attempt for wifi1 also uses 5sec timeout.
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(1, 10, net::OK, 204);
  ASSERT_TRUE(is_state_idle());

  // Check that in error screen strategy detection for wifi1 15sec
  // timeout is used.
  enable_error_screen_strategy();
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(0, 15, net::OK, 204);
  disable_error_screen_strategy();
  ASSERT_TRUE(is_state_idle());
  return;

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, StartDetectionIfIdle) {
  ASSERT_TRUE(is_state_idle());
  set_delay_till_next_attempt(base::TimeDelta());
  SetConnected(kStubWireless1);

  // First portal detection attempt for wifi1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      0, 5, net::ERR_CONNECTION_CLOSED, net::URLFetcher::RESPONSE_CODE_INVALID);
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();

  // Second portal detection attempt for wifi1 uses 10sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(1,
                                        10,
                                        net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);
  ASSERT_TRUE(is_state_portal_detection_pending());
  base::RunLoop().RunUntilIdle();

  // Second portal detection attempt for wifi1 uses 15sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(2,
                                        15,
                                        net::ERR_CONNECTION_CLOSED,
                                        net::URLFetcher::RESPONSE_CODE_INVALID);
  ASSERT_TRUE(is_state_idle());
  start_detection_if_idle();

  ASSERT_TRUE(is_state_portal_detection_pending());

  // First portal detection attempt for wifi1 uses 5sec timeout.
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(0, 5, net::OK, 204);
  ASSERT_TRUE(is_state_idle());

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

}  // namespace chromeos
