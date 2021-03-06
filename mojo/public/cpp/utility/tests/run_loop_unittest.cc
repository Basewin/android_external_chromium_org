// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/utility/run_loop.h"

#include <string>

#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/cpp/utility/run_loop_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

class TestRunLoopHandler : public RunLoopHandler {
 public:
  TestRunLoopHandler()
      : ready_count_(0),
        error_count_(0),
        last_error_result_(MOJO_RESULT_OK) {
  }
  virtual ~TestRunLoopHandler() {}

  void clear_ready_count() { ready_count_ = 0; }
  int ready_count() const { return ready_count_; }

  void clear_error_count() { error_count_ = 0; }
  int error_count() const { return error_count_; }

  MojoResult last_error_result() const { return last_error_result_; }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    ready_count_++;
  }
  virtual void OnHandleError(const Handle& handle, MojoResult result)
      MOJO_OVERRIDE {
    error_count_++;
    last_error_result_ = result;
  }

 private:
  int ready_count_;
  int error_count_;
  MojoResult last_error_result_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestRunLoopHandler);
};

class RunLoopTest : public testing::Test {
 public:
  RunLoopTest() {}

  virtual void SetUp() MOJO_OVERRIDE {
    Test::SetUp();
    RunLoop::SetUp();
  }
  virtual void TearDown() MOJO_OVERRIDE {
    RunLoop::TearDown();
    Test::TearDown();
  }

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(RunLoopTest);
};

// Trivial test to verify Run() with no added handles returns.
TEST_F(RunLoopTest, ExitsWithNoHandles) {
  RunLoop run_loop;
  run_loop.Run();
}

class RemoveOnReadyRunLoopHandler : public TestRunLoopHandler {
 public:
  RemoveOnReadyRunLoopHandler() : run_loop_(NULL) {
  }
  virtual ~RemoveOnReadyRunLoopHandler() {}

  void set_run_loop(RunLoop* run_loop) { run_loop_ = run_loop; }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    run_loop_->RemoveHandler(handle);
    TestRunLoopHandler::OnHandleReady(handle);
  }

 private:
  RunLoop* run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RemoveOnReadyRunLoopHandler);
};

// Verifies RunLoop quits when no more handles (handle is removed when ready).
TEST_F(RunLoopTest, HandleReady) {
  RemoveOnReadyRunLoopHandler handler;
  MessagePipe test_pipe;
  EXPECT_TRUE(test::WriteTextMessage(test_pipe.handle1.get(), std::string()));

  RunLoop run_loop;
  handler.set_run_loop(&run_loop);
  run_loop.AddHandler(&handler, test_pipe.handle0.get(),
                      MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE);
  run_loop.Run();
  EXPECT_EQ(1, handler.ready_count());
  EXPECT_EQ(0, handler.error_count());
  EXPECT_FALSE(run_loop.HasHandler(test_pipe.handle0.get()));
}

class QuitOnReadyRunLoopHandler : public TestRunLoopHandler {
 public:
  QuitOnReadyRunLoopHandler() : run_loop_(NULL) {
  }
  virtual ~QuitOnReadyRunLoopHandler() {}

  void set_run_loop(RunLoop* run_loop) { run_loop_ = run_loop; }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    run_loop_->Quit();
    TestRunLoopHandler::OnHandleReady(handle);
  }

 private:
  RunLoop* run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(QuitOnReadyRunLoopHandler);
};

// Verifies Quit() from OnHandleReady() quits the loop.
TEST_F(RunLoopTest, QuitFromReady) {
  QuitOnReadyRunLoopHandler handler;
  MessagePipe test_pipe;
  EXPECT_TRUE(test::WriteTextMessage(test_pipe.handle1.get(), std::string()));

  RunLoop run_loop;
  handler.set_run_loop(&run_loop);
  run_loop.AddHandler(&handler, test_pipe.handle0.get(),
                      MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE);
  run_loop.Run();
  EXPECT_EQ(1, handler.ready_count());
  EXPECT_EQ(0, handler.error_count());
  EXPECT_TRUE(run_loop.HasHandler(test_pipe.handle0.get()));
}

class QuitOnErrorRunLoopHandler : public TestRunLoopHandler {
 public:
  QuitOnErrorRunLoopHandler() : run_loop_(NULL) {
  }
  virtual ~QuitOnErrorRunLoopHandler() {}

  void set_run_loop(RunLoop* run_loop) { run_loop_ = run_loop; }

  // RunLoopHandler:
  virtual void OnHandleError(const Handle& handle, MojoResult result)
      MOJO_OVERRIDE {
    run_loop_->Quit();
    TestRunLoopHandler::OnHandleError(handle, result);
  }

 private:
  RunLoop* run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(QuitOnErrorRunLoopHandler);
};

// Verifies Quit() when the deadline is reached works.
TEST_F(RunLoopTest, QuitWhenDeadlineExpired) {
  QuitOnErrorRunLoopHandler handler;
  MessagePipe test_pipe;
  RunLoop run_loop;
  handler.set_run_loop(&run_loop);
  run_loop.AddHandler(&handler, test_pipe.handle0.get(),
                      MOJO_HANDLE_SIGNAL_READABLE,
                      static_cast<MojoDeadline>(10000));
  run_loop.Run();
  EXPECT_EQ(0, handler.ready_count());
  EXPECT_EQ(1, handler.error_count());
  EXPECT_EQ(MOJO_RESULT_DEADLINE_EXCEEDED, handler.last_error_result());
  EXPECT_FALSE(run_loop.HasHandler(test_pipe.handle0.get()));
}

TEST_F(RunLoopTest, Current) {
  EXPECT_TRUE(RunLoop::current() == NULL);
  {
    RunLoop run_loop;
    EXPECT_EQ(&run_loop, RunLoop::current());
  }
  EXPECT_TRUE(RunLoop::current() == NULL);
}

// TODO(darin): Add tests for nested calls to RunLoop::Run(). See crbug/384633.

}  // namespace
}  // namespace mojo
