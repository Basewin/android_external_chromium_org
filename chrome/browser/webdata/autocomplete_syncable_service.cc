// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autocomplete_syncable_service.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/webdata/common/web_database.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

using autofill::AutofillChange;
using autofill::AutofillChangeList;
using autofill::AutofillEntry;
using autofill::AutofillKey;
using autofill::AutofillTable;
using autofill::AutofillWebDataService;
using autofill::AutofillWebDataBackend;
using content::BrowserThread;

namespace {

const char kAutofillEntryNamespaceTag[] = "autofill_entry|";

// Merges timestamps from the |sync_timestamps| and the |local_entry|.
// Returns true if they were different, false if they were the same.  If the
// timestamps were different, fills |date_created| and |date_last_used| with the
// merged timestamps.  The |sync_timestamps| vector is assumed to be sorted.
bool MergeTimestamps(
    const google::protobuf::RepeatedField<int64_t>& sync_timestamps,
    const AutofillEntry& local_entry,
    base::Time* date_created,
    base::Time* date_last_used) {
  if (sync_timestamps.size() == 0) {
    *date_created = local_entry.date_created();
    *date_last_used = local_entry.date_last_used();
    return true;
  }

  base::Time sync_date_created =
      base::Time::FromInternalValue(*sync_timestamps.begin());
  base::Time sync_date_last_used =
      base::Time::FromInternalValue(*sync_timestamps.rbegin());

  if (sync_date_created == local_entry.date_created() &&
      sync_date_last_used == local_entry.date_last_used())
    return false;

  *date_created = std::min(local_entry.date_created(), sync_date_created);
  *date_last_used = std::max(local_entry.date_last_used(), sync_date_last_used);
  return true;
}

void* UserDataKey() {
  // Use the address of a static that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

}  // namespace

AutocompleteSyncableService::AutocompleteSyncableService(
    AutofillWebDataBackend* webdata_backend)
    : webdata_backend_(webdata_backend),
      scoped_observer_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(webdata_backend_);

  scoped_observer_.Add(webdata_backend_);
}

AutocompleteSyncableService::~AutocompleteSyncableService() {
  DCHECK(CalledOnValidThread());
}

// static
void AutocompleteSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* webdata_backend) {
  web_data_service->GetDBUserData()->SetUserData(
      UserDataKey(), new AutocompleteSyncableService(webdata_backend));
}

// static
AutocompleteSyncableService* AutocompleteSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutocompleteSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(UserDataKey()));
}

AutocompleteSyncableService::AutocompleteSyncableService()
    : webdata_backend_(NULL),
      scoped_observer_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
}

void AutocompleteSyncableService::InjectStartSyncFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

syncer::SyncMergeResult AutocompleteSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(CalledOnValidThread());
  DCHECK(!sync_processor_);
  DCHECK(sync_processor);
  DCHECK(error_handler);

  syncer::SyncMergeResult merge_result(type);
  error_handler_ = error_handler.Pass();
  std::vector<AutofillEntry> entries;
  if (!LoadAutofillData(&entries)) {
    merge_result.set_error(error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Could not load autocomplete data from the WebDatabase."));
    return merge_result;
  }

  AutocompleteEntryMap new_db_entries;
  for (std::vector<AutofillEntry>::iterator it = entries.begin();
       it != entries.end(); ++it) {
    new_db_entries[it->key()] =
        std::make_pair(syncer::SyncChange::ACTION_ADD, it);
  }

  sync_processor_ = sync_processor.Pass();

  std::vector<AutofillEntry> new_synced_entries;
  // Go through and check for all the entries that sync already knows about.
  // CreateOrUpdateEntry() will remove entries that are same with the synced
  // ones from |new_db_entries|.
  for (syncer::SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end(); ++it) {
    CreateOrUpdateEntry(*it, &new_db_entries, &new_synced_entries);
  }

  if (!SaveChangesToWebData(new_synced_entries)) {
    merge_result.set_error(error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata."));
    return merge_result;
  }

  webdata_backend_->NotifyOfMultipleAutofillChanges();

  syncer::SyncChangeList new_changes;
  for (AutocompleteEntryMap::iterator it = new_db_entries.begin();
       it != new_db_entries.end(); ++it) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           it->second.first,
                           CreateSyncData(*(it->second.second))));
  }

  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));

  // This will schedule a deletion operation on the DB thread, which will
  // trigger a notification to propagate the deletion to Sync.
  // NOTE: This must be called *after* the ProcessSyncChanges call above.
  // Otherwise, an item that Sync is not yet aware of might expire, causing a
  // Sync error when that item's deletion is propagated to Sync.
  webdata_backend_->RemoveExpiredFormElements();

  return merge_result;
}

void AutocompleteSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(syncer::AUTOFILL, type);

  sync_processor_.reset(NULL);
  error_handler_.reset();
}

syncer::SyncDataList AutocompleteSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_);
  DCHECK_EQ(type, syncer::AUTOFILL);

  syncer::SyncDataList current_data;

  std::vector<AutofillEntry> entries;
  if (!LoadAutofillData(&entries))
    return current_data;

  for (std::vector<AutofillEntry>::iterator it = entries.begin();
       it != entries.end(); ++it) {
    current_data.push_back(CreateSyncData(*it));
  }

  return current_data;
}

syncer::SyncError AutocompleteSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_processor_);

  if (!sync_processor_) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.",
                            syncer::AUTOFILL);
    return error;
  }

  // Data is loaded only if we get new ADD/UPDATE change.
  std::vector<AutofillEntry> entries;
  scoped_ptr<AutocompleteEntryMap> db_entries;
  std::vector<AutofillEntry> new_entries;

  syncer::SyncError list_processing_error;

  for (syncer::SyncChangeList::const_iterator i = change_list.begin();
       i != change_list.end() && !list_processing_error.IsSet(); ++i) {
    DCHECK(i->IsValid());
    switch (i->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
      case syncer::SyncChange::ACTION_UPDATE:
        if (!db_entries) {
          if (!LoadAutofillData(&entries)) {
            return error_handler_->CreateAndUploadError(
                FROM_HERE,
                "Could not get the autocomplete data from WebDatabase.");
          }
          db_entries.reset(new AutocompleteEntryMap);
          for (std::vector<AutofillEntry>::iterator it = entries.begin();
               it != entries.end(); ++it) {
            (*db_entries)[it->key()] =
                std::make_pair(syncer::SyncChange::ACTION_ADD, it);
          }
        }
        CreateOrUpdateEntry(i->sync_data(), db_entries.get(), &new_entries);
        break;

      case syncer::SyncChange::ACTION_DELETE: {
        DCHECK(i->sync_data().GetSpecifics().has_autofill())
            << "Autofill specifics data not present on delete!";
        const sync_pb::AutofillSpecifics& autofill =
            i->sync_data().GetSpecifics().autofill();
        if (autofill.has_value())
          list_processing_error = AutofillEntryDelete(autofill);
        else
          VLOG(1) << "Delete for old-style autofill profile being dropped!";
        break;
      }

      default:
        NOTREACHED();
        return error_handler_->CreateAndUploadError(
            FROM_HERE,
            "ProcessSyncChanges failed on ChangeType " +
                 syncer::SyncChange::ChangeTypeToString(i->change_type()));
    }
  }

  if (!SaveChangesToWebData(new_entries)) {
    return error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata.");
  }

  webdata_backend_->NotifyOfMultipleAutofillChanges();

  // This will schedule a deletion operation on the DB thread, which will
  // trigger a notification to propagate the deletion to Sync.
  webdata_backend_->RemoveExpiredFormElements();

  return list_processing_error;
}

void AutocompleteSyncableService::AutofillEntriesChanged(
    const AutofillChangeList& changes) {
  // Check if sync is on. If we receive this notification prior to sync being
  // started, we'll notify sync to start as soon as it can and later process
  // all entries when MergeData..() is called. If we receive this notification
  // sync has exited, it will be synced next time Chrome starts.
  if (sync_processor_) {
    ActOnChanges(changes);
  } else if (!flare_.is_null()) {
    flare_.Run(syncer::AUTOFILL);
    flare_.Reset();
  }
}

bool AutocompleteSyncableService::LoadAutofillData(
    std::vector<AutofillEntry>* entries) const {
  return AutofillTable::FromWebDatabase(
      webdata_backend_->GetDatabase())->GetAllAutofillEntries(entries);
}

bool AutocompleteSyncableService::SaveChangesToWebData(
    const std::vector<AutofillEntry>& new_entries) {
  DCHECK(CalledOnValidThread());

  if (!new_entries.empty() &&
      !AutofillTable::FromWebDatabase(
          webdata_backend_->GetDatabase())->UpdateAutofillEntries(
              new_entries)) {
    return false;
  }
  return true;
}

// Creates or updates an autocomplete entry based on |data|.
void AutocompleteSyncableService::CreateOrUpdateEntry(
    const syncer::SyncData& data,
    AutocompleteEntryMap* loaded_data,
    std::vector<AutofillEntry>* new_entries) {
  const sync_pb::EntitySpecifics& specifics = data.GetSpecifics();
  const sync_pb::AutofillSpecifics& autofill_specifics(specifics.autofill());

  if (!autofill_specifics.has_value()) {
    VLOG(1) << "Add/Update for old-style autofill profile being dropped!";
    return;
  }

  AutofillKey key(autofill_specifics.name().c_str(),
                  autofill_specifics.value().c_str());
  AutocompleteEntryMap::iterator it = loaded_data->find(key);
  const google::protobuf::RepeatedField<int64_t>& timestamps =
      autofill_specifics.usage_timestamp();
  if (it == loaded_data->end()) {
    // New entry.
    base::Time date_created, date_last_used;
    if (timestamps.size() > 0) {
      date_created = base::Time::FromInternalValue(*timestamps.begin());
      date_last_used = base::Time::FromInternalValue(*timestamps.rbegin());
    }
    new_entries->push_back(AutofillEntry(key, date_created, date_last_used));
  } else {
    // Entry already present - merge if necessary.
    base::Time date_created, date_last_used;
    bool different = MergeTimestamps(timestamps, *it->second.second,
                                     &date_created, &date_last_used);
    if (different) {
      AutofillEntry new_entry(
          it->second.second->key(), date_created, date_last_used);
      new_entries->push_back(new_entry);
      // Update the sync db since the timestamps have changed.
      *(it->second.second) = new_entry;
      it->second.first = syncer::SyncChange::ACTION_UPDATE;
    } else {
      loaded_data->erase(it);
    }
  }
}

// static
void AutocompleteSyncableService::WriteAutofillEntry(
    const AutofillEntry& entry, sync_pb::EntitySpecifics* autofill_specifics) {
  sync_pb::AutofillSpecifics* autofill =
      autofill_specifics->mutable_autofill();
  autofill->set_name(base::UTF16ToUTF8(entry.key().name()));
  autofill->set_value(base::UTF16ToUTF8(entry.key().value()));
  autofill->add_usage_timestamp(entry.date_created().ToInternalValue());
  if (entry.date_created() != entry.date_last_used())
    autofill->add_usage_timestamp(entry.date_last_used().ToInternalValue());
}

syncer::SyncError AutocompleteSyncableService::AutofillEntryDelete(
    const sync_pb::AutofillSpecifics& autofill) {
  if (!AutofillTable::FromWebDatabase(
          webdata_backend_->GetDatabase())->RemoveFormElement(
              base::UTF8ToUTF16(autofill.name()),
              base::UTF8ToUTF16(autofill.value()))) {
    return error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Could not remove autocomplete entry from WebDatabase.");
  }
  return syncer::SyncError();
}

void AutocompleteSyncableService::ActOnChanges(
     const AutofillChangeList& changes) {
  DCHECK(sync_processor_);
  syncer::SyncChangeList new_changes;
  for (AutofillChangeList::const_iterator change = changes.begin();
       change != changes.end(); ++change) {
    switch (change->type()) {
      case AutofillChange::ADD:
      case AutofillChange::UPDATE: {
        base::Time date_created, date_last_used;
        WebDatabase* db = webdata_backend_->GetDatabase();
        if (!AutofillTable::FromWebDatabase(db)->GetAutofillTimestamps(
                change->key().name(), change->key().value(),
                &date_created, &date_last_used)) {
          NOTREACHED();
          return;
        }
        AutofillEntry entry(change->key(), date_created, date_last_used);
        syncer::SyncChange::SyncChangeType change_type =
           (change->type() == AutofillChange::ADD) ?
            syncer::SyncChange::ACTION_ADD :
            syncer::SyncChange::ACTION_UPDATE;
        new_changes.push_back(syncer::SyncChange(FROM_HERE,
                                                 change_type,
                                                 CreateSyncData(entry)));
        break;
      }

      case AutofillChange::REMOVE: {
        AutofillEntry entry(change->key(), base::Time(), base::Time());
        new_changes.push_back(
            syncer::SyncChange(FROM_HERE,
                               syncer::SyncChange::ACTION_DELETE,
                               CreateSyncData(entry)));
        break;
      }

      default:
        NOTREACHED();
    }
  }
  syncer::SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (error.IsSet()) {
    VLOG(1) << "[AUTOCOMPLETE SYNC] Failed processing change.  Error: "
            << error.message();
  }
}

syncer::SyncData AutocompleteSyncableService::CreateSyncData(
    const AutofillEntry& entry) const {
  sync_pb::EntitySpecifics autofill_specifics;
  WriteAutofillEntry(entry, &autofill_specifics);
  std::string tag(KeyToTag(base::UTF16ToUTF8(entry.key().name()),
                           base::UTF16ToUTF8(entry.key().value())));
  return syncer::SyncData::CreateLocalData(tag, tag, autofill_specifics);
}

// static
std::string AutocompleteSyncableService::KeyToTag(const std::string& name,
                                                  const std::string& value) {
  std::string prefix(kAutofillEntryNamespaceTag);
  return prefix + net::EscapePath(name) + "|" + net::EscapePath(value);
}
