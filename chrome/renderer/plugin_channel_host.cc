// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugin_channel_host.h"

#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/npobject_base.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"

// A simple MessageFilter that will ignore all messages and respond to sync
// messages with an error when is_listening_ is false.
class IsListeningFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  IsListeningFilter() {}

  // MessageFilter overrides
  virtual void OnFilterRemoved() {}
  virtual void OnFilterAdded(IPC::Channel* channel) { channel_ = channel;  }
  virtual bool OnMessageReceived(const IPC::Message& message);

  static bool is_listening_;

 private:
  IPC::Channel* channel_;

  DISALLOW_EVIL_CONSTRUCTORS(IsListeningFilter);
};

bool IsListeningFilter::OnMessageReceived(const IPC::Message& message) {
  if (IsListeningFilter::is_listening_) {
    // Proceed with normal operation.
    return false;
  }

  // Always process message reply to prevent renderer from hanging on sync
  // messages.
  if (message.is_reply() || message.is_reply_error()) {
    return false;
  }

  // Reply to synchronous messages with an error (so they don't block while
  // we're not listening).
  if (message.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    channel_->Send(reply);
  }
  return true;
}

// static
bool IsListeningFilter::is_listening_ = true;

// static
bool PluginChannelHost::IsListening() {
  return IsListeningFilter::is_listening_;
}

// static
void PluginChannelHost::SetListening(bool flag) {
  IsListeningFilter::is_listening_ = flag;
}

PluginChannelHost* PluginChannelHost::GetPluginChannelHost(
    const std::string& channel_name, MessageLoop* ipc_message_loop) {
  PluginChannelHost* result =
      static_cast<PluginChannelHost*>(PluginChannelBase::GetChannel(
          channel_name,
          IPC::Channel::MODE_CLIENT,
          ClassFactory,
          ipc_message_loop,
          true));
  return result;
}

PluginChannelHost::PluginChannelHost() {
}

PluginChannelHost::~PluginChannelHost() {
}

bool PluginChannelHost::Init(MessageLoop* ipc_message_loop,
                             bool create_pipe_now) {
#if defined(OS_POSIX)
  if (!IPC::ChannelSocketExists(channel_name())) {
    // Attempting to use this IPC channel would result in a crash
    // inside IPC code within the PluginChannelBase::Init call.  The plugin
    // channel in the plugin process is supposed to have created this channel
    // and sent it to this process, the renderer process.  If this channel
    // closes and is removed, it cannot be reused until the plugin process
    // recreates it.
    LOG(ERROR) << "Refusing use of missing IPC channel " << channel_name();
    return false;
  }
#endif

  bool ret = PluginChannelBase::Init(ipc_message_loop, create_pipe_now);
  is_listening_filter_ = new IsListeningFilter;
  channel_->AddFilter(is_listening_filter_);
  return ret;
}

int PluginChannelHost::GenerateRouteID() {
  int route_id = MSG_ROUTING_NONE;
  Send(new PluginMsg_GenerateRouteID(&route_id));

  return route_id;
}

void PluginChannelHost::AddRoute(int route_id,
                                 IPC::Channel::Listener* listener,
                                 NPObjectBase* npobject) {
  PluginChannelBase::AddRoute(route_id, listener, npobject);

  if (!npobject)
    proxies_[route_id] = listener;
}

void PluginChannelHost::RemoveRoute(int route_id) {
  proxies_.erase(route_id);
  PluginChannelBase::RemoveRoute(route_id);
}

void PluginChannelHost::OnControlMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PluginChannelHost, message)
    IPC_MESSAGE_HANDLER(PluginHostMsg_SetException, OnSetException)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void PluginChannelHost::OnSetException(const std::string& message) {
  WebKit::WebBindings::setException(NULL, message.c_str());
}

void PluginChannelHost::OnChannelError() {
  PluginChannelBase::OnChannelError();

  for (ProxyMap::iterator iter = proxies_.begin();
       iter != proxies_.end(); iter++) {
    iter->second->OnChannelError();
  }

  proxies_.clear();
}
