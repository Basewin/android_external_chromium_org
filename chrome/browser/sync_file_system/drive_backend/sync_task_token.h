// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_TOKEN_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_TOKEN_H_

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace sync_file_system {
namespace drive_backend {

class SyncTaskManager;
struct BlockingFactor;

// Represents a running sequence of SyncTasks.  Owned by a callback chain that
// should run exclusively, and held by SyncTaskManager when no task is running.
class SyncTaskToken {
 public:
  static const int64 kForegroundTaskTokenID;
  static const int64 kMinimumBackgroundTaskTokenID;

  static scoped_ptr<SyncTaskToken> CreateForForegroundTask(
      const base::WeakPtr<SyncTaskManager>& manager);
  static scoped_ptr<SyncTaskToken> CreateForBackgroundTask(
      const base::WeakPtr<SyncTaskManager>& manager,
      int64 token_id,
      scoped_ptr<BlockingFactor> blocking_factor);

  void UpdateTask(const tracked_objects::Location& location,
                  const SyncStatusCallback& callback);

  const tracked_objects::Location& location() const { return location_; }
  virtual ~SyncTaskToken();

  static SyncStatusCallback WrapToCallback(scoped_ptr<SyncTaskToken> token);

  SyncTaskManager* manager() { return manager_.get(); }

  const SyncStatusCallback& callback() const { return callback_; }
  void clear_callback() { callback_.Reset(); }

  void set_blocking_factor(scoped_ptr<BlockingFactor> blocking_factor);
  const BlockingFactor* blocking_factor() const;
  void clear_blocking_factor();

  int64 token_id() const { return token_id_; }

 private:
  SyncTaskToken(const base::WeakPtr<SyncTaskManager>& manager,
                int64 token_id,
                scoped_ptr<BlockingFactor> blocking_factor);

  base::WeakPtr<SyncTaskManager> manager_;
  tracked_objects::Location location_;
  int64 token_id_;
  SyncStatusCallback callback_;

  scoped_ptr<BlockingFactor> blocking_factor_;

  DISALLOW_COPY_AND_ASSIGN(SyncTaskToken);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_TASK_TOKEN_H_
