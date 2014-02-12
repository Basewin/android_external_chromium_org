// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/usb_midi_device_android.h"

#include <jni.h>
#include <vector>

#include "base/android/jni_array.h"
#include "jni/UsbMidiDeviceAndroid_jni.h"

namespace media {

UsbMidiDeviceAndroid::UsbMidiDeviceAndroid(ObjectRef raw_device,
                                           UsbMidiDeviceDelegate* delegate)
    : raw_device_(raw_device), delegate_(delegate) {}

UsbMidiDeviceAndroid::~UsbMidiDeviceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UsbMidiDeviceAndroid_close(env, raw_device_.obj());
}

std::vector<uint8> UsbMidiDeviceAndroid::GetDescriptor() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> descriptors =
      Java_UsbMidiDeviceAndroid_getDescriptors(env, raw_device_.obj());

  std::vector<uint8> ret;
  base::android::JavaByteArrayToByteVector(env, descriptors.obj(), &ret);
  return ret;
}

void UsbMidiDeviceAndroid::Send(int endpoint_number,
                                const std::vector<uint8>& data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const uint8* head = data.size() ? &data[0] : NULL;
  ScopedJavaLocalRef<jbyteArray> data_to_pass =
      base::android::ToJavaByteArray(env, head, data.size());

  Java_UsbMidiDeviceAndroid_send(
      env, raw_device_.obj(), endpoint_number, data_to_pass.obj());
}

bool UsbMidiDeviceAndroid::RegisterUsbMidiDevice(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace media
