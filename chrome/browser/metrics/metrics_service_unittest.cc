// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service.h"

#include <string>

#include "base/bind.h"
#include "base/prefs/testing_pref_service.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service_observer.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/variations/metrics_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif  // defined(OS_CHROMEOS)

namespace {

using metrics::MetricsLogManager;

class TestMetricsService : public MetricsService {
 public:
  TestMetricsService(metrics::MetricsStateManager* state_manager,
                     metrics::MetricsServiceClient* client,
                     PrefService* local_state)
      : MetricsService(state_manager, client, local_state) {}
  virtual ~TestMetricsService() {}

  using MetricsService::log_manager;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricsService);
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 metrics::MetricsServiceClient* client,
                 PrefService* local_state)
      : MetricsLog(client_id,
                   session_id,
                   MetricsLog::ONGOING_LOG,
                   client,
                   local_state) {}

  virtual ~TestMetricsLog() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

class MetricsServiceTest : public testing::Test {
 public:
  MetricsServiceTest() : is_metrics_reporting_enabled_(false) {
    MetricsService::RegisterPrefs(testing_local_state_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        GetLocalState(),
        base::Bind(&MetricsServiceTest::is_metrics_reporting_enabled,
                   base::Unretained(this)));
#if defined(OS_CHROMEOS)
    // TODO(blundell): Remove this code once MetricsService no longer creates
    // ChromeOSMetricsProvider. Also remove the #include of login_state.h.
    // (http://crbug.com/375776)
    if (!chromeos::LoginState::IsInitialized())
      chromeos::LoginState::Initialize();
#endif  // defined(OS_CHROMEOS)
  }

  virtual ~MetricsServiceTest() {
    MetricsService::SetExecutionPhase(MetricsService::UNINITIALIZED_PHASE,
                                      GetLocalState());
#if defined(OS_CHROMEOS)
    // TODO(blundell): Remove this code once MetricsService no longer creates
    // ChromeOSMetricsProvider.
    chromeos::LoginState::Shutdown();
#endif  // defined(OS_CHROMEOS)
  }

  metrics::MetricsStateManager* GetMetricsStateManager() {
    return metrics_state_manager_.get();
  }

  PrefService* GetLocalState() { return &testing_local_state_; }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    is_metrics_reporting_enabled_ = true;
  }

  // Waits until base::TimeTicks::Now() no longer equals |value|. This should
  // take between 1-15ms per the documented resolution of base::TimeTicks.
  void WaitUntilTimeChanges(const base::TimeTicks& value) {
    while (base::TimeTicks::Now() == value) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
  }

  // Returns true if there is a synthetic trial in the given vector that matches
  // the given trial name and trial group; returns false otherwise.
  bool HasSyntheticTrial(
      const std::vector<variations::ActiveGroupId>& synthetic_trials,
      const std::string& trial_name,
      const std::string& trial_group) {
    uint32 trial_name_hash = metrics::HashName(trial_name);
    uint32 trial_group_hash = metrics::HashName(trial_group);
    for (std::vector<variations::ActiveGroupId>::const_iterator it =
             synthetic_trials.begin();
         it != synthetic_trials.end(); ++it) {
      if ((*it).name == trial_name_hash && (*it).group == trial_group_hash)
        return true;
    }
    return false;
  }

 private:
  bool is_metrics_reporting_enabled() const {
    return is_metrics_reporting_enabled_;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  bool is_metrics_reporting_enabled_;
  TestingPrefServiceSimple testing_local_state_;
  scoped_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceTest);
};

class TestMetricsServiceObserver : public MetricsServiceObserver {
 public:
  TestMetricsServiceObserver(): observed_(0) {}
  virtual ~TestMetricsServiceObserver() {}

  virtual void OnDidCreateMetricsLog() OVERRIDE {
    ++observed_;
  }
  int observed() const { return observed_; }

 private:
  int observed_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsServiceObserver);
};

}  // namespace

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCleanShutDown) {
  EnableMetricsReporting();
  GetLocalState()->SetBoolean(metrics::prefs::kStabilityExitedCleanly, true);

  metrics::TestMetricsServiceClient client;
  TestMetricsService service(
      GetMetricsStateManager(), &client, GetLocalState());
  service.InitializeMetricsRecordingState();
  // No initial stability log should be generated.
  EXPECT_FALSE(service.log_manager()->has_unsent_logs());
  EXPECT_FALSE(service.log_manager()->has_staged_log());
}

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCrash) {
  // TODO(asvitkine): Eliminate using |testing_local_state| in favor of using
  // |GetLocalState()| once MetricsService no longer internally creates metrics
  // providers that rely on g_browser_process->local_state() being correctly
  // set up. crbug.com/375776.
  ScopedTestingLocalState testing_local_state(
      TestingBrowserProcess::GetGlobal());
  TestingPrefServiceSimple* local_state = testing_local_state.Get();
  EnableMetricsReporting();
  local_state->ClearPref(metrics::prefs::kStabilityExitedCleanly);

  // Set up prefs to simulate restarting after a crash.

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log("client", 1, &client, local_state);
  log.RecordEnvironment(std::vector<metrics::MetricsProvider*>(),
                        std::vector<variations::ActiveGroupId>());

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  local_state->SetInt64(metrics::prefs::kStabilityStatsBuildTime,
                        MetricsLog::GetBuildTime());
  local_state->SetString(metrics::prefs::kStabilityStatsVersion,
                         client.GetVersionString());

  local_state->SetBoolean(metrics::prefs::kStabilityExitedCleanly, false);

  TestMetricsService service(GetMetricsStateManager(), &client, local_state);
  service.InitializeMetricsRecordingState();

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogManager* log_manager = service.log_manager();
  EXPECT_TRUE(log_manager->has_unsent_logs());
  EXPECT_FALSE(log_manager->has_staged_log());

  // Stage the log and retrieve it.
  log_manager->StageNextLogForUpload();
  EXPECT_TRUE(log_manager->has_staged_log());

  metrics::ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(uma_log.ParseFromString(log_manager->staged_log()));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.histogram_event_size());
  EXPECT_EQ(0, uma_log.profiler_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());

  EXPECT_EQ(1, uma_log.system_profile().stability().crash_count());
}

TEST_F(MetricsServiceTest, RegisterSyntheticTrial) {
  metrics::TestMetricsServiceClient client;
  MetricsService service(GetMetricsStateManager(), &client, GetLocalState());

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup trial1(metrics::HashName("TestTrial1"),
                             metrics::HashName("Group1"));
  service.RegisterSyntheticFieldTrial(trial1);

  SyntheticTrialGroup trial2(metrics::HashName("TestTrial2"),
                             metrics::HashName("Group2"));
  service.RegisterSyntheticFieldTrial(trial2);
  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  service.log_manager_.BeginLoggingWithLog(scoped_ptr<MetricsLog>(
      new MetricsLog("clientID",
                     1,
                     MetricsLog::INITIAL_STABILITY_LOG,
                     &client,
                     GetLocalState())));
  // Save the time when the log was started (it's okay for this to be greater
  // than the time recorded by the above call since it's used to ensure the
  // value changes).
  const base::TimeTicks begin_log_time = base::TimeTicks::Now();

  std::vector<variations::ActiveGroupId> synthetic_trials;
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group1"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(begin_log_time);

  // Change the group for the first trial after the log started.
  SyntheticTrialGroup trial3(metrics::HashName("TestTrial1"),
                             metrics::HashName("Group2"));
  service.RegisterSyntheticFieldTrial(trial3);
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Add a new trial after the log started and confirm that it doesn't show up.
  SyntheticTrialGroup trial4(metrics::HashName("TestTrial3"),
                             metrics::HashName("Group3"));
  service.RegisterSyntheticFieldTrial(trial4);
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Start a new log and ensure all three trials appear in it.
  service.log_manager_.FinishCurrentLog();
  service.log_manager_.BeginLoggingWithLog(
      scoped_ptr<MetricsLog>(new MetricsLog(
          "clientID", 1, MetricsLog::ONGOING_LOG, &client, GetLocalState())));
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(3U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial3", "Group3"));
  service.log_manager_.FinishCurrentLog();
}

TEST_F(MetricsServiceTest, MetricsServiceObserver) {
  metrics::TestMetricsServiceClient client;
  MetricsService service(GetMetricsStateManager(), &client, GetLocalState());
  TestMetricsServiceObserver observer1;
  TestMetricsServiceObserver observer2;

  service.AddObserver(&observer1);
  EXPECT_EQ(0, observer1.observed());
  EXPECT_EQ(0, observer2.observed());

  service.OpenNewLog();
  EXPECT_EQ(1, observer1.observed());
  EXPECT_EQ(0, observer2.observed());
  service.log_manager_.FinishCurrentLog();

  service.AddObserver(&observer2);

  service.OpenNewLog();
  EXPECT_EQ(2, observer1.observed());
  EXPECT_EQ(1, observer2.observed());
  service.log_manager_.FinishCurrentLog();

  service.RemoveObserver(&observer1);

  service.OpenNewLog();
  EXPECT_EQ(2, observer1.observed());
  EXPECT_EQ(2, observer2.observed());
  service.log_manager_.FinishCurrentLog();

  service.RemoveObserver(&observer2);
}
