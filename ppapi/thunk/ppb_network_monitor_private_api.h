// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_NETWORK_MONITOR_PRIVATE_API_H_
#define PPAPI_THUNK_PPB_NETWORK_MONITOR_PRIVATE_API_H_

#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_NetworkMonitor_Private_API {
 public:
  virtual ~PPB_NetworkMonitor_Private_API() {}
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_NETWORK_MONITOR_PRIVATE_API_H_
