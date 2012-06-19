// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_fetcher.h"

#include "base/bind.h"
#include "content/common/net/url_request_user_data.h"
#include "net/url_request/url_fetcher.h"

namespace content {

namespace URLFetcher {

// We have to mark the definition as CONTENT_EXPORT, too, as the
// declaration isn't visible from here (since it's protected by an
// #ifdef).
CONTENT_EXPORT net::URLFetcher* Create(
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* d) {
  return net::URLFetcher::Create(url, request_type, d);
}

}  // namespace URLFetcher

namespace {

base::SupportsUserData::Data* CreateURLRequestUserData(
    int render_process_id,
    int render_view_id) {
  return new URLRequestUserData(render_process_id, render_view_id);
}

}  // namespace

void AssociateURLFetcherWithRenderView(net::URLFetcher* url_fetcher,
                                       const GURL& first_party_for_cookies,
                                       int render_process_id,
                                       int render_view_id) {
  url_fetcher->SetFirstPartyForCookies(first_party_for_cookies);
  url_fetcher->SetURLRequestUserData(
      URLRequestUserData::kUserDataKey,
      base::Bind(&CreateURLRequestUserData,
                 render_process_id, render_view_id));
}

}  // namespace content
