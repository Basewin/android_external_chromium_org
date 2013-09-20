// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <jni.h>
#include <string>

#include "base/android/jni_helper.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

class TokenService;


// A specialization of ProfileOAuth2TokenService that will be returned by
// ProfileOAuth2TokenServiceFactory for OS_ANDROID.  This instance uses
// native Android features to lookup OAuth2 tokens.
//
// See |ProfileOAuth2TokenService| for usage details.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use ProfileOAuth2TokenServiceRequest.
class AndroidProfileOAuth2TokenService : public ProfileOAuth2TokenService {
 public:
  virtual bool RefreshTokenIsAvailable(
      const std::string& account_id) OVERRIDE;

  // Registers the AndroidProfileOAuth2TokenService's native methods through
  // JNI.
  static bool Register(JNIEnv* env);

  // Lists account IDs of all accounts with a refresh token.
  virtual std::vector<std::string> GetAccounts() OVERRIDE;

 protected:
  friend class ProfileOAuth2TokenServiceFactory;
  AndroidProfileOAuth2TokenService();
  virtual ~AndroidProfileOAuth2TokenService();

  // Overridden from OAuth2TokenService to intercept token fetch requests and
  // redirect them to the Account Manager.
  virtual void FetchOAuth2Token(RequestImpl* request,
                                const std::string& account_id,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes) OVERRIDE;

  // Overridden from OAuth2TokenService to intercept token fetch requests and
  // redirect them to the Account Manager.
  virtual void InvalidateOAuth2Token(const std::string& account_id,
                                     const std::string& client_id,
                                     const ScopeSet& scopes,
                                     const std::string& access_token) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_PROFILE_OAUTH2_TOKEN_SERVICE_H_
