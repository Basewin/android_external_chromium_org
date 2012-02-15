// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_DEVICE_REF_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_DEVICE_REF_SHARED_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_device_ref_api.h"

namespace ppapi {

// IF YOU ADD STUFF TO THIS CLASS
// ==============================
// Be sure to add it to the STRUCT_TRAITS at the top of ppapi_messages.h.
struct PPAPI_SHARED_EXPORT DeviceRefData {
  DeviceRefData();

  PP_DeviceType_Dev type;
  std::string name;
  std::string id;
};

class PPAPI_SHARED_EXPORT PPB_DeviceRef_Shared
    : public Resource,
      public thunk::PPB_DeviceRef_API {
 public:
  PPB_DeviceRef_Shared(ResourceObjectType type,
                       PP_Instance instance,
                       const DeviceRefData& data);

  // Resource overrides.
  virtual PPB_DeviceRef_API* AsPPB_DeviceRef_API() OVERRIDE;

  // PPB_DeviceRef_API implementation.
  virtual const DeviceRefData& GetDeviceRefData() const OVERRIDE;
  virtual PP_DeviceType_Dev GetType() OVERRIDE;
  virtual PP_Var GetName() OVERRIDE;

  static PP_Resource CreateResourceArray(
      ResourceObjectType type,
      PP_Instance instance,
      const std::vector<DeviceRefData>& devices);

 private:
  DeviceRefData data_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PPB_DeviceRef_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_DEVICE_REF_SHARED_H_
