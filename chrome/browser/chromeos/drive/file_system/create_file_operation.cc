// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"

#include <string>

#include "base/file_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

const char kMimeTypeOctetStream[] = "application/octet-stream";

// Updates local state.
FileError UpdateLocalState(internal::ResourceMetadata* metadata,
                           const base::FilePath& file_path,
                           const std::string& mime_type_in,
                           ResourceEntry* entry) {
  DCHECK(metadata);

  FileError error = metadata->GetResourceEntryByPath(file_path, entry);
  if (error == FILE_ERROR_OK)
    return FILE_ERROR_EXISTS;

  if (error != FILE_ERROR_NOT_FOUND)
    return error;

  // If parent path is not a directory, it is an error.
  ResourceEntry parent;
  if (metadata->GetResourceEntryByPath(
          file_path.DirName(), &parent) != FILE_ERROR_OK ||
      !parent.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  // If mime_type is not set or "application/octet-stream", guess from the
  // |file_path|. If it is still unsure, use octet-stream by default.
  std::string mime_type = mime_type_in;
  if ((mime_type.empty() || mime_type == kMimeTypeOctetStream) &&
      !net::GetMimeTypeFromFile(file_path, &mime_type))
    mime_type = kMimeTypeOctetStream;

  // Add the entry to the local resource metadata.
  const base::Time now = base::Time::Now();
  entry->mutable_file_info()->set_last_modified(now.ToInternalValue());
  entry->mutable_file_info()->set_last_accessed(now.ToInternalValue());
  entry->set_title(file_path.BaseName().AsUTF8Unsafe());
  entry->set_parent_local_id(parent.local_id());
  entry->set_metadata_edit_state(ResourceEntry::DIRTY);
  entry->set_modification_date(base::Time::Now().ToInternalValue());
  entry->mutable_file_specific_info()->set_content_mime_type(mime_type);

  std::string local_id;
  error = metadata->AddEntry(*entry, &local_id);
  entry->set_local_id(local_id);
  return error;
}

}  // namespace

CreateFileOperation::CreateFileOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    internal::ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      metadata_(metadata),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CreateFileOperation::~CreateFileOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void CreateFileOperation::CreateFile(const base::FilePath& file_path,
                                     bool is_exclusive,
                                     const std::string& mime_type,
                                     const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalState,
                 metadata_,
                 file_path,
                 mime_type,
                 entry),
      base::Bind(&CreateFileOperation::CreateFileAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 file_path,
                 is_exclusive,
                 base::Owned(entry)));
}

void CreateFileOperation::CreateFileAfterUpdateLocalState(
    const FileOperationCallback& callback,
    const base::FilePath& file_path,
    bool is_exclusive,
    ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_EXISTS) {
    // Error if an exclusive mode is requested, or the entry is not a file.
    error = (is_exclusive ||
             entry->file_info().is_directory() ||
             entry->file_specific_info().is_hosted_document()) ?
        FILE_ERROR_EXISTS : FILE_ERROR_OK;
  } else if (error == FILE_ERROR_OK) {
    // Notify observer if the file was newly created.
    observer_->OnDirectoryChangedByOperation(file_path.DirName());
    observer_->OnEntryUpdatedByOperation(entry->local_id());
  }
  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
