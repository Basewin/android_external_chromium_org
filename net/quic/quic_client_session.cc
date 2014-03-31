// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_stream_factory.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {

// Histograms for tracking down the crashes from http://crbug.com/354669
// Note: these values must be kept in sync with the corresponding values in:
// tools/metrics/histograms/histograms.xml
enum Location {
  DESTRUCTOR = 0,
  ADD_OBSERVER = 1,
  TRY_CREATE_STREAM = 2,
  CREATE_OUTGOING_RELIABLE_STREAM = 3,
  NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER = 4,
  NOTIFY_FACTORY_OF_SESSION_CLOSED = 5,
  NUM_LOCATIONS = 6,
};

void RecordUnexpectedOpenStreams(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedOpenStreams", location,
                            NUM_LOCATIONS);
}

void RecordUnexpectedObservers(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedObservers", location,
                            NUM_LOCATIONS);
}

void RecordUnexpectedNotGoingAway(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedNotGoingAway", location,
                            NUM_LOCATIONS);
}

// Note: these values must be kept in sync with the corresponding values in:
// tools/metrics/histograms/histograms.xml
enum HandshakeState {
  STATE_STARTED = 0,
  STATE_ENCRYPTION_ESTABLISHED = 1,
  STATE_HANDSHAKE_CONFIRMED = 2,
  STATE_FAILED = 3,
  NUM_HANDSHAKE_STATES = 4
};

void RecordHandshakeState(HandshakeState state) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicHandshakeState", state,
                            NUM_HANDSHAKE_STATES);
}

}  // namespace

QuicClientSession::StreamRequest::StreamRequest() : stream_(NULL) {}

QuicClientSession::StreamRequest::~StreamRequest() {
  CancelRequest();
}

int QuicClientSession::StreamRequest::StartRequest(
    const base::WeakPtr<QuicClientSession>& session,
    QuicReliableClientStream** stream,
    const CompletionCallback& callback) {
  session_ = session;
  stream_ = stream;
  int rv = session_->TryCreateStream(this, stream_);
  if (rv == ERR_IO_PENDING) {
    callback_ = callback;
  }

  return rv;
}

void QuicClientSession::StreamRequest::CancelRequest() {
  if (session_)
    session_->CancelRequest(this);
  session_.reset();
  callback_.Reset();
}

void QuicClientSession::StreamRequest::OnRequestCompleteSuccess(
    QuicReliableClientStream* stream) {
  session_.reset();
  *stream_ = stream;
  ResetAndReturn(&callback_).Run(OK);
}

void QuicClientSession::StreamRequest::OnRequestCompleteFailure(int rv) {
  session_.reset();
  ResetAndReturn(&callback_).Run(rv);
}

QuicClientSession::QuicClientSession(
    QuicConnection* connection,
    scoped_ptr<DatagramClientSocket> socket,
    scoped_ptr<QuicDefaultPacketWriter> writer,
    QuicStreamFactory* stream_factory,
    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
    scoped_ptr<QuicServerInfo> server_info,
    const QuicSessionKey& server_key,
    const QuicConfig& config,
    QuicCryptoClientConfig* crypto_config,
    NetLog* net_log)
    : QuicClientSessionBase(connection, config),
      require_confirmation_(false),
      stream_factory_(stream_factory),
      socket_(socket.Pass()),
      writer_(writer.Pass()),
      read_buffer_(new IOBufferWithSize(kMaxPacketSize)),
      server_info_(server_info.Pass()),
      read_pending_(false),
      num_total_streams_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_QUIC_SESSION)),
      logger_(net_log_),
      num_packets_read_(0),
      going_away_(false),
      weak_factory_(this) {
  crypto_stream_.reset(
      crypto_client_stream_factory ?
          crypto_client_stream_factory->CreateQuicCryptoClientStream(
              server_key, this, crypto_config) :
          new QuicCryptoClientStream(server_key, this,
                                     new ProofVerifyContextChromium(net_log_),
                                     crypto_config));

  connection->set_debug_visitor(&logger_);
  // TODO(rch): pass in full host port proxy pair
  net_log_.BeginEvent(
      NetLog::TYPE_QUIC_SESSION,
      NetLog::StringCallback("host", &server_key.host()));
}

QuicClientSession::~QuicClientSession() {
  if (!streams()->empty())
    RecordUnexpectedOpenStreams(DESTRUCTOR);
  if (!observers_.empty())
    RecordUnexpectedObservers(DESTRUCTOR);
  if (!going_away_)
    RecordUnexpectedNotGoingAway(DESTRUCTOR);

  // The session must be closed before it is destroyed.
  DCHECK(streams()->empty());
  CloseAllStreams(ERR_UNEXPECTED);
  DCHECK(observers_.empty());
  CloseAllObservers(ERR_UNEXPECTED);

  connection()->set_debug_visitor(NULL);
  net_log_.EndEvent(NetLog::TYPE_QUIC_SESSION);

  while (!stream_requests_.empty()) {
    StreamRequest* request = stream_requests_.front();
    stream_requests_.pop_front();
    request->OnRequestCompleteFailure(ERR_ABORTED);
  }

  if (IsEncryptionEstablished())
    RecordHandshakeState(STATE_ENCRYPTION_ESTABLISHED);
  if (IsCryptoHandshakeConfirmed())
    RecordHandshakeState(STATE_HANDSHAKE_CONFIRMED);
  else
    RecordHandshakeState(STATE_FAILED);

  UMA_HISTOGRAM_COUNTS("Net.QuicSession.NumTotalStreams", num_total_streams_);
  UMA_HISTOGRAM_COUNTS("Net.QuicNumSentClientHellos",
                       crypto_stream_->num_sent_client_hellos());
  if (!IsCryptoHandshakeConfirmed())
    return;

  // Sending one client_hello means we had zero handshake-round-trips.
  int round_trip_handshakes = crypto_stream_->num_sent_client_hellos() - 1;

  // Don't bother with these histogram during tests, which mock out
  // num_sent_client_hellos().
  if (round_trip_handshakes < 0 || !stream_factory_)
    return;

  bool port_selected = stream_factory_->enable_port_selection();
  SSLInfo ssl_info;
  if (!GetSSLInfo(&ssl_info) || !ssl_info.cert) {
    if (port_selected) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectSelectPortForHTTP",
                                  round_trip_handshakes, 0, 3, 4);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectRandomPortForHTTP",
                                  round_trip_handshakes, 0, 3, 4);
    }
  } else {
    if (port_selected) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectSelectPortForHTTPS",
                                  round_trip_handshakes, 0, 3, 4);
    } else {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectRandomPortForHTTPS",
                                  round_trip_handshakes, 0, 3, 4);
    }
  }
}

bool QuicClientSession::OnStreamFrames(
    const std::vector<QuicStreamFrame>& frames) {
  // Record total number of stream frames.
  UMA_HISTOGRAM_COUNTS("Net.QuicNumStreamFramesInPacket", frames.size());

  // Record number of frames per stream in packet.
  typedef std::map<QuicStreamId, size_t> FrameCounter;
  FrameCounter frames_per_stream;
  for (size_t i = 0; i < frames.size(); ++i) {
    frames_per_stream[frames[i].stream_id]++;
  }
  for (FrameCounter::const_iterator it = frames_per_stream.begin();
       it != frames_per_stream.end(); ++it) {
    UMA_HISTOGRAM_COUNTS("Net.QuicNumStreamFramesPerStreamInPacket",
                         it->second);
  }

  return QuicSession::OnStreamFrames(frames);
}

void QuicClientSession::AddObserver(Observer* observer) {
  if (going_away_)
    RecordUnexpectedObservers(ADD_OBSERVER);

  DCHECK(!ContainsKey(observers_, observer));
  observers_.insert(observer);
}

void QuicClientSession::RemoveObserver(Observer* observer) {
  DCHECK(ContainsKey(observers_, observer));
  observers_.erase(observer);
}

int QuicClientSession::TryCreateStream(StreamRequest* request,
                                       QuicReliableClientStream** stream) {
  if (!crypto_stream_->encryption_established()) {
    DLOG(DFATAL) << "Encryption not established.";
    return ERR_CONNECTION_CLOSED;
  }

  if (goaway_received()) {
    DVLOG(1) << "Going away.";
    return ERR_CONNECTION_CLOSED;
  }

  if (!connection()->connected()) {
    DVLOG(1) << "Already closed.";
    return ERR_CONNECTION_CLOSED;
  }

  if (going_away_) {
    RecordUnexpectedOpenStreams(TRY_CREATE_STREAM);
    return ERR_CONNECTION_CLOSED;
  }

  if (GetNumOpenStreams() < get_max_open_streams()) {
    *stream = CreateOutgoingReliableStreamImpl();
    return OK;
  }

  stream_requests_.push_back(request);
  return ERR_IO_PENDING;
}

void QuicClientSession::CancelRequest(StreamRequest* request) {
  // Remove |request| from the queue while preserving the order of the
  // other elements.
  StreamRequestQueue::iterator it =
      std::find(stream_requests_.begin(), stream_requests_.end(), request);
  if (it != stream_requests_.end()) {
    it = stream_requests_.erase(it);
  }
}

QuicReliableClientStream* QuicClientSession::CreateOutgoingDataStream() {
  if (!crypto_stream_->encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return NULL;
  }
  if (GetNumOpenStreams() >= get_max_open_streams()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already " << GetNumOpenStreams() << " open.";
    return NULL;
  }
  if (goaway_received()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already received goaway.";
    return NULL;
  }
  if (going_away_) {
    RecordUnexpectedOpenStreams(CREATE_OUTGOING_RELIABLE_STREAM);
    return NULL;
  }
  return CreateOutgoingReliableStreamImpl();
}

QuicReliableClientStream*
QuicClientSession::CreateOutgoingReliableStreamImpl() {
  DCHECK(connection()->connected());
  QuicReliableClientStream* stream =
      new QuicReliableClientStream(GetNextStreamId(), this, net_log_);
  ActivateStream(stream);
  ++num_total_streams_;
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.NumOpenStreams", GetNumOpenStreams());
  return stream;
}

QuicCryptoClientStream* QuicClientSession::GetCryptoStream() {
  return crypto_stream_.get();
};

// TODO(rtenneti): Add unittests for GetSSLInfo which exercise the various ways
// we learn about SSL info (sync vs async vs cached).
bool QuicClientSession::GetSSLInfo(SSLInfo* ssl_info) const {
  ssl_info->Reset();
  if (!cert_verify_result_) {
    return false;
  }

  ssl_info->cert_status = cert_verify_result_->cert_status;
  ssl_info->cert = cert_verify_result_->verified_cert;

  // TODO(rtenneti): Figure out what to set for the following.
  // Temporarily hard coded cipher_suite as 0xc031 to represent
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (from
  // net/ssl/ssl_cipher_suite_names.cc) and encryption as 256.
  int cipher_suite = 0xc02f;
  int ssl_connection_status = 0;
  ssl_connection_status |=
      (cipher_suite & SSL_CONNECTION_CIPHERSUITE_MASK) <<
       SSL_CONNECTION_CIPHERSUITE_SHIFT;
  ssl_connection_status |=
      (SSL_CONNECTION_VERSION_TLS1_2 & SSL_CONNECTION_VERSION_MASK) <<
       SSL_CONNECTION_VERSION_SHIFT;

  ssl_info->public_key_hashes = cert_verify_result_->public_key_hashes;
  ssl_info->is_issued_by_known_root =
      cert_verify_result_->is_issued_by_known_root;

  ssl_info->connection_status = ssl_connection_status;
  ssl_info->client_cert_sent = false;
  ssl_info->channel_id_sent = false;
  ssl_info->security_bits = 256;
  ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
  return true;
}

int QuicClientSession::CryptoConnect(bool require_confirmation,
                                     const CompletionCallback& callback) {
  require_confirmation_ = require_confirmation;
  RecordHandshakeState(STATE_STARTED);
  if (!crypto_stream_->CryptoConnect()) {
    // TODO(wtc): change crypto_stream_.CryptoConnect() to return a
    // QuicErrorCode and map it to a net error code.
    return ERR_CONNECTION_FAILED;
  }

  bool can_notify = require_confirmation_ ?
      IsCryptoHandshakeConfirmed() : IsEncryptionEstablished();
  if (can_notify) {
    return OK;
  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

int QuicClientSession::GetNumSentClientHellos() const {
  return crypto_stream_->num_sent_client_hellos();
}

bool QuicClientSession::CanPool(const std::string& hostname) const {
  // TODO(rch): When QUIC supports channel ID or client certificates, this
  // logic will need to be revised.
  DCHECK(connection()->connected());
  SSLInfo ssl_info;
  bool unused = false;
  if (!GetSSLInfo(&ssl_info) || !ssl_info.cert) {
    // We can always pool with insecure QUIC sessions.
    return true;
  }
  // Only pool secure QUIC sessions if the cert matches the new hostname.
  return ssl_info.cert->VerifyNameMatch(hostname, &unused);
}

QuicDataStream* QuicClientSession::CreateIncomingDataStream(
    QuicStreamId id) {
  DLOG(ERROR) << "Server push not supported";
  return NULL;
}

void QuicClientSession::CloseStream(QuicStreamId stream_id) {
  QuicSession::CloseStream(stream_id);
  OnClosedStream();
}

void QuicClientSession::SendRstStream(QuicStreamId id,
                                      QuicRstStreamErrorCode error,
                                      QuicStreamOffset bytes_written) {
  QuicSession::SendRstStream(id, error, bytes_written);
  OnClosedStream();
}

void QuicClientSession::OnClosedStream() {
  if (GetNumOpenStreams() < get_max_open_streams() &&
      !stream_requests_.empty() &&
      crypto_stream_->encryption_established() &&
      !goaway_received() &&
      !going_away_ &&
      connection()->connected()) {
    StreamRequest* request = stream_requests_.front();
    stream_requests_.pop_front();
    request->OnRequestCompleteSuccess(CreateOutgoingReliableStreamImpl());
  }

  if (GetNumOpenStreams() == 0) {
    stream_factory_->OnIdleSession(this);
  }
}

void QuicClientSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  if (!callback_.is_null() &&
      (!require_confirmation_ || event == HANDSHAKE_CONFIRMED)) {
    // TODO(rtenneti): Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    base::ResetAndReturn(&callback_).Run(OK);
  }
  if (event == HANDSHAKE_CONFIRMED) {
    ObserverSet::iterator it = observers_.begin();
    while (it != observers_.end()) {
      Observer* observer = *it;
      ++it;
      observer->OnCryptoHandshakeConfirmed();
    }
  }
  QuicSession::OnCryptoHandshakeEvent(event);
}

void QuicClientSession::OnCryptoHandshakeMessageSent(
    const CryptoHandshakeMessage& message) {
  logger_.OnCryptoHandshakeMessageSent(message);
}

void QuicClientSession::OnCryptoHandshakeMessageReceived(
    const CryptoHandshakeMessage& message) {
  logger_.OnCryptoHandshakeMessageReceived(message);
}

void QuicClientSession::OnConnectionClosed(QuicErrorCode error,
                                           bool from_peer) {
  DCHECK(!connection()->connected());
  logger_.OnConnectionClosed(error, from_peer);
  if (from_peer) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.QuicSession.ConnectionCloseErrorCodeServer", error);
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Net.QuicSession.ConnectionCloseErrorCodeClient", error);
  }

  if (error == QUIC_CONNECTION_TIMED_OUT) {
    UMA_HISTOGRAM_COUNTS(
        "Net.QuicSession.ConnectionClose.NumOpenStreams.TimedOut",
        GetNumOpenStreams());
    if (!IsCryptoHandshakeConfirmed()) {
      // If there have been any streams created, they were 0-RTT speculative
      // requests that have not be serviced.
      UMA_HISTOGRAM_COUNTS(
          "Net.QuicSession.ConnectionClose.NumTotalStreams.HandshakeTimedOut",
          num_total_streams_);
    }
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.QuicVersion",
                              connection()->version());
  NotifyFactoryOfSessionGoingAway();
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(ERR_QUIC_PROTOCOL_ERROR);
  }
  socket_->Close();
  QuicSession::OnConnectionClosed(error, from_peer);
  DCHECK(streams()->empty());
  CloseAllStreams(ERR_UNEXPECTED);
  CloseAllObservers(ERR_UNEXPECTED);
  NotifyFactoryOfSessionClosedLater();
}

void QuicClientSession::OnSuccessfulVersionNegotiation(
    const QuicVersion& version) {
  logger_.OnSuccessfulVersionNegotiation(version);
  QuicSession::OnSuccessfulVersionNegotiation(version);
}

void QuicClientSession::OnProofValid(
    const QuicCryptoClientConfig::CachedState& cached) {
  DCHECK(cached.proof_valid());

  if (!server_info_ || !server_info_->IsReadyToPersist()) {
    return;
  }

  QuicServerInfo::State* state = server_info_->mutable_state();

  state->server_config = cached.server_config();
  state->source_address_token = cached.source_address_token();
  state->server_config_sig = cached.signature();
  state->certs = cached.certs();

  server_info_->Persist();
}

void QuicClientSession::OnProofVerifyDetailsAvailable(
    const ProofVerifyDetails& verify_details) {
  const CertVerifyResult* cert_verify_result_other =
      &(reinterpret_cast<const ProofVerifyDetailsChromium*>(
          &verify_details))->cert_verify_result;
  CertVerifyResult* result_copy = new CertVerifyResult;
  result_copy->CopyFrom(*cert_verify_result_other);
  cert_verify_result_.reset(result_copy);
}

void QuicClientSession::StartReading() {
  if (read_pending_) {
    return;
  }
  read_pending_ = true;
  int rv = socket_->Read(read_buffer_.get(),
                         read_buffer_->size(),
                         base::Bind(&QuicClientSession::OnReadComplete,
                                    weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    num_packets_read_ = 0;
    return;
  }

  if (++num_packets_read_ > 32) {
    num_packets_read_ = 0;
    // Data was read, process it.
    // Schedule the work through the message loop to avoid recursive
    // callbacks.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&QuicClientSession::OnReadComplete,
                   weak_factory_.GetWeakPtr(), rv));
  } else {
    OnReadComplete(rv);
  }
}

void QuicClientSession::CloseSessionOnError(int error) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.CloseSessionOnError", -error);
  CloseSessionOnErrorInner(error, QUIC_INTERNAL_ERROR);
  NotifyFactoryOfSessionClosed();
}

void QuicClientSession::CloseSessionOnErrorInner(int net_error,
                                                 QuicErrorCode quic_error) {
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(net_error);
  }
  CloseAllStreams(net_error);
  CloseAllObservers(net_error);
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CLOSE_ON_ERROR,
      NetLog::IntegerCallback("net_error", net_error));

  connection()->CloseConnection(quic_error, false);
  DCHECK(!connection()->connected());
}

void QuicClientSession::CloseAllStreams(int net_error) {
  while (!streams()->empty()) {
    ReliableQuicStream* stream = streams()->begin()->second;
    QuicStreamId id = stream->id();
    static_cast<QuicReliableClientStream*>(stream)->OnError(net_error);
    CloseStream(id);
  }
}

void QuicClientSession::CloseAllObservers(int net_error) {
  while (!observers_.empty()) {
    Observer* observer = *observers_.begin();
    observers_.erase(observer);
    observer->OnSessionClosed(net_error);
  }
}

base::Value* QuicClientSession::GetInfoAsValue(
    const std::set<HostPortPair>& aliases) const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // TODO(rch): remove "host_port_pair" when Chrome 34 is stable.
  dict->SetString("host_port_pair", aliases.begin()->ToString());
  dict->SetString("version", QuicVersionToString(connection()->version()));
  dict->SetInteger("open_streams", GetNumOpenStreams());
  dict->SetInteger("total_streams", num_total_streams_);
  dict->SetString("peer_address", peer_address().ToString());
  dict->SetString("connection_id", base::Uint64ToString(connection_id()));
  dict->SetBoolean("connected", connection()->connected());
  SSLInfo ssl_info;
  dict->SetBoolean("secure", GetSSLInfo(&ssl_info) && ssl_info.cert);

  base::ListValue* alias_list = new base::ListValue();
  for (std::set<HostPortPair>::const_iterator it = aliases.begin();
       it != aliases.end(); it++) {
    alias_list->Append(new base::StringValue(it->ToString()));
  }
  dict->Set("aliases", alias_list);

  return dict;
}

base::WeakPtr<QuicClientSession> QuicClientSession::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void QuicClientSession::OnReadComplete(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    DVLOG(1) << "Closing session on read error: " << result;
    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.ReadError", -result);
    NotifyFactoryOfSessionGoingAway();
    CloseSessionOnErrorInner(result, QUIC_PACKET_READ_ERROR);
    NotifyFactoryOfSessionClosedLater();
    return;
  }

  scoped_refptr<IOBufferWithSize> buffer(read_buffer_);
  read_buffer_ = new IOBufferWithSize(kMaxPacketSize);
  QuicEncryptedPacket packet(buffer->data(), result);
  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  // ProcessUdpPacket might result in |this| being deleted, so we
  // use a weak pointer to be safe.
  connection()->ProcessUdpPacket(local_address, peer_address, packet);
  if (!connection()->connected()) {
    NotifyFactoryOfSessionClosedLater();
    return;
  }
  StartReading();
}

void QuicClientSession::NotifyFactoryOfSessionGoingAway() {
  going_away_ = true;
  if (stream_factory_)
    stream_factory_->OnSessionGoingAway(this);
}

void QuicClientSession::NotifyFactoryOfSessionClosedLater() {
  if (!streams()->empty())
    RecordUnexpectedOpenStreams(NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER);

  if (!going_away_)
    RecordUnexpectedNotGoingAway(NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER);

  going_away_ = true;
  DCHECK_EQ(0u, GetNumOpenStreams());
  DCHECK(!connection()->connected());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QuicClientSession::NotifyFactoryOfSessionClosed,
                 weak_factory_.GetWeakPtr()));
}

void QuicClientSession::NotifyFactoryOfSessionClosed() {
  if (!streams()->empty())
    RecordUnexpectedOpenStreams(NOTIFY_FACTORY_OF_SESSION_CLOSED);

  if (!going_away_)
    RecordUnexpectedNotGoingAway(NOTIFY_FACTORY_OF_SESSION_CLOSED);

  going_away_ = true;
  DCHECK_EQ(0u, GetNumOpenStreams());
  // Will delete |this|.
  if (stream_factory_)
    stream_factory_->OnSessionClosed(this);
}

}  // namespace net
