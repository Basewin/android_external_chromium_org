// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_certificate_selector_test.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/cert_test_util.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/test_data_directory.h"
#include "net/base/x509_certificate.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using ::testing::Mock;
using ::testing::StrictMock;
using content::BrowserThread;

SSLClientCertificateSelectorTestBase::SSLClientCertificateSelectorTestBase()
    : io_loop_finished_event_(false, false) {
}

SSLClientCertificateSelectorTestBase::~SSLClientCertificateSelectorTestBase() {
}

void SSLClientCertificateSelectorTestBase::SetUpInProcessBrowserTestFixture() {
  FilePath certs_dir = net::GetTestCertsDirectory();

  mit_davidben_cert_ = net::ImportCertFromFile(certs_dir, "mit.davidben.der");
  ASSERT_TRUE(mit_davidben_cert_);

  foaf_me_chromium_test_cert_ = net::ImportCertFromFile(
      certs_dir, "foaf.me.chromium-test-cert.der");
  ASSERT_TRUE(foaf_me_chromium_test_cert_);

  cert_request_info_ = new net::SSLCertRequestInfo;
  cert_request_info_->host_and_port = "foo:123";
  cert_request_info_->client_certs.push_back(mit_davidben_cert_);
  cert_request_info_->client_certs.push_back(foaf_me_chromium_test_cert_);
}

void SSLClientCertificateSelectorTestBase::SetUpOnMainThread() {
  url_request_context_getter_ = browser()->profile()->GetRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SSLClientCertificateSelectorTestBase::SetUpOnIOThread,
                 this));

  io_loop_finished_event_.Wait();

  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Have to release our reference to the auth handler during the test to allow
// it to be destroyed while the Browser and its IO thread still exist.
void SSLClientCertificateSelectorTestBase::CleanUpOnMainThread() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SSLClientCertificateSelectorTestBase::CleanUpOnIOThread,
                 this));

  io_loop_finished_event_.Wait();

  auth_requestor_ = NULL;
}

void SSLClientCertificateSelectorTestBase::SetUpOnIOThread() {
  url_request_ = MakeURLRequest(url_request_context_getter_);

  auth_requestor_ = new StrictMock<SSLClientAuthRequestorMock>(
      url_request_,
      cert_request_info_);

  io_loop_finished_event_.Signal();
}

void SSLClientCertificateSelectorTestBase::CleanUpOnIOThread() {
  delete url_request_;

  io_loop_finished_event_.Signal();
}

net::URLRequest* SSLClientCertificateSelectorTestBase::MakeURLRequest(
    net::URLRequestContextGetter* context_getter) {
  net::URLRequest* request =
      context_getter->GetURLRequestContext()->CreateRequest(
          GURL("https://example"), NULL);
  return request;
}
