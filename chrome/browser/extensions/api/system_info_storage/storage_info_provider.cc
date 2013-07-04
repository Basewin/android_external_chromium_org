// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

using content::BrowserThread;
using chrome::StorageMonitor;
using api::experimental_system_info_storage::StorageUnitInfo;
using api::experimental_system_info_storage::STORAGE_UNIT_TYPE_FIXED;
using api::experimental_system_info_storage::STORAGE_UNIT_TYPE_REMOVABLE;

namespace systeminfo {

const char kStorageTypeUnknown[] = "unknown";
const char kStorageTypeFixed[] = "fixed";
const char kStorageTypeRemovable[] = "removable";

void BuildStorageUnitInfo(const chrome::StorageInfo& info,
                          StorageUnitInfo* unit) {
  unit->id = StorageInfoProvider::Get()->GetTransientIdForDeviceId(
                 info.device_id());
  unit->name = UTF16ToUTF8(info.name());
  // TODO(hmin): Might need to take MTP device into consideration.
  unit->type = chrome::StorageInfo::IsRemovableDevice(info.device_id()) ?
      STORAGE_UNIT_TYPE_REMOVABLE : STORAGE_UNIT_TYPE_FIXED;
  unit->capacity = static_cast<double>(info.total_size_in_bytes());
  unit->available_capacity = 0;
}

}  // namespace systeminfo

const int kDefaultPollingIntervalMs = 1000;
const char kWatchingTokenName[] = "_storage_info_watching_token_";

StorageInfoProvider::StorageInfoProvider()
    : observers_(new ObserverListThreadSafe<StorageFreeSpaceObserver>()),
      watching_interval_(kDefaultPollingIntervalMs) {
}

StorageInfoProvider::~StorageInfoProvider() {
}

void StorageInfoProvider::StartQueryInfoImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Register a callback to notify UI thread that StorageMonitor finishes the
  // storage metadata retrieval on FILE thread. See the comments of
  // StorageMonitor::Initialize about when the callback gets run.
  StorageMonitor::GetInstance()->EnsureInitialized(
      base::Bind(&StorageInfoProvider::QueryInfoImplOnUIThread, this));
}

void StorageInfoProvider::QueryInfoImplOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // At this point, we can call StorageMonitor::GetAllAvailableStorages to
  // get the correct results.
  QueryInfo(&info_);
  // The amount of available capacity should be queried on blocking pool.
  PostQueryTaskToBlockingPool(FROM_HERE,
      base::Bind(&StorageInfoProvider::QueryAvailableCapacityOnBlockingPool,
          this));
}

void StorageInfoProvider::QueryAvailableCapacityOnBlockingPool() {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  for (StorageInfo::iterator it = info_.begin(); it != info_.end(); ++it) {
    int64 amount = GetStorageFreeSpaceFromTransientId((*it)->id);
    if (amount > 0)
      (*it)->available_capacity = static_cast<double>(amount);
  }

  // Notify UI thread that the querying operation has completed.
  BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&StorageInfoProvider::OnQueryCompleted, this, true));
}

std::vector<chrome::StorageInfo> StorageInfoProvider::GetAllStorages() const {
  return StorageMonitor::GetInstance()->GetAllAvailableStorages();
}

bool StorageInfoProvider::QueryInfo(StorageInfo* info) {
  std::vector<chrome::StorageInfo> storage_list = GetAllStorages();
  StorageInfo results;
  std::vector<chrome::StorageInfo>::const_iterator it = storage_list.begin();
  for (; it != storage_list.end(); ++it) {
    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    systeminfo::BuildStorageUnitInfo(*it, unit.get());
    results.push_back(unit);
  }
  info->swap(results);

  return true;
}

void StorageInfoProvider::AddObserver(StorageFreeSpaceObserver* obs) {
  observers_->AddObserver(obs);
}

void StorageInfoProvider::RemoveObserver(StorageFreeSpaceObserver* obs) {
  observers_->RemoveObserver(obs);
}

void StorageInfoProvider::StartWatching(const std::string& transient_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostBlockingPoolSequencedTask(
      kWatchingTokenName,
      FROM_HERE,
      base::Bind(&StorageInfoProvider::AddWatchedStorageOnBlockingPool,
                 this, transient_id));
}

void StorageInfoProvider::StopWatching(const std::string& transient_id) {
  base::FilePath mount_path;
  BrowserThread::PostBlockingPoolSequencedTask(
      kWatchingTokenName,
      FROM_HERE,
      base::Bind(&StorageInfoProvider::RemoveWatchedStorageOnBlockingPool,
                 this, transient_id));
}

int64 StorageInfoProvider::GetStorageFreeSpaceFromTransientId(
    const std::string& transient_id) {
  std::vector<chrome::StorageInfo> storage_list = GetAllStorages();
  std::string device_id = GetDeviceIdForTransientId(transient_id);

  // Lookup the matched storage info by |device_id|.
  for (std::vector<chrome::StorageInfo>::const_iterator it =
       storage_list.begin();
       it != storage_list.end(); ++it) {
    if (device_id == it->device_id())
      return base::SysInfo::AmountOfFreeDiskSpace(
                 base::FilePath(it->location()));
  }

  return -1;
}

void StorageInfoProvider::AddWatchedStorageOnBlockingPool(
    const std::string& transient_id) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  // If the storage |transient_id| is already being watched.
  if (ContainsKey(storage_transient_id_to_size_map_, transient_id))
    return;

  int64 available_bytes = GetStorageFreeSpaceFromTransientId(transient_id);
  if (available_bytes < 0)
    return;

  storage_transient_id_to_size_map_[transient_id] = available_bytes;

  // If it is the first storage to be watched, we need to start the watching
  // timer.
  if (storage_transient_id_to_size_map_.size() == 1) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&StorageInfoProvider::StartWatchingTimerOnUIThread, this));
  }
}

void StorageInfoProvider::RemoveWatchedStorageOnBlockingPool(
    const std::string& transient_id) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  if (storage_transient_id_to_size_map_.erase(transient_id) == 0)
    return;

  // Stop watching timer if there is no storage to be watched.
  if (storage_transient_id_to_size_map_.empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&StorageInfoProvider::StopWatchingTimerOnUIThread, this));
  }
}

void StorageInfoProvider::CheckWatchedStorages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<chrome::StorageInfo> storage_list = GetAllStorages();

  for (std::vector<chrome::StorageInfo>::iterator it = storage_list.begin();
       it != storage_list.end(); ++it) {
    BrowserThread::PostBlockingPoolSequencedTask(
        kWatchingTokenName,
        FROM_HERE,
        base::Bind(&StorageInfoProvider::CheckWatchedStorageOnBlockingPool,
                   this, GetTransientIdForDeviceId(it->device_id())));
  }
}

void StorageInfoProvider::CheckWatchedStorageOnBlockingPool(
    const std::string& transient_id) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  if (!ContainsKey(storage_transient_id_to_size_map_, transient_id))
    return;

  double new_available_bytes = GetStorageFreeSpaceFromTransientId(transient_id);
  double old_available_bytes = storage_transient_id_to_size_map_[transient_id];

  // No free space change.
  if (new_available_bytes == old_available_bytes)
    return;

  if (new_available_bytes < 0) {
    // In case we can't establish the new free space value from |transient_id|,
    // the |transient_id| is currently not available, so we need to remove it
    // from |storage_transient_id_to_size_map_|.
    storage_transient_id_to_size_map_.erase(transient_id);
    if (storage_transient_id_to_size_map_.size() == 0) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&StorageInfoProvider::StopWatchingTimerOnUIThread, this));
    }
    return;
  }

  // Ignore free space change event if the old available capacity is 0.
  if (old_available_bytes > 0) {
    observers_->Notify(&StorageFreeSpaceObserver::OnFreeSpaceChanged,
                       transient_id,
                       old_available_bytes,
                       new_available_bytes);
  }
  storage_transient_id_to_size_map_[transient_id] = new_available_bytes;
}

void StorageInfoProvider::StartWatchingTimerOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Start the watching timer if it is not running.
  if (!watching_timer_.IsRunning()) {
    watching_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(watching_interval_),
        this, &StorageInfoProvider::CheckWatchedStorages);
  }
}

void StorageInfoProvider::StopWatchingTimerOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  watching_timer_.Stop();
}

std::string StorageInfoProvider::GetTransientIdForDeviceId(
    const std::string& device_id) const {
  return StorageMonitor::GetInstance()->GetTransientIdForDeviceId(device_id);
}

std::string StorageInfoProvider::GetDeviceIdForTransientId(
    const std::string& transient_id) const {
  return StorageMonitor::GetInstance()->GetDeviceIdForTransientId(transient_id);
}

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  return StorageInfoProvider::GetInstance<StorageInfoProvider>();
}

}  // namespace extensions
