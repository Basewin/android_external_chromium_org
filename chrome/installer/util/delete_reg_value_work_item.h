// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_REG_VALUE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_REG_VALUE_WORK_ITEM_H_
#pragma once

#include <windows.h>

#include <string>

#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that deletes a registry value with REG_SZ, REG_DWORD, or
// REG_QWORD type at the specified path. The value is only deleted if the target
// key exists.
class DeleteRegValueWorkItem : public WorkItem {
 public:
  virtual ~DeleteRegValueWorkItem();

  virtual bool Do();

  virtual void Rollback();

 private:
  friend class WorkItem;

  enum DeletionStatus {
    // The status before Do is called.
    DELETE_VALUE,
    // One possible outcome after Do(). Value is deleted.
    VALUE_DELETED,
    // One possible outcome after Do(). Value is not found.
    VALUE_NOT_FOUND,
    // The status after Do() and Rollback() is called.
    VALUE_ROLLED_BACK,
    // Another possible outcome after Do() (when there is an error).
    VALUE_UNCHANGED
  };

  DeleteRegValueWorkItem(HKEY predefined_root, const std::wstring& key_path,
                         const std::wstring& value_name, DWORD type);

  // Root key of the target key under which the value is set. The root key can
  // only be one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the target key under which the value is set.
  std::wstring key_path_;

  // Name of the value to be set.
  std::wstring value_name_;

  // DWORD that tells whether data value is of type REG_SZ, REG_DWORD, or
  // REG_QWORD
  // Ideally we do not need this information from user of this class and can
  // check the registry for the type. But to simpify implementation we are
  // going to put the burden on the caller for now to provide us the type.
  DWORD type_;

  DeletionStatus status_;

  // Data of the previous value.
  std::wstring old_str_;  // if data is of type REG_SZ
  DWORD old_dw_;  // if data is of type REG_DWORD
  int64 old_qword_;  // if data is of type REG_QWORD
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_REG_VALUE_WORK_ITEM_H_
