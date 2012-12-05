// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SSL_CLIENT_CERTIFICATE_SELECTOR_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_SSL_CLIENT_CERTIFICATE_SELECTOR_COCOA_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"

@class SFChooseIdentityPanel;
class SSLClientAuthObserverCocoaBridge;

@interface SSLClientCertificateSelectorCocoa : NSObject {
 @private
  // The list of identities offered to the user.
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> identities_;
  // The corresponding list of certificates.
  std::vector<scoped_refptr<net::X509Certificate> > certificates_;
  // A C++ object to bridge SSLClientAuthObserver notifications to us.
  scoped_ptr<SSLClientAuthObserverCocoaBridge> observer_;
  scoped_nsobject<SFChooseIdentityPanel> panel_;
}

@property (readonly, nonatomic) SFChooseIdentityPanel* panel;

- (id)initWithNetworkSession:(const net::HttpNetworkSession*)networkSession
             certRequestInfo:(net::SSLCertRequestInfo*)certRequestInfo
                    callback:(const chrome::SelectCertificateCallback&)callback;
- (void)onNotification;
- (void)displayForWebContents:(content::WebContents*)webContents;

@end

#endif  // CHROME_BROWSER_UI_COCOA_SSL_CLIENT_CERTIFICATE_SELECTOR_COCOA_H_
