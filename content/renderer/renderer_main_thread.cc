// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_thread.h"

#include "content/renderer/render_process.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

RendererMainThread::RendererMainThread(const std::string& channel_id)
    : Thread("Chrome_InProcRendererThread"), channel_id_(channel_id) {
}

RendererMainThread::~RendererMainThread() {
  Stop();
}

void RendererMainThread::Init() {
  render_process_.reset(new RenderProcessImpl());
  new RenderThreadImpl(channel_id_);
}

void RendererMainThread::CleanUp() {
  render_process_.reset();

  // It's a little lame to manually set this flag.  But the single process
  // RendererThread will receive the WM_QUIT.  We don't need to assert on
  // this thread, so just force the flag manually.
  // If we want to avoid this, we could create the InProcRendererThread
  // directly with _beginthreadex() rather than using the Thread class.
  // We used to set this flag in the Init function above. However there
  // other threads like WebThread which are created by this thread
  // which resets this flag. Please see Thread::StartWithOptions. Setting
  // this flag to true in Cleanup works around these problems.
  SetThreadWasQuitProperly(true);
}

base::Thread* CreateRendererMainThread(const std::string& channel_id) {
  return new RendererMainThread(channel_id);
}

}  // namespace content
