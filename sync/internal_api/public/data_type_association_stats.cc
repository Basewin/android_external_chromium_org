// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/data_type_association_stats.h"

namespace syncer {

DataTypeAssociationStats::DataTypeAssociationStats()
  : num_local_items_before_association(0),
    num_sync_items_before_association(0),
    num_local_items_after_association(0),
    num_sync_items_after_association(0),
    num_local_items_added(0),
    num_local_items_deleted(0),
    num_local_items_modified(0),
    num_sync_items_added(0),
    num_sync_items_deleted(0),
    num_sync_items_modified(0),
    had_error(false) {
}

DataTypeAssociationStats::~DataTypeAssociationStats() {
}

}  // namespace syncer
