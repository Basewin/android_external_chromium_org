/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

#ifdef PROVIDES_SOCKET_API

int bind(int fd, const struct sockaddr* addr, socklen_t len) {
  return ki_bind(fd, addr, len);
}

#endif
