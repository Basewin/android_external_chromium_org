// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/webworkerclient_proxy.h"

#include "chrome/common/child_process.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/worker_messages.h"
#include "chrome/worker/worker_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWorker.h"

using WebKit::WebString;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

WebWorkerClientProxy::WebWorkerClientProxy(const GURL& url, int route_id)
    : url_(url),
      route_id_(route_id),
      ALLOW_THIS_IN_INITIALIZER_LIST(impl_(WebWorker::create(this))) {
  AddRef();
  WorkerThread::current()->AddRoute(route_id_, this);
  ChildProcess::current()->AddRefProcess();
}

WebWorkerClientProxy::~WebWorkerClientProxy() {
  WorkerThread::current()->RemoveRoute(route_id_);
  ChildProcess::current()->ReleaseProcess();
}

void WebWorkerClientProxy::postMessageToWorkerObject(
    const WebString& message) {
  Send(new WorkerHostMsg_PostMessageToWorkerObject(route_id_, message));
}

void WebWorkerClientProxy::postExceptionToWorkerObject(
    const WebString& error_message,
    int line_number,
    const WebString& source_url) {
  Send(new WorkerHostMsg_PostExceptionToWorkerObject(
      route_id_, error_message, line_number, source_url));
}

void WebWorkerClientProxy::postConsoleMessageToWorkerObject(
    int destination,
    int source,
    int level,
    const WebString& message,
    int line_number,
    const WebString& source_url) {
  Send(new WorkerHostMsg_PostConsoleMessageToWorkerObject(
      route_id_, destination, source, level,message, line_number, source_url));
}

void WebWorkerClientProxy::confirmMessageFromWorkerObject(
    bool has_pending_activity) {
  Send(new WorkerHostMsg_ConfirmMessageFromWorkerObject(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::reportPendingActivity(bool has_pending_activity) {
  Send(new WorkerHostMsg_ReportPendingActivity(
      route_id_, has_pending_activity));
}

void WebWorkerClientProxy::workerContextDestroyed() {
  Send(new WorkerHostMsg_WorkerContextDestroyed(route_id_));
  impl_ = NULL;

  WorkerThread::current()->message_loop()->ReleaseSoon(FROM_HERE, this);
}

bool WebWorkerClientProxy::Send(IPC::Message* message) {
  if (MessageLoop::current() != WorkerThread::current()->message_loop()) {
    WorkerThread::current()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebWorkerClientProxy::Send, message));
    return true;
  }

  return WorkerThread::current()->Send(message);
}

void WebWorkerClientProxy::OnMessageReceived(const IPC::Message& message) {
  if (!impl_)
    return;

  IPC_BEGIN_MESSAGE_MAP(WebWorkerClientProxy, message)
    IPC_MESSAGE_FORWARD(WorkerMsg_StartWorkerContext, impl_,
                        WebWorker::startWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_TerminateWorkerContext, impl_,
                        WebWorker::terminateWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_PostMessageToWorkerContext, impl_,
                        WebWorker::postMessageToWorkerContext)
    IPC_MESSAGE_FORWARD(WorkerMsg_WorkerObjectDestroyed, impl_,
                        WebWorker::workerObjectDestroyed)
  IPC_END_MESSAGE_MAP()
}
