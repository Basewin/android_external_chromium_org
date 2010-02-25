// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/sync/engine/verify_updates_command.h"

#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

using syncable::ScopedDirLookup;
using syncable::SyncName;
using syncable::WriteTransaction;

using syncable::GET_BY_ID;
using syncable::SYNCER;

VerifyUpdatesCommand::VerifyUpdatesCommand() {}
VerifyUpdatesCommand::~VerifyUpdatesCommand() {}

void VerifyUpdatesCommand::ExecuteImpl(sessions::SyncSession* session) {
  LOG(INFO) << "Beginning Update Verification";
  ScopedDirLookup dir(session->context()->directory_manager(),
                      session->context()->account_name());
  if (!dir.good()) {
    LOG(ERROR) << "Scoped dir lookup failed!";
    return;
  }
  WriteTransaction trans(dir, SYNCER, __FILE__, __LINE__);
  sessions::StatusController* status = session->status_controller();
  const GetUpdatesResponse& updates = status->updates_response().get_updates();
  int update_count = updates.entries().size();

  LOG(INFO) << update_count << " entries to verify";
  for (int i = 0; i < update_count; i++) {
    const SyncEntity entry =
        *reinterpret_cast<const SyncEntity *>(&(updates.entries(i)));
    // Needs to be done separately in order to make sure the update processing
    // still happens like normal. We should really just use one type of
    // ID in fact, there isn't actually a need for server_knows and not IDs.
    SyncerUtil::AttemptReuniteLostCommitResponses(&trans, entry,
        trans.directory()->cache_guid());

    // Affix IDs properly prior to inputting data into server entry.
    SyncerUtil::AttemptReuniteClientTag(&trans, entry);

    VerifyUpdateResult result = VerifyUpdate(&trans, entry,
                                             session->routing_info());
    status->GetUnrestrictedUpdateProgress(
        result.placement)->AddVerifyResult(result.value, entry);
  }
}

namespace {
// In the event that IDs match, but tags differ AttemptReuniteClient tag
// will have refused to unify the update.
// We should not attempt to apply it at all since it violates consistency
// rules.
VerifyResult VerifyTagConsistency(const SyncEntity& entry,
                                  const syncable::MutableEntry& same_id) {
  if (entry.has_client_defined_unique_tag() &&
      entry.client_defined_unique_tag() !=
          same_id.Get(syncable::UNIQUE_CLIENT_TAG)) {
    return VERIFY_FAIL;
  }
  return VERIFY_UNDECIDED;
}
}  // namespace

VerifyUpdatesCommand::VerifyUpdateResult VerifyUpdatesCommand::VerifyUpdate(
    syncable::WriteTransaction* trans, const SyncEntity& entry,
    const ModelSafeRoutingInfo& routes) {
  syncable::Id id = entry.id();
  VerifyUpdateResult result = {VERIFY_FAIL, GROUP_PASSIVE};

  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = entry.IsFolder();
  const syncable::ModelType model_type = entry.GetModelType();

  if (!id.ServerKnows()) {
    LOG(ERROR) << "Illegal negative id in received updates";
    return result;
  }
  if (!entry.parent_id().ServerKnows()) {
    LOG(ERROR) << "Illegal parent id in received updates";
    return result;
  }
  {
    const std::string name = SyncerProtoUtil::NameFromSyncEntity(entry);
    if (name.empty() && !deleted) {
      LOG(ERROR) << "Zero length name in non-deleted update";
      return result;
    }
  }

  syncable::MutableEntry same_id(trans, GET_BY_ID, id);
  result.value = SyncerUtil::VerifyNewEntry(entry, &same_id, deleted);

  syncable::ModelType placement_type = !deleted ? entry.GetModelType()
    : same_id.good() ? same_id.GetModelType() : syncable::UNSPECIFIED;
  result.placement = GetGroupForModelType(placement_type, routes);

  if (VERIFY_UNDECIDED == result.value) {
    result.value = VerifyTagConsistency(entry, same_id);
  }

  if (VERIFY_UNDECIDED == result.value) {
    if (deleted)
      result.value = VERIFY_SUCCESS;
  }

  // If we have an existing entry, we check here for updates that break
  // consistency rules.
  if (VERIFY_UNDECIDED == result.value) {
    result.value = SyncerUtil::VerifyUpdateConsistency(trans, entry, &same_id,
        deleted, is_directory, model_type);
  }

  if (VERIFY_UNDECIDED == result.value)
    result.value = VERIFY_SUCCESS;  // No news is good news.

  return result;  // This might be VERIFY_SUCCESS as well
}

}  // namespace browser_sync
