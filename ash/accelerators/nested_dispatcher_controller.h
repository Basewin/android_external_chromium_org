// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_NESTED_DISPATCHER_CONTROLLER_H_
#define ASH_ACCELERATORS_NESTED_DISPATCHER_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/message_loop.h"
#include "ui/aura/client/dispatcher_client.h"

namespace ash {

// Creates a dispatcher which wraps another dispatcher.
// The outer dispatcher runs first and performs ash specific handling.
// If it does not consume the event it forwards the event to the nested
// dispatcher.
class ASH_EXPORT NestedDispatcherController
    : public aura::client::DispatcherClient {
 public:
  NestedDispatcherController();

  void RunWithDispatcher(MessageLoop::Dispatcher* dispatcher,
                         bool nestable_tasks_allowed);

 private:
  DISALLOW_COPY_AND_ASSIGN(NestedDispatcherController);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_NESTED_DISPATCHER_CONTROLLER_H_
