// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_desktop_session.h"

#include "base/strings/utf_string_conversions.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/win/chromoting_module.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

RdpDesktopSession::RdpDesktopSession() {
}

STDMETHODIMP RdpDesktopSession::Connect(
    long width,
    long height,
    BSTR terminal_id,
    IRdpDesktopSessionEventHandler* event_handler) {
  event_handler_ = event_handler;

  scoped_refptr<AutoThreadTaskRunner> task_runner =
      ChromotingModule::task_runner();
  DCHECK(task_runner->BelongsToCurrentThread());

  client_.reset(new RdpClient(task_runner, task_runner,
                              webrtc::DesktopSize(width, height),
                              base::UTF16ToUTF8(terminal_id), this));
  return S_OK;
}

STDMETHODIMP RdpDesktopSession::Disconnect() {
  client_.reset();
  event_handler_ = NULL;
  return S_OK;
}

STDMETHODIMP RdpDesktopSession::ChangeResolution(long width, long height) {
  return E_NOTIMPL;
}

STDMETHODIMP RdpDesktopSession::InjectSas() {
  if (client_)
    client_->InjectSas();
  return S_OK;
}

void RdpDesktopSession::OnRdpConnected() {
  HRESULT result = event_handler_->OnRdpConnected();
  CHECK(SUCCEEDED(result)) << "OnRdpConnected() failed: 0x"
                           << std::hex << result << std::dec << ".";
}

void RdpDesktopSession::OnRdpClosed() {
  HRESULT result = event_handler_->OnRdpClosed();
  CHECK(SUCCEEDED(result)) << "OnRdpClosed() failed: 0x" << std::hex << result
                           << std::dec << ".";
}

} // namespace remoting
