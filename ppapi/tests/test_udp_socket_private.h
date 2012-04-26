// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_H_
#define PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/private/udp_socket_private.h"
#include "ppapi/tests/test_case.h"

class TestUDPSocketPrivate : public TestCase {
 public:
  explicit TestUDPSocketPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string GetLocalAddress(PP_NetAddress_Private* address);
  std::string BindUDPSocket(pp::UDPSocketPrivate* socket,
                            PP_NetAddress_Private *address);
  std::string BindUDPSocketFailure(pp::UDPSocketPrivate* socket,
                                   PP_NetAddress_Private *address);

  std::string TestConnect();
  std::string TestConnectFailure();

  std::string host_;
  uint16_t port_;
};

#endif  // PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_H_
