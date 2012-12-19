// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"

namespace base {
class Time;
}

class GURL;

namespace content {

class BrowserPpapiHost;
class ResourceContext;

class PepperFlashBrowserHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashBrowserHost(BrowserPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);
  virtual ~PepperFlashBrowserHost();

  // ppapi::host::ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnMsgUpdateActivity(ppapi::host::HostMessageContext* host_context);
  int32_t OnMsgGetLocalTimeZoneOffset(
      ppapi::host::HostMessageContext* host_context,
      const base::Time& t);
  int32_t OnMsgGetLocalDataRestrictions(
      ppapi::host::HostMessageContext* context);

  void GetLocalDataRestrictions(ppapi::host::ReplyMessageContext reply_context,
                                const GURL& document_url,
                                const GURL& plugin_url,
                                ResourceContext* resource_context);

  BrowserPpapiHost* host_;
  int render_process_id_;
  ResourceContext* resource_context_;  // For fetching the Flash LSO settings.
  base::WeakPtrFactory<PepperFlashBrowserHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperFlashBrowserHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FLASH_BROWSER_HOST_H_
