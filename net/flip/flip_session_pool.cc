// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_session_pool.h"

#include "base/logging.h"
#include "net/flip/flip_session.h"

namespace net {

// The maximum number of sessions to open to a single domain.
static const size_t kMaxSessionsPerDomain = 1;

FlipSessionPool::FlipSessionPool() {}
FlipSessionPool::~FlipSessionPool() {
  CloseAllSessions();
}

FlipSession* FlipSessionPool::Get(const HostResolver::RequestInfo& info,
                                  HttpNetworkSession* session) {
  const std::string& domain = info.hostname();
  FlipSession* flip_session = NULL;
  FlipSessionList* list = GetSessionList(domain);
  if (list) {
    if (list->size() >= kMaxSessionsPerDomain) {
      flip_session = list->front();
      list->pop_front();
    }
  } else {
    list = AddSessionList(domain);
  }

  DCHECK(list);
  if (!flip_session) {
    flip_session = new FlipSession(domain, session);
    flip_session->AddRef();  // Keep it in the cache.
  }

  DCHECK(flip_session);
  list->push_back(flip_session);
  DCHECK(list->size() <= kMaxSessionsPerDomain);
  return flip_session;
}

bool FlipSessionPool::HasSession(const HostResolver::RequestInfo& info) const {
  const std::string& domain = info.hostname();
  if (GetSessionList(domain))
    return true;
  return false;
}

void FlipSessionPool::Remove(FlipSession* session) {
  std::string domain = session->domain();
  FlipSessionList* list = GetSessionList(domain);
  if (list == NULL)
    return;
  list->remove(session);
  if (list->empty())
    RemoveSessionList(domain);
}

FlipSessionPool::FlipSessionList*
    FlipSessionPool::AddSessionList(const std::string& domain) {
  DCHECK(sessions_.find(domain) == sessions_.end());
  return sessions_[domain] = new FlipSessionList();
}

FlipSessionPool::FlipSessionList*
    FlipSessionPool::GetSessionList(const std::string& domain) {
  FlipSessionsMap::iterator it = sessions_.find(domain);
  if (it == sessions_.end())
    return NULL;
  return it->second;
}

const FlipSessionPool::FlipSessionList*
    FlipSessionPool::GetSessionList(const std::string& domain) const {
  FlipSessionsMap::const_iterator it = sessions_.find(domain);
  if (it == sessions_.end())
    return NULL;
  return it->second;
}

void FlipSessionPool::RemoveSessionList(const std::string& domain) {
  FlipSessionList* list = GetSessionList(domain);
  if (list) {
    delete list;
    sessions_.erase(domain);
  } else {
    DCHECK(false) << "removing orphaned session list";
  }
}

void FlipSessionPool::CloseAllSessions() {
  while (sessions_.size()) {
    FlipSessionList* list = sessions_.begin()->second;
    DCHECK(list);
    sessions_.erase(sessions_.begin()->first);
    while (list->size()) {
      FlipSession* session = list->front();
      list->pop_front();
      session->CloseAllStreams(net::OK);
      session->Release();
    }
    delete list;
  }
}

}  // namespace net
