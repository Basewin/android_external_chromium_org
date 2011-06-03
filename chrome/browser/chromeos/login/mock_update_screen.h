// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_UPDATE_SCREEN_H_
#pragma once

#include "chrome/browser/chromeos/login/update_screen.h"
#include "chrome/browser/chromeos/login/update_screen_actor.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUpdateScreen : public UpdateScreen {
 public:
  MockUpdateScreen(ScreenObserver* screen_observer, UpdateScreenActor* actor);
  virtual ~MockUpdateScreen();

  MOCK_METHOD0(StartUpdate, void());
};

class MockUpdateScreenActor : public UpdateScreenActor {
 public:
  MockUpdateScreenActor();
  virtual ~MockUpdateScreenActor();

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(ShowManualRebootInfo, void());
  MOCK_METHOD1(SetProgress, void(int progress));
  MOCK_METHOD1(ShowCurtain, void(bool enable));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_UPDATE_SCREEN_H_
