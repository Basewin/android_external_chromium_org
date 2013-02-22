// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/sync_file_metadata.h"

using sync_file_system::SyncFileType;

namespace fileapi {

SyncFileMetadata::SyncFileMetadata()
    : file_type(sync_file_system::SYNC_FILE_TYPE_UNKNOWN),
      size(-1) {
}

SyncFileMetadata::SyncFileMetadata(
    SyncFileType file_type,
    int64 size,
    const base::Time& last_modified)
    : file_type(file_type),
      size(size),
      last_modified(last_modified) {
}

SyncFileMetadata::~SyncFileMetadata() {}

bool SyncFileMetadata::operator==(const SyncFileMetadata& that) const {
  return file_type == that.file_type &&
         size == that.size &&
         last_modified == that.last_modified;
}

ConflictFileInfo::ConflictFileInfo() {}
ConflictFileInfo::~ConflictFileInfo() {}

LocalFileSyncInfo::LocalFileSyncInfo() {}
LocalFileSyncInfo::~LocalFileSyncInfo() {}

}  // namespace fileapi
