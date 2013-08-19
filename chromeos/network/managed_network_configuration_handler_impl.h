// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile_observer.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkConfigurationHandler;
struct NetworkProfile;
class NetworkProfileHandler;
class NetworkStateHandler;

class CHROMEOS_EXPORT ManagedNetworkConfigurationHandlerImpl
    : public ManagedNetworkConfigurationHandler,
      public NetworkProfileObserver {
 public:
  typedef std::map<std::string, const base::DictionaryValue*> GuidToPolicyMap;
  typedef std::map<std::string, GuidToPolicyMap> UserToPoliciesMap;

  virtual ~ManagedNetworkConfigurationHandlerImpl();

  // ManagedNetworkConfigurationHandler overrides
  virtual void AddObserver(NetworkPolicyObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NetworkPolicyObserver* observer) OVERRIDE;

  virtual void GetProperties(
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void GetManagedProperties(
      const std::string& userhash,
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  virtual void SetProperties(
      const std::string& service_path,
      const base::DictionaryValue& user_settings,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void CreateConfiguration(
      const std::string& userhash,
      const base::DictionaryValue& properties,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void RemoveConfiguration(
      const std::string& service_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void SetPolicy(onc::ONCSource onc_source,
                         const std::string& userhash,
                         const base::ListValue& network_configs_onc) OVERRIDE;

  virtual const base::DictionaryValue* FindPolicyByGUID(
      const std::string userhash,
      const std::string& guid,
      onc::ONCSource* onc_source) const OVERRIDE;

  virtual const base::DictionaryValue* FindPolicyByGuidAndProfile(
      const std::string& guid,
      const std::string& profile_path) const OVERRIDE;

  // NetworkProfileObserver overrides
  virtual void OnProfileAdded(const NetworkProfile& profile) OVERRIDE;
  virtual void OnProfileRemoved(const NetworkProfile& profile) OVERRIDE;

  NetworkConfigurationHandler* network_configuration_handler() {
    return network_configuration_handler_;
  }

 private:
  friend class ClientCertResolverTest;
  friend class NetworkHandler;
  friend class ManagedNetworkConfigurationHandlerTest;
  class PolicyApplicator;

  ManagedNetworkConfigurationHandlerImpl();

  void Init(NetworkStateHandler* network_state_handler,
            NetworkProfileHandler* network_profile_handler,
            NetworkConfigurationHandler* network_configuration_handler);

  void GetManagedPropertiesCallback(
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback,
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);

  const GuidToPolicyMap* GetPoliciesForUser(const std::string& userhash) const;
  const GuidToPolicyMap* GetPoliciesForProfile(
      const NetworkProfile& profile) const;

  void OnPolicyApplied(const std::string& service_path);

  // The DictionaryValues of the nested maps are owned by this class and are
  // explicitly deleted where necessary. If present, the empty string maps to
  // the device policy.
  UserToPoliciesMap policies_by_user_;

  // Local references to the associated handler instances.
  NetworkStateHandler* network_state_handler_;
  NetworkProfileHandler* network_profile_handler_;
  NetworkConfigurationHandler* network_configuration_handler_;

  ObserverList<NetworkPolicyObserver> observers_;

  // For Shill client callbacks
  base::WeakPtrFactory<ManagedNetworkConfigurationHandlerImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedNetworkConfigurationHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MANAGED_NETWORK_CONFIGURATION_HANDLER_IMPL_H_
