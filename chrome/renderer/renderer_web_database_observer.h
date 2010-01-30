// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEB_DATABASE_OBSERVER_H_
#define CHROME_RENDERER_RENDERER_WEB_DATABASE_OBSERVER_H_

#include "ipc/ipc_message.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabaseObserver.h"
#include "webkit/database/database_connections.h"

class RendererWebDatabaseObserver : public WebKit::WebDatabaseObserver {
 public:
  explicit RendererWebDatabaseObserver(IPC::Message::Sender* sender);
  virtual void databaseOpened(const WebKit::WebDatabase& database);
  virtual void databaseModified(const WebKit::WebDatabase& database);
  virtual void databaseClosed(const WebKit::WebDatabase& database);

  void WaitForAllDatabasesToClose();

 private:
  IPC::Message::Sender* sender_;
  bool waiting_for_dbs_to_close_;
  webkit_database::DatabaseConnections database_connections_;
};

#endif  // CHROME_RENDERER_RENDERER_WEB_DATABASE_OBSERVER_H_
