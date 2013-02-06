// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_REGISTRY_H_
#define CHROME_BROWSER_PREFS_PREF_REGISTRY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_value_map.h"

namespace base {
class Value;
}

class DefaultPrefStore;
class PrefStore;

// Preferences need to be registered with a type and default value
// before they are used.
//
// The way you use a PrefRegistry is that you register all required
// preferences on it (via one of its subclasses), then pass it as a
// construction parameter to PrefService.
//
// Currently, registrations after constructing the PrefService will
// also work, but this is being deprecated.
class PrefRegistry : public base::RefCounted<PrefRegistry> {
 public:
  typedef PrefValueMap::const_iterator const_iterator;
  typedef base::Callback<void(const char*, base::Value*)> RegistrationCallback;
  typedef base::Callback<void(const char*)> UnregistrationCallback;

  PrefRegistry();

  // Gets the registered defaults.
  scoped_refptr<PrefStore> defaults();

  // Allows iteration over defaults.
  const_iterator begin() const;
  const_iterator end() const;

  // Exactly one callback can be set for each of two events:
  // Registration and unregistration. If either is set, the callback
  // will be invoked each time registration and/or unregistration has
  // been performed on this object.
  //
  // Calling either of these methods after a callback has already been
  // set will make the object forget the previous callback and use the
  // new one instead.
  void SetRegistrationCallback(const RegistrationCallback& callback);
  void SetUnregistrationCallback(const UnregistrationCallback& callback);

  // Unregisters a preference. This is going away soon.
  virtual void DeprecatedUnregisterPreference(const char* path);

 protected:
  friend class base::RefCounted<PrefRegistry>;
  virtual ~PrefRegistry();

  // TODO(joi): Temporary until we have PrefRegistrySyncable.
  friend class PrefServiceSyncable;

  // Used by subclasses to register a default value for a preference.
  void RegisterPreference(const char* path, base::Value* default_value);

 private:
  RegistrationCallback registration_callback_;
  UnregistrationCallback unregistration_callback_;
  scoped_refptr<DefaultPrefStore> defaults_;

  DISALLOW_COPY_AND_ASSIGN(PrefRegistry);
};

#endif  // CHROME_BROWSER_PREFS_PREF_REGISTRY_H_
