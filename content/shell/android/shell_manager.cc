// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/android/shell_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_content_browser_client.h"
#include "googleurl/src/gurl.h"
#include "jni/ShellManager_jni.h"

using base::android::ScopedJavaLocalRef;

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject> >
    g_content_shell_manager = LAZY_INSTANCE_INITIALIZER;

namespace content {

jobject CreateShellView() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ShellManager_createShell(
      env, g_content_shell_manager.Get().obj()).Release();
}

// Register native methods
bool RegisterShellManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void Init(JNIEnv* env, jclass clazz, jobject obj) {
  g_content_shell_manager.Get().Reset(
      base::android::ScopedJavaLocalRef<jobject>(env, obj));
}

void LaunchShell(JNIEnv* env, jclass clazz, jstring jurl) {
  ShellBrowserContext* browserContext =
      static_cast<ShellContentBrowserClient*>(
          GetContentClient()->browser())->browser_context();
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  Shell::CreateNewWindow(browserContext,
                         url,
                         NULL,
                         MSG_ROUTING_NONE,
                         NULL);
}

}  // namespace content
