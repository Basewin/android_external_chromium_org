// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_state_manager.h"

#include "base/command_line.h"
#include "base/guid.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/metrics/cloned_install_detector.h"
#include "components/metrics/machine_id_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_switches.h"
#include "components/variations/caching_permuted_entropy_provider.h"

namespace metrics {

namespace {

// The argument used to generate a non-identifying entropy source. We want no
// more than 13 bits of entropy, so use this max to return a number in the range
// [0, 7999] as the entropy source (12.97 bits of entropy).
const int kMaxLowEntropySize = 8000;

// Default prefs value for prefs::kMetricsLowEntropySource to indicate that
// the value has not yet been set.
const int kLowEntropySourceNotSet = -1;

// Generates a new non-identifying entropy source used to seed persistent
// activities.
int GenerateLowEntropySource() {
  return base::RandInt(0, kMaxLowEntropySize - 1);
}

}  // namespace

// static
bool MetricsStateManager::instance_exists_ = false;

MetricsStateManager::MetricsStateManager(
    PrefService* local_state,
    const base::Callback<bool(void)>& is_reporting_enabled_callback,
    const StoreClientInfoCallback& store_client_info,
    const LoadClientInfoCallback& retrieve_client_info)
    : local_state_(local_state),
      is_reporting_enabled_callback_(is_reporting_enabled_callback),
      store_client_info_(store_client_info),
      load_client_info_(retrieve_client_info),
      low_entropy_source_(kLowEntropySourceNotSet),
      entropy_source_returned_(ENTROPY_SOURCE_NONE) {
  ResetMetricsIDsIfNecessary();
  if (IsMetricsReportingEnabled())
    ForceClientIdCreation();

  DCHECK(!instance_exists_);
  instance_exists_ = true;
}

MetricsStateManager::~MetricsStateManager() {
  DCHECK(instance_exists_);
  instance_exists_ = false;
}

bool MetricsStateManager::IsMetricsReportingEnabled() {
  return is_reporting_enabled_callback_.Run();
}

void MetricsStateManager::ForceClientIdCreation() {
  if (!client_id_.empty())
    return;

  client_id_ = local_state_->GetString(prefs::kMetricsClientID);
  if (!client_id_.empty()) {
    // It is technically sufficient to only save a backup of the client id when
    // it is initially generated below, but since the backup was only introduced
    // in M38, seed it explicitly from here for some time.
    BackUpCurrentClientInfo();
    return;
  }

  const scoped_ptr<ClientInfo> client_info_backup =
      LoadClientInfoAndMaybeMigrate();
  if (client_info_backup) {
    client_id_ = client_info_backup->client_id;

    const base::Time now = base::Time::Now();

    // Save the recovered client id and also try to reinstantiate the backup
    // values for the dates corresponding with that client id in order to avoid
    // weird scenarios where we could report an old client id with a recent
    // install date.
    local_state_->SetString(prefs::kMetricsClientID, client_id_);
    local_state_->SetInt64(prefs::kInstallDate,
                           client_info_backup->installation_date != 0
                               ? client_info_backup->installation_date
                               : now.ToTimeT());
    local_state_->SetInt64(prefs::kMetricsReportingEnabledTimestamp,
                           client_info_backup->reporting_enabled_date != 0
                               ? client_info_backup->reporting_enabled_date
                               : now.ToTimeT());

    base::TimeDelta recovered_installation_age;
    if (client_info_backup->installation_date != 0) {
      recovered_installation_age =
          now - base::Time::FromTimeT(client_info_backup->installation_date);
    }
    UMA_HISTOGRAM_COUNTS_10000("UMA.ClientIdBackupRecoveredWithAge",
                               recovered_installation_age.InHours());

    // Flush the backup back to persistent storage in case we re-generated
    // missing data above.
    BackUpCurrentClientInfo();
    return;
  }

  // Failing attempts at getting an existing client ID, generate a new one.
  client_id_ = base::GenerateGUID();
  local_state_->SetString(prefs::kMetricsClientID, client_id_);

  if (local_state_->GetString(prefs::kMetricsOldClientID).empty()) {
    // Record the timestamp of when the user opted in to UMA.
    local_state_->SetInt64(prefs::kMetricsReportingEnabledTimestamp,
                           base::Time::Now().ToTimeT());
  } else {
    UMA_HISTOGRAM_BOOLEAN("UMA.ClientIdMigrated", true);
  }
  local_state_->ClearPref(prefs::kMetricsOldClientID);

  BackUpCurrentClientInfo();
}

void MetricsStateManager::CheckForClonedInstall(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!cloned_install_detector_);

  MachineIdProvider* provider = MachineIdProvider::CreateInstance();
  if (!provider)
    return;

  cloned_install_detector_.reset(new ClonedInstallDetector(provider));
  cloned_install_detector_->CheckForClonedInstall(local_state_, task_runner);
}

scoped_ptr<const base::FieldTrial::EntropyProvider>
MetricsStateManager::CreateEntropyProvider() {
  // For metrics reporting-enabled users, we combine the client ID and low
  // entropy source to get the final entropy source. Otherwise, only use the low
  // entropy source.
  // This has two useful properties:
  //  1) It makes the entropy source less identifiable for parties that do not
  //     know the low entropy source.
  //  2) It makes the final entropy source resettable.
  const int low_entropy_source_value = GetLowEntropySource();
  UMA_HISTOGRAM_SPARSE_SLOWLY("UMA.LowEntropySourceValue",
                              low_entropy_source_value);
  if (IsMetricsReportingEnabled()) {
    if (entropy_source_returned_ == ENTROPY_SOURCE_NONE)
      entropy_source_returned_ = ENTROPY_SOURCE_HIGH;
    DCHECK_EQ(ENTROPY_SOURCE_HIGH, entropy_source_returned_);
    const std::string high_entropy_source =
        client_id_ + base::IntToString(low_entropy_source_value);
    return scoped_ptr<const base::FieldTrial::EntropyProvider>(
        new SHA1EntropyProvider(high_entropy_source));
  }

  if (entropy_source_returned_ == ENTROPY_SOURCE_NONE)
    entropy_source_returned_ = ENTROPY_SOURCE_LOW;
  DCHECK_EQ(ENTROPY_SOURCE_LOW, entropy_source_returned_);

#if defined(OS_ANDROID) || defined(OS_IOS)
  return scoped_ptr<const base::FieldTrial::EntropyProvider>(
      new CachingPermutedEntropyProvider(local_state_,
                                         low_entropy_source_value,
                                         kMaxLowEntropySize));
#else
  return scoped_ptr<const base::FieldTrial::EntropyProvider>(
      new PermutedEntropyProvider(low_entropy_source_value,
                                  kMaxLowEntropySize));
#endif
}

// static
scoped_ptr<MetricsStateManager> MetricsStateManager::Create(
    PrefService* local_state,
    const base::Callback<bool(void)>& is_reporting_enabled_callback,
    const StoreClientInfoCallback& store_client_info,
    const LoadClientInfoCallback& retrieve_client_info) {
  scoped_ptr<MetricsStateManager> result;
  // Note: |instance_exists_| is updated in the constructor and destructor.
  if (!instance_exists_) {
    result.reset(new MetricsStateManager(local_state,
                                         is_reporting_enabled_callback,
                                         store_client_info,
                                         retrieve_client_info));
  }
  return result.Pass();
}

// static
void MetricsStateManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMetricsResetIds, false);
  registry->RegisterStringPref(prefs::kMetricsClientID, std::string());
  registry->RegisterInt64Pref(prefs::kMetricsReportingEnabledTimestamp, 0);
  registry->RegisterIntegerPref(prefs::kMetricsLowEntropySource,
                                kLowEntropySourceNotSet);

  ClonedInstallDetector::RegisterPrefs(registry);
  CachingPermutedEntropyProvider::RegisterPrefs(registry);

  // TODO(asvitkine): Remove these once a couple of releases have passed.
  // http://crbug.com/357704
  registry->RegisterStringPref(prefs::kMetricsOldClientID, std::string());
  registry->RegisterIntegerPref(prefs::kMetricsOldLowEntropySource, 0);
}

void MetricsStateManager::BackUpCurrentClientInfo() {
  ClientInfo client_info;
  client_info.client_id = client_id_;
  client_info.installation_date = local_state_->GetInt64(prefs::kInstallDate);
  client_info.reporting_enabled_date =
      local_state_->GetInt64(prefs::kMetricsReportingEnabledTimestamp);
  store_client_info_.Run(client_info);
}

scoped_ptr<ClientInfo> MetricsStateManager::LoadClientInfoAndMaybeMigrate() {
  scoped_ptr<metrics::ClientInfo> client_info = load_client_info_.Run();

  // Prior to 2014-07, the client ID was stripped of its dashes before being
  // saved. Migrate back to a proper GUID if this is the case. This migration
  // code can be removed in M41+.
  const size_t kGUIDLengthWithoutDashes = 32U;
  if (client_info &&
      client_info->client_id.length() == kGUIDLengthWithoutDashes) {
    DCHECK(client_info->client_id.find('-') == std::string::npos);

    std::string client_id_with_dashes;
    client_id_with_dashes.reserve(kGUIDLengthWithoutDashes + 4U);
    std::string::const_iterator client_id_it = client_info->client_id.begin();
    for (size_t i = 0; i < kGUIDLengthWithoutDashes + 4U; ++i) {
      if (i == 8U || i == 13U || i == 18U || i == 23U) {
        client_id_with_dashes.push_back('-');
      } else {
        client_id_with_dashes.push_back(*client_id_it);
        ++client_id_it;
      }
    }
    DCHECK(client_id_it == client_info->client_id.end());
    client_info->client_id.assign(client_id_with_dashes);
  }

  // The GUID retrieved (and possibly fixed above) should be valid unless
  // retrieval failed.
  DCHECK(!client_info || base::IsValidGUID(client_info->client_id));

  return client_info.Pass();
}

int MetricsStateManager::GetLowEntropySource() {
  // Note that the default value for the low entropy source and the default pref
  // value are both kLowEntropySourceNotSet, which is used to identify if the
  // value has been set or not.
  if (low_entropy_source_ != kLowEntropySourceNotSet)
    return low_entropy_source_;

  const CommandLine* command_line(CommandLine::ForCurrentProcess());
  // Only try to load the value from prefs if the user did not request a
  // reset.
  // Otherwise, skip to generating a new value.
  if (!command_line->HasSwitch(switches::kResetVariationState)) {
    int value = local_state_->GetInteger(prefs::kMetricsLowEntropySource);
    // If the value is outside the [0, kMaxLowEntropySize) range, re-generate
    // it below.
    if (value >= 0 && value < kMaxLowEntropySize) {
      low_entropy_source_ = value;
      UMA_HISTOGRAM_BOOLEAN("UMA.GeneratedLowEntropySource", false);
      return low_entropy_source_;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("UMA.GeneratedLowEntropySource", true);
  low_entropy_source_ = GenerateLowEntropySource();
  local_state_->SetInteger(prefs::kMetricsLowEntropySource,
                           low_entropy_source_);
  local_state_->ClearPref(prefs::kMetricsOldLowEntropySource);
  CachingPermutedEntropyProvider::ClearCache(local_state_);

  return low_entropy_source_;
}

void MetricsStateManager::ResetMetricsIDsIfNecessary() {
  if (!local_state_->GetBoolean(prefs::kMetricsResetIds))
    return;

  UMA_HISTOGRAM_BOOLEAN("UMA.MetricsIDsReset", true);

  DCHECK(client_id_.empty());
  DCHECK_EQ(kLowEntropySourceNotSet, low_entropy_source_);

  local_state_->ClearPref(prefs::kMetricsClientID);
  local_state_->ClearPref(prefs::kMetricsLowEntropySource);
  local_state_->ClearPref(prefs::kMetricsResetIds);

  // Also clear the backed up client info.
  store_client_info_.Run(ClientInfo());
}

}  // namespace metrics
