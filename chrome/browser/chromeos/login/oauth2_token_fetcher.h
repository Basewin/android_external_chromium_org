// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_TOKEN_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_TOKEN_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

// OAuth2TokenFetcher is used to convert authenticated cookie jar from the
// authentication profile into OAuth2 tokens and GAIA credentials that will be
// used to kick off other token retrieval tasks.
class OAuth2TokenFetcher : public base::SupportsWeakPtr<OAuth2TokenFetcher>,
                           public GaiaAuthConsumer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnOAuth2TokensAvailable(
        const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) = 0;
    virtual void OnOAuth2TokensFetchFailed() = 0;
  };

  OAuth2TokenFetcher(OAuth2TokenFetcher::Delegate* delegate,
                     net::URLRequestContextGetter* context_getter);
  virtual ~OAuth2TokenFetcher();

  void StartExchangeFromCookies();
  void StartExchangeFromAuthCode(const std::string& auth_code);

 private:
  // Decides how to proceed on GAIA |error|. If the error looks temporary,
  // retries |task| until max retry count is reached.
  // If retry count runs out, or error condition is unrecoverable, it runs
  // |error_handler|.
  void RetryOnError(const GoogleServiceAuthError& error,
                    const base::Closure& task,
                    const base::Closure& error_handler);

  // GaiaAuthConsumer overrides.
  virtual void OnClientOAuthSuccess(
      const GaiaAuthConsumer::ClientOAuthResult& result) OVERRIDE;
  virtual void OnClientOAuthFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  OAuth2TokenFetcher::Delegate* delegate_;
  GaiaAuthConsumer::ClientOAuthResult oauth_tokens_;
  GaiaAuthFetcher auth_fetcher_;

  // The retry counter. Increment this only when failure happened.
  int retry_count_;
  std::string auth_code_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OAUTH2_TOKEN_FETCHER_H_
