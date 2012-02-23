// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pepper_session_manager.h"

#include "base/bind.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/jingle_info_request.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/pepper_session.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;

namespace remoting {
namespace protocol {

PepperSessionManager::PepperSessionManager(
    scoped_ptr<TransportFactory> transport_factory)
    : transport_factory_(transport_factory.Pass()),
      signal_strategy_(NULL),
      listener_(NULL),
      ready_(false) {
}

PepperSessionManager::~PepperSessionManager() {
  Close();
}

void PepperSessionManager::Init(
    SignalStrategy* signal_strategy,
    SessionManager::Listener* listener,
    const NetworkSettings& network_settings) {
  listener_ = listener;
  signal_strategy_ = signal_strategy;
  iq_sender_.reset(new IqSender(signal_strategy_));

  transport_config_.nat_traversal = network_settings.allow_nat_traversal;
  transport_config_.min_port = network_settings.min_port;
  transport_config_.max_port = network_settings.max_port;

  signal_strategy_->AddListener(this);

  OnSignalStrategyStateChange(signal_strategy_->GetState());
}

void PepperSessionManager::OnJingleInfo(
    const std::string& relay_token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<talk_base::SocketAddress>& stun_hosts) {
  DCHECK(CalledOnValidThread());

  // TODO(sergeyu): Add support for multiple STUN/relay servers when
  // it's implemented in libjingle and P2P Transport API.
  transport_config_.stun_server = stun_hosts[0].ToString();
  transport_config_.relay_server = relay_hosts[0];
  transport_config_.relay_token = relay_token;
  VLOG(1) << "STUN server: " << transport_config_.stun_server
          << " Relay server: " << transport_config_.relay_server
          << " Relay token: " << transport_config_.relay_token;

  if (!ready_) {
    ready_ = true;
    listener_->OnSessionManagerReady();
  }
}

scoped_ptr<Session> PepperSessionManager::Connect(
    const std::string& host_jid,
    scoped_ptr<Authenticator> authenticator,
    scoped_ptr<CandidateSessionConfig> config,
    const Session::StateChangeCallback& state_change_callback) {
  scoped_ptr<PepperSession> session(new PepperSession(this));
  session->StartConnection(host_jid, authenticator.Pass(), config.Pass(),
                           state_change_callback);
  sessions_[session->session_id_] = session.get();
  return session.PassAs<Session>();
}

void PepperSessionManager::Close() {
  DCHECK(CalledOnValidThread());

  // Close() can be called only after all sessions are destroyed.
  DCHECK(sessions_.empty());

  listener_ = NULL;
  jingle_info_request_.reset();

  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
    signal_strategy_ = NULL;
  }
}

void PepperSessionManager::set_authenticator_factory(
    scoped_ptr<AuthenticatorFactory> authenticator_factory) {
  DCHECK(CalledOnValidThread());
  authenticator_factory_ = authenticator_factory.Pass();
}

void PepperSessionManager::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  // If NAT traversal is enabled then we need to request STUN/Relay info.
  if (state == SignalStrategy::CONNECTED) {
    if (transport_config_.nat_traversal) {
      jingle_info_request_.reset(new JingleInfoRequest(signal_strategy_));
      jingle_info_request_->Send(base::Bind(&PepperSessionManager::OnJingleInfo,
                                            base::Unretained(this)));
    } else if (!ready_) {
      ready_ = true;
      listener_->OnSessionManagerReady();
    }
  }
}

bool PepperSessionManager::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  if (!JingleMessage::IsJingleMessage(stanza))
    return false;

  JingleMessage message;
  std::string error;
  if (!message.ParseXml(stanza, &error)) {
    SendReply(stanza, JingleMessageReply::BAD_REQUEST);
    return true;
  }

  if (message.action == JingleMessage::SESSION_INITIATE) {
    // Description must be present in session-initiate messages.
    DCHECK(message.description.get());

    SendReply(stanza, JingleMessageReply::NONE);

    scoped_ptr<Authenticator> authenticator =
        authenticator_factory_->CreateAuthenticator(
            message.from, message.description->authenticator_message());

    PepperSession* session = new PepperSession(this);
    session->InitializeIncomingConnection(message, authenticator.Pass());
    sessions_[session->session_id_] = session;

    IncomingSessionResponse response = SessionManager::DECLINE;
    listener_->OnIncomingSession(session, &response);

    if (response == SessionManager::ACCEPT) {
      session->AcceptIncomingConnection(message);
    } else {
      session->Close();
      delete session;
      DCHECK(sessions_.find(message.sid) == sessions_.end());
    }

    return true;
  }

  SessionsMap::iterator it = sessions_.find(message.sid);
  if (it == sessions_.end()) {
    SendReply(stanza, JingleMessageReply::INVALID_SID);
    return true;
  }

  it->second->OnIncomingMessage(message, base::Bind(
      &PepperSessionManager::SendReply, base::Unretained(this), stanza));
  return true;
}

void PepperSessionManager::SendReply(const buzz::XmlElement* original_stanza,
                                     JingleMessageReply::ErrorType error) {
  signal_strategy_->SendStanza(
      JingleMessageReply(error).ToXml(original_stanza));
}

void PepperSessionManager::SessionDestroyed(PepperSession* session) {
  sessions_.erase(session->session_id_);
}

}  // namespace protocol
}  // namespace remoting
