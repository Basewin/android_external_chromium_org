// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/networking_private_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/net/managed_network_configuration_handler.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"

using namespace chromeos;
namespace api = extensions::api::networking_private;

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction

NetworkingPrivateGetPropertiesFunction::
  ~NetworkingPrivateGetPropertiesFunction() {
}

bool NetworkingPrivateGetPropertiesFunction::RunImpl() {
  scoped_ptr<api::GetProperties::Params> params =
      api::GetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  // The |network_guid| parameter is storing the service path.
  std::string service_path = params->network_guid;

  ManagedNetworkConfigurationHandler::Get()->GetProperties(
      service_path,
      base::Bind(
          &NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess,
          this),
      base::Bind(&NetworkingPrivateGetPropertiesFunction::GetPropertiesFailed,
                 this));
  return true;
}

void NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess(
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  base::DictionaryValue* network_properties = dictionary.DeepCopy();
  network_properties->SetStringWithoutPathExpansion(onc::network_config::kGUID,
                                                    service_path);
  SetResult(network_properties);
  SendResponse(true);
}

void NetworkingPrivateGetPropertiesFunction::GetPropertiesFailed(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
~NetworkingPrivateGetVisibleNetworksFunction() {
}

bool NetworkingPrivateGetVisibleNetworksFunction::RunImpl() {
  scoped_ptr<api::GetVisibleNetworks::Params> params =
      api::GetVisibleNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string type_filter =
      api::GetVisibleNetworks::Params::ToString(params->type);

  NetworkStateHandler::NetworkStateList network_states;
  NetworkStateHandler::Get()->GetNetworkList(&network_states);

  base::ListValue* network_properties_list = new base::ListValue;
  for (NetworkStateHandler::NetworkStateList::iterator it =
           network_states.begin();
       it != network_states.end(); ++it) {
    const std::string& service_path = (*it)->path();
    base::DictionaryValue shill_dictionary;
    (*it)->GetProperties(&shill_dictionary);

    scoped_ptr<base::DictionaryValue> onc_network_part =
        onc::TranslateShillServiceToONCPart(shill_dictionary,
                                            &onc::kNetworkWithStateSignature);

    std::string onc_type;
    onc_network_part->GetStringWithoutPathExpansion(onc::network_config::kType,
                                                    &onc_type);
    if (type_filter == onc::network_type::kAllTypes ||
        onc_type == type_filter) {
      onc_network_part->SetStringWithoutPathExpansion(
          onc::network_config::kGUID,
          service_path);
      network_properties_list->Append(onc_network_part.release());
    }
  }

  SetResult(network_properties_list);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

NetworkingPrivateStartConnectFunction::
  ~NetworkingPrivateStartConnectFunction() {
}

void  NetworkingPrivateStartConnectFunction::ConnectionStartSuccess() {
  SendResponse(true);
}

void NetworkingPrivateStartConnectFunction::ConnectionStartFailed(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

bool NetworkingPrivateStartConnectFunction::RunImpl() {
  scoped_ptr<api::StartConnect::Params> params =
      api::StartConnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // The |network_guid| parameter is storing the service path.
  std::string service_path = params->network_guid;

  ManagedNetworkConfigurationHandler::Get()->Connect(
      service_path,
      base::Bind(
          &NetworkingPrivateStartConnectFunction::ConnectionStartSuccess,
          this),
      base::Bind(
          &NetworkingPrivateStartConnectFunction::ConnectionStartFailed,
          this));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

NetworkingPrivateStartDisconnectFunction::
  ~NetworkingPrivateStartDisconnectFunction() {
}

void  NetworkingPrivateStartDisconnectFunction::DisconnectionStartSuccess() {
  SendResponse(true);
}

void NetworkingPrivateStartDisconnectFunction::DisconnectionStartFailed(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

bool NetworkingPrivateStartDisconnectFunction::RunImpl() {
  scoped_ptr<api::StartDisconnect::Params> params =
      api::StartDisconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // The |network_guid| parameter is storing the service path.
  std::string service_path = params->network_guid;

  ManagedNetworkConfigurationHandler::Get()->Disconnect(
      service_path,
      base::Bind(
          &NetworkingPrivateStartDisconnectFunction::DisconnectionStartSuccess,
          this),
      base::Bind(
          &NetworkingPrivateStartDisconnectFunction::DisconnectionStartFailed,
          this));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyDestinationFunction

NetworkingPrivateVerifyDestinationFunction::
  ~NetworkingPrivateVerifyDestinationFunction() {
}

bool NetworkingPrivateVerifyDestinationFunction::RunImpl() {
  scoped_ptr<api::VerifyDestination::Params> params =
      api::VerifyDestination::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  DBusThreadManager::Get()->GetShillManagerClient()->VerifyDestination(
      params->properties.certificate,
      params->properties.public_key,
      params->properties.nonce,
      params->properties.signed_data,
      params->properties.device_serial,
      base::Bind(
          &NetworkingPrivateVerifyDestinationFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyDestinationFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyDestinationFunction::ResultCallback(
    bool result) {
  results_ = api::VerifyDestination::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyDestinationFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptCredentialsFunction

NetworkingPrivateVerifyAndEncryptCredentialsFunction::
  ~NetworkingPrivateVerifyAndEncryptCredentialsFunction() {
}

bool NetworkingPrivateVerifyAndEncryptCredentialsFunction::RunImpl() {
  scoped_ptr<api::VerifyAndEncryptCredentials::Params> params =
      api::VerifyAndEncryptCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  ShillManagerClient* shill_manager_client =
      DBusThreadManager::Get()->GetShillManagerClient();
  shill_manager_client->VerifyAndEncryptCredentials(
      params->properties.certificate,
      params->properties.public_key,
      params->properties.nonce,
      params->properties.signed_data,
      params->properties.device_serial,
      params->guid,
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptCredentialsFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptCredentialsFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyAndEncryptCredentialsFunction::ResultCallback(
    const std::string& result) {
  results_ = api::VerifyAndEncryptCredentials::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptCredentialsFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptDataFunction

NetworkingPrivateVerifyAndEncryptDataFunction::
  ~NetworkingPrivateVerifyAndEncryptDataFunction() {
}

bool NetworkingPrivateVerifyAndEncryptDataFunction::RunImpl() {
  scoped_ptr<api::VerifyAndEncryptData::Params> params =
      api::VerifyAndEncryptData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  DBusThreadManager::Get()->GetShillManagerClient()->VerifyAndEncryptData(
      params->properties.certificate,
      params->properties.public_key,
      params->properties.nonce,
      params->properties.signed_data,
      params->properties.device_serial,
      params->data,
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptDataFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptDataFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyAndEncryptDataFunction::ResultCallback(
    const std::string& result) {
  results_ = api::VerifyAndEncryptData::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptDataFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}
