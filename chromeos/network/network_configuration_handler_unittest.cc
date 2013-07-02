  // Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/dbus/mock_shill_manager_client.h"
#include "chromeos/dbus/mock_shill_profile_client.h"
#include "chromeos/dbus/mock_shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

// Matcher to match base::Value.
MATCHER_P(IsEqualTo, value, "") { return arg.Equals(value); }

namespace chromeos {

namespace {

static std::string PrettyJson(const base::DictionaryValue& value) {
  std::string pretty;
  base::JSONWriter::WriteWithOptions(&value,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &pretty);
  return pretty;
}

void DictionaryValueCallback(
    const std::string& expected_id,
    const std::string& expected_json,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  std::string dict_str = PrettyJson(dictionary);
  EXPECT_EQ(expected_json, dict_str);
  EXPECT_EQ(expected_id, service_path);
}

void ErrorCallback(bool error_expected,
                   const std::string& expected_id,
                   const std::string& error_name,
                   scoped_ptr<base::DictionaryValue> error_data) {
  EXPECT_TRUE(error_expected) << "Unexpected error: " << error_name
      << " with associated data: \n"
      << PrettyJson(*error_data);
}

void StringResultCallback(const std::string& expected_result,
                          const std::string& result) {
  EXPECT_EQ(expected_result, result);
}

void DBusErrorCallback(const std::string& error_name,
                       const std::string& error_message) {
  EXPECT_TRUE(false) << "DBus Error: " << error_name << "("
      << error_message << ")";
}

class TestCallback {
 public:
  TestCallback() : run_count_(0) {}
  void Run() {
    ++run_count_;
  }
  int run_count() const { return run_count_; }

 private:
  int run_count_;
};

}  // namespace

class NetworkConfigurationHandlerTest : public testing::Test {
 public:
  NetworkConfigurationHandlerTest()
      : mock_manager_client_(NULL),
        mock_profile_client_(NULL),
        mock_service_client_(NULL),
        dictionary_value_result_(NULL) {}
  virtual ~NetworkConfigurationHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    MockDBusThreadManager* mock_dbus_thread_manager = new MockDBusThreadManager;
    EXPECT_CALL(*mock_dbus_thread_manager, GetSystemBus())
    .WillRepeatedly(Return(reinterpret_cast<dbus::Bus*>(NULL)));
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    mock_manager_client_ =
        mock_dbus_thread_manager->mock_shill_manager_client();
    mock_profile_client_ =
        mock_dbus_thread_manager->mock_shill_profile_client();
    mock_service_client_ =
        mock_dbus_thread_manager->mock_shill_service_client();

    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_configuration_handler_.reset(new NetworkConfigurationHandler());
    network_configuration_handler_->Init(network_state_handler_.get());
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    network_configuration_handler_.reset();
    network_state_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  // Handles responses for GetProperties method calls.
  void OnGetProperties(
      const dbus::ObjectPath& path,
      const ShillClientHelper::DictionaryValueCallback& callback) {
    callback.Run(DBUS_METHOD_CALL_SUCCESS, *dictionary_value_result_);
  }

  // Handles responses for SetProperties method calls.
  void OnSetProperties(const base::DictionaryValue& properties,
                       const ObjectPathCallback& callback,
                       const ShillClientHelper::ErrorCallback& error_callback) {
    callback.Run(dbus::ObjectPath());
  }

  // Handles responses for ClearProperties method calls.
  void OnClearProperties(
      const dbus::ObjectPath& service_path,
      const std::vector<std::string>& names,
      const ShillClientHelper::ListValueCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    base::ListValue result;
    result.AppendBoolean(true);
    callback.Run(result);
  }

  // Handles responses for ClearProperties method calls, and simulates an error
  // result.
  void OnClearPropertiesError(
      const dbus::ObjectPath& service_path,
      const std::vector<std::string>& names,
      const ShillClientHelper::ListValueCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) {
    base::ListValue result;
    result.AppendBoolean(false);
    callback.Run(result);
  }

  void OnGetService(const base::DictionaryValue& properties,
                    const ObjectPathCallback& callback,
                    const ShillClientHelper::ErrorCallback& error_callback) {
    callback.Run(dbus::ObjectPath("/service/2"));
  }

  void OnGetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      const ShillClientHelper::DictionaryValueCallback& callback) {
    base::DictionaryValue entries;
    entries.SetString("profile1", "entry1");
    entries.SetString("profile2", "entry2");
    callback.Run(DBUS_METHOD_CALL_SUCCESS, entries);
  }

  void OnDeleteEntry(const dbus::ObjectPath& profile_path,
                     const std::string& entry_path,
                     const base::Closure& callback,
                     const ShillClientHelper::ErrorCallback& error_callback) {
    // Don't run the callback immediately to emulate actual behavior.
    message_loop_.PostTask(FROM_HERE, callback);
  }

  bool PendingProfileEntryDeleterForTest(const std::string& service_path) {
    return network_configuration_handler_->
        PendingProfileEntryDeleterForTest(service_path);
  }

 protected:
  MockShillManagerClient* mock_manager_client_;
  MockShillProfileClient* mock_profile_client_;
  MockShillServiceClient* mock_service_client_;
  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  base::MessageLoop message_loop_;
  base::DictionaryValue* dictionary_value_result_;
};

TEST_F(NetworkConfigurationHandlerTest, GetProperties) {
  std::string service_path = "/service/1";
  std::string expected_json = "{\n   \"SSID\": \"MyNetwork\"\n}\n";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  scoped_ptr<base::StringValue> networkNameValue(
      base::Value::CreateStringValue(networkName));

  base::DictionaryValue value;
  value.Set(key, base::Value::CreateStringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_service_client_,
              SetProperty(dbus::ObjectPath(service_path), key,
                          IsEqualTo(networkNameValue.get()), _, _)).Times(1);
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(service_path), key, *networkNameValue,
      base::Bind(&base::DoNothing),
      base::Bind(&DBusErrorCallback));
  message_loop_.RunUntilIdle();

  ShillServiceClient::DictionaryValueCallback get_properties_callback;
  EXPECT_CALL(*mock_service_client_,
              GetProperties(_, _)).WillOnce(
                  Invoke(this,
                         &NetworkConfigurationHandlerTest::OnGetProperties));
  network_configuration_handler_->GetProperties(
      service_path,
      base::Bind(&DictionaryValueCallback,
                 service_path,
                 expected_json),
                 base::Bind(&ErrorCallback, false, service_path));
  message_loop_.RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, SetProperties) {
  std::string service_path = "/service/1";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  scoped_ptr<base::StringValue> networkNameValue(
      base::Value::CreateStringValue(networkName));

  base::DictionaryValue value;
  value.Set(key, base::Value::CreateStringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_manager_client_,
              ConfigureService(_, _, _)).WillOnce(
                  Invoke(this,
                         &NetworkConfigurationHandlerTest::OnSetProperties));
  network_configuration_handler_->SetProperties(
      service_path,
      value,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  message_loop_.RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, ClearProperties) {
  std::string service_path = "/service/1";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  scoped_ptr<base::StringValue> networkNameValue(
      base::Value::CreateStringValue(networkName));

  // First set up a value to clear.
  base::DictionaryValue value;
  value.Set(key, base::Value::CreateStringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_manager_client_,
              ConfigureService(_, _, _)).WillOnce(
                  Invoke(this,
                         &NetworkConfigurationHandlerTest::OnSetProperties));
  network_configuration_handler_->SetProperties(
      service_path,
      value,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  message_loop_.RunUntilIdle();

  // Now clear it.
  std::vector<std::string> values_to_clear;
  values_to_clear.push_back(key);
  EXPECT_CALL(*mock_service_client_,
              ClearProperties(_, _, _, _)).WillOnce(
                  Invoke(this,
                         &NetworkConfigurationHandlerTest::OnClearProperties));
  network_configuration_handler_->ClearProperties(
      service_path,
      values_to_clear,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  message_loop_.RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, ClearPropertiesError) {
  std::string service_path = "/service/1";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  scoped_ptr<base::StringValue> networkNameValue(
      base::Value::CreateStringValue(networkName));

  // First set up a value to clear.
  base::DictionaryValue value;
  value.Set(key, base::Value::CreateStringValue(networkName));
  dictionary_value_result_ = &value;
  EXPECT_CALL(*mock_manager_client_,
              ConfigureService(_, _, _)).WillOnce(
                  Invoke(this,
                         &NetworkConfigurationHandlerTest::OnSetProperties));
  network_configuration_handler_->SetProperties(
      service_path,
      value,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, false, service_path));
  message_loop_.RunUntilIdle();

  // Now clear it.
  std::vector<std::string> values_to_clear;
  values_to_clear.push_back(key);
  EXPECT_CALL(
      *mock_service_client_,
      ClearProperties(_, _, _, _)).WillOnce(
          Invoke(this,
                 &NetworkConfigurationHandlerTest::OnClearPropertiesError));
  network_configuration_handler_->ClearProperties(
      service_path,
      values_to_clear,
      base::Bind(&base::DoNothing),
      base::Bind(&ErrorCallback, true, service_path));
  message_loop_.RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, CreateConfiguration) {
  std::string expected_json = "{\n   \"SSID\": \"MyNetwork\"\n}\n";
  std::string networkName = "MyNetwork";
  std::string key = "SSID";
  scoped_ptr<base::StringValue> networkNameValue(
      base::Value::CreateStringValue(networkName));
  base::DictionaryValue value;
  value.Set(key, base::Value::CreateStringValue(networkName));

  EXPECT_CALL(
      *mock_manager_client_,
      GetService(_, _, _)).WillOnce(
          Invoke(this,
                 &NetworkConfigurationHandlerTest::OnGetService));
  network_configuration_handler_->CreateConfiguration(
      value,
      base::Bind(&StringResultCallback, std::string("/service/2")),
      base::Bind(&ErrorCallback, false, std::string("")));
  message_loop_.RunUntilIdle();
}

TEST_F(NetworkConfigurationHandlerTest, RemoveConfiguration) {
  std::string service_path = "/service/1";

  TestCallback test_callback;
  EXPECT_CALL(
      *mock_service_client_,
      GetLoadableProfileEntries(_, _)).WillOnce(Invoke(
          this,
          &NetworkConfigurationHandlerTest::OnGetLoadableProfileEntries));
  EXPECT_CALL(
      *mock_profile_client_,
      DeleteEntry(_, _, _, _)).WillRepeatedly(Invoke(
          this,
          &NetworkConfigurationHandlerTest::OnDeleteEntry));

  network_configuration_handler_->RemoveConfiguration(
      service_path,
      base::Bind(&TestCallback::Run, base::Unretained(&test_callback)),
      base::Bind(&ErrorCallback, false, service_path));
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, test_callback.run_count());
  EXPECT_FALSE(PendingProfileEntryDeleterForTest(service_path));
}

}  // namespace chromeos
