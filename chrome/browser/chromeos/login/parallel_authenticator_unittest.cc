// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/parallel_authenticator.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cryptohome/mock_async_method_caller.h"
#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"
#include "chrome/browser/chromeos/login/mock_url_fetchers.h"
#include "chrome/browser/chromeos/login/test_attempt_state.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/mock_url_fetcher_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;
using file_util::CloseFile;
using file_util::CreateAndOpenTemporaryFile;
using file_util::CreateAndOpenTemporaryFileInDir;
using file_util::Delete;
using file_util::WriteFile;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

class TestOnlineAttempt : public OnlineAttempt {
 public:
  TestOnlineAttempt(AuthAttemptState* state,
                    AuthAttemptStateResolver* resolver)
      : OnlineAttempt(false, state, resolver) {
  }
};

class ParallelAuthenticatorTest : public testing::Test {
 public:
  ParallelAuthenticatorTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO),
        username_("me@nowhere.org"),
        password_("fakepass") {
    hash_ascii_.assign("0a010000000000a0");
    hash_ascii_.append(std::string(16, '0'));
  }

  ~ParallelAuthenticatorTest() {
    DCHECK(!mock_caller_);
  }

  virtual void SetUp() {
    mock_caller_ = new cryptohome::MockAsyncMethodCaller;
    cryptohome::AsyncMethodCaller::InitializeForTesting(mock_caller_);

    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();

    loader_ = new MockLibraryLoader();
    ON_CALL(*loader_, Load(_))
        .WillByDefault(Return(true));
    EXPECT_CALL(*loader_, Load(_))
        .Times(AnyNumber());

    test_api->SetLibraryLoader(loader_, true);

    mock_library_ = new MockCryptohomeLibrary();
    test_api->SetCryptohomeLibrary(mock_library_, true);
    io_thread_.Start();

    auth_ = new ParallelAuthenticator(&consumer_);
    auth_->set_using_oauth(false);
    state_.reset(new TestAttemptState(username_,
                                      password_,
                                      hash_ascii_,
                                      "",
                                      "",
                                      false));
  }

  // Tears down the test fixture.
  virtual void TearDown() {
    // Prevent bogus gMock leak check from firing.
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api->SetLibraryLoader(NULL, false);
    test_api->SetCryptohomeLibrary(NULL, false);

    cryptohome::AsyncMethodCaller::Shutdown();
    mock_caller_ = NULL;
  }

  FilePath PopulateTempFile(const char* data, int data_len) {
    FilePath out;
    FILE* tmp_file = CreateAndOpenTemporaryFile(&out);
    EXPECT_NE(tmp_file, static_cast<FILE*>(NULL));
    EXPECT_EQ(WriteFile(out, data, data_len), data_len);
    EXPECT_TRUE(CloseFile(tmp_file));
    return out;
  }

  // Allow test to fail and exit gracefully, even if OnLoginFailure()
  // wasn't supposed to happen.
  void FailOnLoginFailure() {
    ON_CALL(consumer_, OnLoginFailure(_))
        .WillByDefault(Invoke(MockConsumer::OnFailQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if
  // OnDemoUserLoginSuccess() wasn't supposed to happen.
  void FailOnDemoUserLoginSuccess() {
    ON_CALL(consumer_, OnDemoUserLoginSuccess())
        .WillByDefault(Invoke(MockConsumer::OnDemoUserSuccessQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if OnLoginSuccess()
  // wasn't supposed to happen.
  void FailOnLoginSuccess() {
    ON_CALL(consumer_, OnLoginSuccess(_, _, _, _))
        .WillByDefault(Invoke(MockConsumer::OnSuccessQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if
  // OnOffTheRecordLoginSuccess() wasn't supposed to happen.
  void FailOnGuestLoginSuccess() {
    ON_CALL(consumer_, OnOffTheRecordLoginSuccess())
        .WillByDefault(Invoke(MockConsumer::OnGuestSuccessQuitAndFail));
  }

  void ExpectLoginFailure(const LoginFailure& failure) {
    EXPECT_CALL(consumer_, OnLoginFailure(failure))
        .WillOnce(Invoke(MockConsumer::OnFailQuit))
        .RetiresOnSaturation();
  }

  void ExpectDemoUserLoginSuccess() {
    EXPECT_CALL(consumer_, OnDemoUserLoginSuccess())
        .WillOnce(Invoke(MockConsumer::OnDemoUserSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectLoginSuccess(const std::string& username,
                          const std::string& password,
                          bool pending) {
    EXPECT_CALL(consumer_, OnLoginSuccess(username, password, pending,
                                          false))
        .WillOnce(Invoke(MockConsumer::OnSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectGuestLoginSuccess() {
    EXPECT_CALL(consumer_, OnOffTheRecordLoginSuccess())
        .WillOnce(Invoke(MockConsumer::OnGuestSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectPasswordChange() {
    EXPECT_CALL(consumer_, OnPasswordChangeDetected())
        .WillOnce(Invoke(MockConsumer::OnMigrateQuit))
        .RetiresOnSaturation();
  }

  void RunResolve(ParallelAuthenticator* auth) {
    auth->Resolve();
    message_loop_.RunAllPending();
  }

  void SetAttemptState(ParallelAuthenticator* auth, TestAttemptState* state) {
    auth->set_attempt_state(state);
  }

  ParallelAuthenticator::AuthState SetAndResolveState(
      ParallelAuthenticator* auth, TestAttemptState* state) {
    auth->set_attempt_state(state);
    return auth->ResolveState();
  }

  void FakeOnlineAttempt() {
    auth_->set_online_attempt(new TestOnlineAttempt(state_.get(), auth_.get()));
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  std::string username_;
  std::string password_;
  std::string hash_ascii_;

  // Initializes / shuts down a stub CrosLibrary.
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;

  // Mocks, destroyed by CrosLibrary class.
  MockCryptohomeLibrary* mock_library_;
  MockLibraryLoader* loader_;

  cryptohome::MockAsyncMethodCaller* mock_caller_;

  MockConsumer consumer_;
  scoped_refptr<ParallelAuthenticator> auth_;
  scoped_ptr<TestAttemptState> state_;
};

TEST_F(ParallelAuthenticatorTest, OnLoginSuccess) {
  EXPECT_CALL(consumer_, OnLoginSuccess(username_, password_, false, false))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_, state_.release());
  auth_->OnLoginSuccess(false);
}

TEST_F(ParallelAuthenticatorTest, OnPasswordChangeDetected) {
  EXPECT_CALL(consumer_, OnPasswordChangeDetected())
      .Times(1)
      .RetiresOnSaturation();
  SetAttemptState(auth_, state_.release());
  auth_->OnPasswordChangeDetected();
}

TEST_F(ParallelAuthenticatorTest, ResolveNothingDone) {
  EXPECT_EQ(ParallelAuthenticator::CONTINUE,
            SetAndResolveState(auth_, state_.release()));
}

TEST_F(ParallelAuthenticatorTest, ResolvePossiblePwChange) {
  // Set a fake online attempt so that we return intermediate cryptohome state.
  FakeOnlineAttempt();

  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected.
  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);

  EXPECT_EQ(ParallelAuthenticator::POSSIBLE_PW_CHANGE,
            SetAndResolveState(auth_, state_.release()));
}

TEST_F(ParallelAuthenticatorTest, ResolvePossiblePwChangeToFailedMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected.
  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);

  // When there is no online attempt and online results, POSSIBLE_PW_CHANGE
  EXPECT_EQ(ParallelAuthenticator::FAILED_MOUNT,
            SetAndResolveState(auth_, state_.release()));
}

TEST_F(ParallelAuthenticatorTest, ResolveNeedOldPw) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because of unmatched key; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);
  state_->PresetOnlineLoginStatus(LoginFailure::None());

  EXPECT_EQ(ParallelAuthenticator::NEED_OLD_PW,
            SetAndResolveState(auth_, state_.release()));
}

TEST_F(ParallelAuthenticatorTest, DriveFailedMount) {
  FailOnLoginSuccess();
  ExpectLoginFailure(LoginFailure(LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME));

  // Set up state as though a cryptohome mount attempt has occurred
  // and failed.
  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_NONE);
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get());
}

TEST_F(ParallelAuthenticatorTest, DriveGuestLogin) {
  ExpectGuestLoginSuccess();
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond as though a tmpfs mount
  // attempt has occurred and succeeded.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_))
      .Times(1)
      .RetiresOnSaturation();

  auth_->LoginOffTheRecord();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveGuestLoginButFail) {
  FailOnGuestLoginSuccess();
  ExpectLoginFailure(LoginFailure(LoginFailure::COULD_NOT_MOUNT_TMPFS));

  // Set up mock cryptohome library to respond as though a tmpfs mount
  // attempt has occurred and failed.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_))
      .Times(1)
      .RetiresOnSaturation();

  auth_->LoginOffTheRecord();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveDemoUserLogin) {
  ExpectDemoUserLoginSuccess();
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond as though a tmpfs mount
  // attempt has occurred and succeeded.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_))
      .Times(1)
      .RetiresOnSaturation();

  auth_->LoginDemoUser();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveDemoUserLoginButFail) {
  FailOnDemoUserLoginSuccess();
  ExpectLoginFailure(LoginFailure(LoginFailure::COULD_NOT_MOUNT_TMPFS));

  // Set up mock cryptohome library to respond as though a tmpfs mount
  // attempt has occurred and failed.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_))
      .Times(1)
      .RetiresOnSaturation();

  auth_->LoginDemoUser();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveDataResync) {
  ExpectLoginSuccess(username_, password_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a cryptohome
  // remove attempt and a cryptohome create attempt (specified by the |true|
  // argument to AsyncMount).
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncRemove(username_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_caller_, AsyncMount(username_, hash_ascii_, true, _))
      .Times(1)
      .RetiresOnSaturation();

  state_->PresetOnlineLoginStatus(LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  auth_->ResyncEncryptedData();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveResyncFail) {
  FailOnLoginSuccess();
  ExpectLoginFailure(LoginFailure(LoginFailure::DATA_REMOVAL_FAILED));

  // Set up mock cryptohome library to fail a cryptohome remove attempt.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncRemove(username_, _))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_, state_.release());

  auth_->ResyncEncryptedData();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveRequestOldPassword) {
  FailOnLoginSuccess();
  ExpectPasswordChange();

  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);
  state_->PresetOnlineLoginStatus(LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get());
}

TEST_F(ParallelAuthenticatorTest, DriveDataRecover) {
  ExpectLoginSuccess(username_, password_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a key migration.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMigrateKey(username_, _, hash_ascii_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_caller_, AsyncMount(username_, hash_ascii_, false, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  state_->PresetOnlineLoginStatus(LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  auth_->RecoverEncryptedData(std::string());
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveDataRecoverButFail) {
  FailOnLoginSuccess();
  ExpectPasswordChange();

  // Set up mock cryptohome library to fail a key migration attempt,
  // asserting that the wrong password was used.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);
  EXPECT_CALL(*mock_caller_, AsyncMigrateKey(username_, _, hash_ascii_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  SetAttemptState(auth_, state_.release());

  auth_->RecoverEncryptedData(std::string());
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, ResolveNoMount) {
  // Set a fake online attempt so that we return intermediate cryptohome state.
  FakeOnlineAttempt();

  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist.
  state_->PresetCryptohomeStatus(false,
                                 cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST);

  EXPECT_EQ(ParallelAuthenticator::NO_MOUNT,
            SetAndResolveState(auth_, state_.release()));
}

TEST_F(ParallelAuthenticatorTest, ResolveNoMountToFailedMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist.
  state_->PresetCryptohomeStatus(false,
                                 cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST);

  // When there is no online attempt and online results, NO_MOUNT will be
  // resolved to FAILED_MOUNT.
  EXPECT_EQ(ParallelAuthenticator::FAILED_MOUNT,
            SetAndResolveState(auth_, state_.release()));
}

TEST_F(ParallelAuthenticatorTest, ResolveCreateNew) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(false,
                                 cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST);
  state_->PresetOnlineLoginStatus(LoginFailure::None());

  EXPECT_EQ(ParallelAuthenticator::CREATE_NEW,
            SetAndResolveState(auth_, state_.release()));
}

TEST_F(ParallelAuthenticatorTest, DriveCreateForNewUser) {
  ExpectLoginSuccess(username_, password_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a cryptohome
  // create attempt (specified by the |true| argument to AsyncMount).
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMount(username_, hash_ascii_, true, _))
      .Times(1)
      .RetiresOnSaturation();

  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(false,
                                 cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST);
  state_->PresetOnlineLoginStatus(LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get());
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLogin) {
  ExpectLoginSuccess(username_, password_, false);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_CONNECTION_RESET);
  state_->PresetOnlineLoginStatus(LoginFailure::FromNetworkAuthFailure(error));
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get());
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLoginDelayedOnline) {
  ExpectLoginSuccess(username_, password_, true);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  // state_ is released further down.
  SetAttemptState(auth_, state_.get());
  RunResolve(auth_.get());

  // Offline login has completed, so now we "complete" the online request.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  state_.release()->PresetOnlineLoginStatus(failure);
  ExpectLoginFailure(failure);

  RunResolve(auth_.get());
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLoginGetNewPassword) {
  ExpectLoginSuccess(username_, password_, true);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a key migration.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMigrateKey(username_,
                                              state_->ascii_hash,
                                              _,
                                              _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded; also, an online request that never made it.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  // state_ is released further down.
  SetAttemptState(auth_, state_.get());
  RunResolve(auth_.get());

  // Offline login has completed, so now we "complete" the online request.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  state_.release()->PresetOnlineLoginStatus(failure);
  ExpectLoginFailure(failure);

  RunResolve(auth_.get());

  // After the request below completes, OnLoginSuccess gets called again.
  ExpectLoginSuccess(username_, password_, false);

  MockURLFetcherFactory<SuccessFetcher> factory;
  TestingProfile profile;

  auth_->RetryAuth(&profile,
                   username_,
                   std::string(),
                   std::string(),
                   std::string());
  message_loop_.Run();
  message_loop_.RunAllPending();
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLoginGetCaptchad) {
  ExpectLoginSuccess(username_, password_, true);
  FailOnLoginFailure();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded; also, an online request that never made it.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  // state_ is released further down.
  SetAttemptState(auth_, state_.get());
  RunResolve(auth_.get());

  // Offline login has completed, so now we "complete" the online request.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  state_.release()->PresetOnlineLoginStatus(failure);
  ExpectLoginFailure(failure);

  RunResolve(auth_.get());

  // After the request below completes, OnLoginSuccess gets called again.
  failure = LoginFailure::FromNetworkAuthFailure(
      GoogleServiceAuthError::FromCaptchaChallenge(
          CaptchaFetcher::GetCaptchaToken(),
          GURL(CaptchaFetcher::GetCaptchaUrl()),
          GURL(CaptchaFetcher::GetUnlockUrl())));
  ExpectLoginFailure(failure);

  MockURLFetcherFactory<CaptchaFetcher> factory;
  TestingProfile profile;

  auth_->RetryAuth(&profile,
                   username_,
                   std::string(),
                   std::string(),
                   std::string());
  message_loop_.Run();
  message_loop_.RunAllPending();
}

TEST_F(ParallelAuthenticatorTest, DriveOnlineLogin) {
  ExpectLoginSuccess(username_, password_, false);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  state_->PresetOnlineLoginStatus(LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get());
}

// http://crbug.com/106538
TEST_F(ParallelAuthenticatorTest, DISABLED_DriveNeedNewPassword) {
  FailOnLoginSuccess();  // Set failing on success as the default...
  // ...but expect ONE successful login first.
  ExpectLoginSuccess(username_, password_, true);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  ExpectLoginFailure(failure);

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  state_->PresetOnlineLoginStatus(failure);
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get());
}

TEST_F(ParallelAuthenticatorTest, DriveUnlock) {
  ExpectLoginSuccess(username_, std::string(), false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a cryptohome
  // key-check attempt.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncCheckKey(username_, _, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  auth_->AuthenticateToUnlock(username_, "");
  message_loop_.Run();
}

}  // namespace chromeos
