// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_TEST_BASE_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_TEST_BASE_H_

#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

class ProfileResetter;

// The ProfileResetterMockObject is used to block the thread until
// ProfileResetter::Reset has completed:

// ProfileResetterMockObject mock_object;
// resetter_->Reset(ProfileResetter::ALL,
//                  base::Bind(&ProfileResetterMockObject::StopLoop,
//                             base::Unretained(&mock_object)));
// mock_object.RunLoop();
class ProfileResetterMockObject {
 public:
  ProfileResetterMockObject();
  ~ProfileResetterMockObject();

  void RunLoop();
  void StopLoop();

 private:
  MOCK_METHOD0(Callback, void(void));

  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetterMockObject);
};

// Base class for all ProfileResetter unit tests.
class ProfileResetterTestBase {
 public:
  ProfileResetterTestBase();
  ~ProfileResetterTestBase();

 protected:
  testing::StrictMock<ProfileResetterMockObject> mock_object_;
  scoped_ptr<ProfileResetter> resetter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileResetterTestBase);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_TEST_BASE_H_
