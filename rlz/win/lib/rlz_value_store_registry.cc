// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/win/lib/rlz_value_store_registry.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/rlz_lib.h"
#include "rlz/lib/string_utils.h"
#include "rlz/win/lib/registry_util.h"

using base::ASCIIToWide;

namespace rlz_lib {

namespace {

//
// Registry keys:
//
//   RLZ's are stored as:
//   <AccessPointName>  = <RLZ value> @ kRootKey\kLibKeyName\kRlzsSubkeyName.
//
//   Events are stored as:
//   <AccessPointName><EventName> = 1 @
//   HKCU\kLibKeyName\kEventsSubkeyName\GetProductName(product).
//
//   The OEM Deal Confirmation Code (DCC) is stored as
//   kDccValueName = <DCC value> @ HKLM\kLibKeyName
//
//   The last ping time, per product is stored as:
//   GetProductName(product) = <last ping time> @
//   HKCU\kLibKeyName\kPingTimesSubkeyName.
//
// The server does not care about any of these constants.
//
const char kLibKeyName[]               = "Software\\Google\\Common\\Rlz";
const wchar_t kGoogleKeyName[]         = L"Software\\Google";
const wchar_t kGoogleCommonKeyName[]   = L"Software\\Google\\Common";
const char kRlzsSubkeyName[]           = "RLZs";
const char kEventsSubkeyName[]         = "Events";
const char kStatefulEventsSubkeyName[] = "StatefulEvents";
const char kPingTimesSubkeyName[]      = "PTimes";

std::wstring GetWideProductName(Product product) {
  return ASCIIToWide(GetProductName(product));
}

void AppendBrandToString(std::string* str) {
  std::string brand(SupplementaryBranding::GetBrand());
  if (!brand.empty())
    base::StringAppendF(str, "\\_%s", brand.c_str());
}

// Function to get the specific registry keys.
bool GetRegKey(const char* name, REGSAM access, base::win::RegKey* key) {
  std::string key_location;
  base::StringAppendF(&key_location, "%s\\%s", kLibKeyName, name);
  AppendBrandToString(&key_location);

  LONG ret = ERROR_SUCCESS;
  if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK)) {
    ret = key->Create(HKEY_CURRENT_USER, ASCIIToWide(key_location).c_str(),
                      access);
  } else {
    ret = key->Open(HKEY_CURRENT_USER, ASCIIToWide(key_location).c_str(),
                    access);
  }

  return ret == ERROR_SUCCESS;
}

bool GetPingTimesRegKey(REGSAM access, base::win::RegKey* key) {
  return GetRegKey(kPingTimesSubkeyName, access, key);
}


bool GetEventsRegKey(const char* event_type,
                     const rlz_lib::Product* product,
                     REGSAM access, base::win::RegKey* key) {
  std::string key_location;
  base::StringAppendF(&key_location, "%s\\%s", kLibKeyName,
                      event_type);
  AppendBrandToString(&key_location);

  if (product != NULL) {
    std::string product_name = GetProductName(*product);
    if (product_name.empty())
      return false;

    base::StringAppendF(&key_location, "\\%s", product_name.c_str());
  }

  LONG ret = ERROR_SUCCESS;
  if (access & (KEY_SET_VALUE | KEY_CREATE_SUB_KEY | KEY_CREATE_LINK)) {
    ret = key->Create(HKEY_CURRENT_USER, ASCIIToWide(key_location).c_str(),
                      access);
  } else {
    ret = key->Open(HKEY_CURRENT_USER, ASCIIToWide(key_location).c_str(),
                    access);
  }

  return ret == ERROR_SUCCESS;
}

bool GetAccessPointRlzsRegKey(REGSAM access, base::win::RegKey* key) {
  return GetRegKey(kRlzsSubkeyName, access, key);
}

bool ClearAllProductEventValues(rlz_lib::Product product, const char* key) {
  std::wstring product_name = GetWideProductName(product);
  if (product_name.empty())
    return false;

  base::win::RegKey reg_key;
  GetEventsRegKey(key, NULL, KEY_WRITE, &reg_key);
  reg_key.DeleteKey(product_name.c_str());

  // Verify that the value no longer exists.
  base::win::RegKey product_events(
      reg_key.Handle(), product_name.c_str(), KEY_READ);
  if (product_events.Valid()) {
    ASSERT_STRING("ClearAllProductEvents: Key deletion failed");
    return false;
  }

  return true;
}

// Deletes a registry key if it exists and has no subkeys or values.
// TODO: Move this to a registry_utils file and add unittest.
bool DeleteKeyIfEmpty(HKEY root_key, const wchar_t* key_name) {
  if (!key_name) {
    ASSERT_STRING("DeleteKeyIfEmpty: key_name is NULL");
    return false;
  } else {  // Scope needed for RegKey
    base::win::RegKey key(root_key, key_name, KEY_READ);
    if (!key.Valid())
      return true;  // Key does not exist - nothing to do.

    base::win::RegistryKeyIterator key_iter(root_key, key_name);
    if (key_iter.SubkeyCount() > 0)
      return true;  // Not empty, so nothing to do

    base::win::RegistryValueIterator value_iter(root_key, key_name);
    if (value_iter.ValueCount() > 0)
      return true;  // Not empty, so nothing to do
  }

  // The key is empty - delete it now.
  base::win::RegKey key(root_key, L"", KEY_WRITE);
  return key.DeleteKey(key_name) == ERROR_SUCCESS;
}

}  // namespace

// static
std::wstring RlzValueStoreRegistry::GetWideLibKeyName() {
  return ASCIIToWide(kLibKeyName);
}

bool RlzValueStoreRegistry::HasAccess(AccessType type) {
  return HasUserKeyAccess(type == kWriteAccess);
}

bool RlzValueStoreRegistry::WritePingTime(Product product, int64 time) {
  base::win::RegKey key;
  std::wstring product_name = GetWideProductName(product);
  return GetPingTimesRegKey(KEY_WRITE, &key) &&
      key.WriteValue(product_name.c_str(), &time, sizeof(time),
                     REG_QWORD) == ERROR_SUCCESS;
}

bool RlzValueStoreRegistry::ReadPingTime(Product product, int64* time) {
  base::win::RegKey key;
  std::wstring product_name = GetWideProductName(product);
  return GetPingTimesRegKey(KEY_READ, &key) &&
      key.ReadInt64(product_name.c_str(), time) == ERROR_SUCCESS;
}

bool RlzValueStoreRegistry::ClearPingTime(Product product) {
  base::win::RegKey key;
  GetPingTimesRegKey(KEY_WRITE, &key);

  std::wstring product_name = GetWideProductName(product);
  key.DeleteValue(product_name.c_str());

  // Verify deletion.
  uint64 value;
  DWORD size = sizeof(value);
  if (key.ReadValue(
        product_name.c_str(), &value, &size, NULL) == ERROR_SUCCESS) {
    ASSERT_STRING("RlzValueStoreRegistry::ClearPingTime: Failed to delete.");
    return false;
  }

  return true;
}

bool RlzValueStoreRegistry::WriteAccessPointRlz(AccessPoint access_point,
                                                const char* new_rlz) {
  const char* access_point_name = GetAccessPointName(access_point);
  if (!access_point_name)
    return false;

  std::wstring access_point_name_wide(ASCIIToWide(access_point_name));
  base::win::RegKey key;
  GetAccessPointRlzsRegKey(KEY_WRITE, &key);

  if (!RegKeyWriteValue(key, access_point_name_wide.c_str(), new_rlz)) {
    ASSERT_STRING("SetAccessPointRlz: Could not write the new RLZ value");
    return false;
  }
  return true;
}

bool RlzValueStoreRegistry::ReadAccessPointRlz(AccessPoint access_point,
                                               char* rlz,
                                               size_t rlz_size) {
  const char* access_point_name = GetAccessPointName(access_point);
  if (!access_point_name)
    return false;

  size_t size = rlz_size;
  base::win::RegKey key;
  GetAccessPointRlzsRegKey(KEY_READ, &key);
  if (!RegKeyReadValue(key, ASCIIToWide(access_point_name).c_str(),
                       rlz, &size)) {
    rlz[0] = 0;
    if (size > rlz_size) {
      ASSERT_STRING("GetAccessPointRlz: Insufficient buffer size");
      return false;
    }
  }
  return true;
}

bool RlzValueStoreRegistry::ClearAccessPointRlz(AccessPoint access_point) {
  const char* access_point_name = GetAccessPointName(access_point);
  if (!access_point_name)
    return false;

  std::wstring access_point_name_wide(ASCIIToWide(access_point_name));
  base::win::RegKey key;
  GetAccessPointRlzsRegKey(KEY_WRITE, &key);

  key.DeleteValue(access_point_name_wide.c_str());

  // Verify deletion.
  DWORD value;
  if (key.ReadValueDW(access_point_name_wide.c_str(), &value) ==
      ERROR_SUCCESS) {
    ASSERT_STRING("SetAccessPointRlz: Could not clear the RLZ value.");
    return false;
  }
  return true;
}

bool RlzValueStoreRegistry::AddProductEvent(Product product,
                                            const char* event_rlz) {
  std::wstring event_rlz_wide(ASCIIToWide(event_rlz));
  base::win::RegKey reg_key;
  GetEventsRegKey(kEventsSubkeyName, &product, KEY_WRITE, &reg_key);
  if (reg_key.WriteValue(event_rlz_wide.c_str(), 1) != ERROR_SUCCESS) {
    ASSERT_STRING("AddProductEvent: Could not write the new event value");
    return false;
  }

  return true;
}

bool RlzValueStoreRegistry::ReadProductEvents(Product product,
                                             std::vector<std::string>* events) {
  // Open the events key.
  base::win::RegKey events_key;
  GetEventsRegKey(kEventsSubkeyName, &product, KEY_READ, &events_key);
  if (!events_key.Valid())
    return false;

  // Append the events to the buffer.
  int num_values = 0;
  LONG result = ERROR_SUCCESS;
  for (num_values = 0; result == ERROR_SUCCESS; ++num_values) {
    // Max 32767 bytes according to MSDN, but we never use that much.
    const size_t kMaxValueNameLength = 2048;
    char buffer[kMaxValueNameLength];
    DWORD size = arraysize(buffer);

    result = RegEnumValueA(events_key.Handle(), num_values, buffer, &size,
                           NULL, NULL, NULL, NULL);
    if (result == ERROR_SUCCESS)
      events->push_back(std::string(buffer));
  }

  return result == ERROR_NO_MORE_ITEMS;
}

bool RlzValueStoreRegistry::ClearProductEvent(Product product,
                                              const char* event_rlz) {
  std::wstring event_rlz_wide(ASCIIToWide(event_rlz));
  base::win::RegKey key;
  GetEventsRegKey(kEventsSubkeyName, &product, KEY_WRITE, &key);
  key.DeleteValue(event_rlz_wide.c_str());

  // Verify deletion.
  DWORD value;
  if (key.ReadValueDW(event_rlz_wide.c_str(), &value) == ERROR_SUCCESS) {
    ASSERT_STRING("ClearProductEvent: Could not delete the event value.");
    return false;
  }

  return true;
}

bool RlzValueStoreRegistry::ClearAllProductEvents(Product product) {
  return ClearAllProductEventValues(product, kEventsSubkeyName);
}

bool RlzValueStoreRegistry::AddStatefulEvent(Product product,
                                             const char* event_rlz) {
  base::win::RegKey key;
  std::wstring event_rlz_wide(ASCIIToWide(event_rlz));
  if (!GetEventsRegKey(kStatefulEventsSubkeyName, &product, KEY_WRITE, &key) ||
      key.WriteValue(event_rlz_wide.c_str(), 1) != ERROR_SUCCESS) {
    ASSERT_STRING(
        "AddStatefulEvent: Could not write the new stateful event");
    return false;
  }

  return true;
}

bool RlzValueStoreRegistry::IsStatefulEvent(Product product,
                                            const char* event_rlz) {
  DWORD value;
  base::win::RegKey key;
  GetEventsRegKey(kStatefulEventsSubkeyName, &product, KEY_READ, &key);
  std::wstring event_rlz_wide(ASCIIToWide(event_rlz));
  return key.ReadValueDW(event_rlz_wide.c_str(), &value) == ERROR_SUCCESS;
}

bool RlzValueStoreRegistry::ClearAllStatefulEvents(Product product) {
  return ClearAllProductEventValues(product, kStatefulEventsSubkeyName);
}

void RlzValueStoreRegistry::CollectGarbage() {
  // Delete each of the known subkeys if empty.
  const char* subkeys[] = {
    kRlzsSubkeyName,
    kEventsSubkeyName,
    kStatefulEventsSubkeyName,
    kPingTimesSubkeyName
  };

  for (int i = 0; i < arraysize(subkeys); i++) {
    std::string subkey_name;
    base::StringAppendF(&subkey_name, "%s\\%s", kLibKeyName, subkeys[i]);
    AppendBrandToString(&subkey_name);

    VERIFY(DeleteKeyIfEmpty(HKEY_CURRENT_USER,
                            ASCIIToWide(subkey_name).c_str()));
  }

  // Delete the library key and its parents too now if empty.
  VERIFY(DeleteKeyIfEmpty(HKEY_CURRENT_USER, GetWideLibKeyName().c_str()));
  VERIFY(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kGoogleCommonKeyName));
  VERIFY(DeleteKeyIfEmpty(HKEY_CURRENT_USER, kGoogleKeyName));
}

ScopedRlzValueStoreLock::ScopedRlzValueStoreLock() {
  if (!lock_.failed())
    store_.reset(new RlzValueStoreRegistry);
}

ScopedRlzValueStoreLock::~ScopedRlzValueStoreLock() {
}

RlzValueStore* ScopedRlzValueStoreLock::GetStore() {
  return store_.get();
}

}  // namespace rlz_lib
