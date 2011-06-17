// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service.h"

#include "base/command_line.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/time.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/safe_browsing/client_model.pb.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/common/notification_service.h"
#include "content/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace safe_browsing {

const size_t ClientSideDetectionService::kMaxModelSizeBytes = 90 * 1024;
const int ClientSideDetectionService::kMaxReportsPerInterval = 3;
// TODO(noelutz): once we know this mechanism works as intended we should fetch
// the model much more frequently.  E.g., every 5 minutes or so.
const int ClientSideDetectionService::kClientModelFetchIntervalMs = 3600 * 1000;
const int ClientSideDetectionService::kInitialClientModelFetchDelayMs = 10000;

const base::TimeDelta ClientSideDetectionService::kReportsInterval =
    base::TimeDelta::FromDays(1);
const base::TimeDelta ClientSideDetectionService::kNegativeCacheInterval =
    base::TimeDelta::FromDays(1);
const base::TimeDelta ClientSideDetectionService::kPositiveCacheInterval =
    base::TimeDelta::FromMinutes(30);

const char ClientSideDetectionService::kClientReportPhishingUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/phishing";
// Note: when updatng the model version, don't forget to change the filename
// in chrome/common/chrome_constants.cc as well, or else existing users won't
// download the new model.
const char ClientSideDetectionService::kClientModelUrl[] =
    "https://ssl.gstatic.com/safebrowsing/csd/client_model_v2.pb";

struct ClientSideDetectionService::ClientReportInfo {
  scoped_ptr<ClientReportPhishingRequestCallback> callback;
  GURL phishing_url;
};

ClientSideDetectionService::CacheState::CacheState(bool phish, base::Time time)
    : is_phishing(phish),
      timestamp(time) {}

ClientSideDetectionService::ClientSideDetectionService(
    net::URLRequestContextGetter* request_context_getter)
    : model_version_(-1),
      tmp_model_version_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      request_context_getter_(request_context_getter) {
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
}

ClientSideDetectionService::~ClientSideDetectionService() {
  method_factory_.RevokeAll();
  STLDeleteContainerPairPointers(client_phishing_reports_.begin(),
                                 client_phishing_reports_.end());
  client_phishing_reports_.clear();
}

/* static */
ClientSideDetectionService* ClientSideDetectionService::Create(
    const FilePath& model_dir,
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ClientSideDetectionService> service(
      new ClientSideDetectionService(request_context_getter));
  if (!service->InitializePrivateNetworks()) {
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.InitPrivateNetworksFailed", 1);
    return NULL;
  }
  // We fetch the model at every browser restart.  In a lot of cases the model
  // will be in the cache so it won't actually be fetched from the network.
  // We delay the first model fetch to avoid slowing down browser startup.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      service->method_factory_.NewRunnableMethod(
          &ClientSideDetectionService::StartFetchModel),
      kInitialClientModelFetchDelayMs);

  // Delete the previous-version model files.
  // TODO(bryner): Remove this for M15 (including the model_dir argument to
  // Create()).
  base::FileUtilProxy::Delete(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      model_dir.AppendASCII("Safe Browsing Phishing Model"),
      false /* not recursive */,
      NULL /* not interested in result */);
  base::FileUtilProxy::Delete(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      model_dir.AppendASCII("Safe Browsing Phishing Model v1"),
      false /* not recursive */,
      NULL /* not interested in result */);
  return service.release();
}

void ClientSideDetectionService::SendClientReportPhishingRequest(
    ClientPhishingRequest* verdict,
    ClientReportPhishingRequestCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &ClientSideDetectionService::StartClientReportPhishingRequest,
          verdict, callback));
}

bool ClientSideDetectionService::IsPrivateIPAddress(
    const std::string& ip_address) const {
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_address, &ip_number)) {
    DLOG(WARNING) << "Unable to parse IP address: " << ip_address;
    // Err on the side of safety and assume this might be private.
    return true;
  }

  for (std::vector<AddressRange>::const_iterator it =
           private_networks_.begin();
       it != private_networks_.end(); ++it) {
    if (net::IPNumberMatchesPrefix(ip_number, it->first, it->second)) {
      return true;
    }
  }
  return false;
}

void ClientSideDetectionService::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  if (source == model_fetcher_.get()) {
    HandleModelResponse(source, url, status, response_code, cookies, data);
  } else if (client_phishing_reports_.find(source) !=
             client_phishing_reports_.end()) {
    HandlePhishingVerdict(source, url, status, response_code, cookies, data);
  } else {
    NOTREACHED();
  }
}

void ClientSideDetectionService::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == NotificationType::RENDERER_PROCESS_CREATED);
  if (model_version_ < 0) {
    // Model might not be ready or maybe there was an error.
    return;
  }
  SendModelToProcess(Source<RenderProcessHost>(source).ptr());
}

void ClientSideDetectionService::SendModelToProcess(
    RenderProcessHost* process) {
  VLOG(2) << "Sending phishing model to renderer";
  process->Send(new SafeBrowsingMsg_SetPhishingModel(model_str_));
}

void ClientSideDetectionService::SendModelToRenderers() {
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    SendModelToProcess(i.GetCurrentValue());
  }
}

void ClientSideDetectionService::StartFetchModel() {
  // Start fetching the model either from the cache or possibly from the
  // network if the model isn't in the cache.
  model_fetcher_.reset(URLFetcher::Create(0 /* ID is not used */,
                                          GURL(kClientModelUrl),
                                          URLFetcher::GET,
                                          this));
  model_fetcher_->set_request_context(request_context_getter_.get());
  model_fetcher_->Start();
}

void ClientSideDetectionService::EndFetchModel(ClientModelStatus status) {
  UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.ClientModelStatus",
                            status,
                            MODEL_STATUS_MAX);
  // If there is already a valid model but we're unable to reload one
  // we leave the old model.
  if (status == MODEL_SUCCESS) {
    // Replace the model string and version;
    model_str_.swap(tmp_model_str_);
    model_version_ = tmp_model_version_;
    SendModelToRenderers();
  }
  tmp_model_str_.clear();
  tmp_model_version_ = -1;

  int delay_ms = kClientModelFetchIntervalMs;
  // If the most recently fetched model had a valid max-age and the model was
  // valid we're scheduling the next model update for after the max-age expired.
  if (tmp_model_max_age_.get() &&
      (status == MODEL_SUCCESS || status == MODEL_NOT_CHANGED)) {
    // We're adding 60s of additional delay to make sure we're past
    // the model's age.
    *tmp_model_max_age_ += base::TimeDelta::FromMinutes(1);
    delay_ms = tmp_model_max_age_->InMilliseconds();
  }
  tmp_model_max_age_.reset();

  // Schedule the next model reload.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &ClientSideDetectionService::StartFetchModel),
      delay_ms);
}

void ClientSideDetectionService::StartClientReportPhishingRequest(
    ClientPhishingRequest* verdict,
    ClientReportPhishingRequestCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ClientPhishingRequest> request(verdict);
  scoped_ptr<ClientReportPhishingRequestCallback> cb(callback);

  std::string request_data;
  if (!request->SerializeToString(&request_data)) {
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.RequestNotSerialized", 1);
    VLOG(1) << "Unable to serialize the CSD request. Proto file changed?";
    cb->Run(GURL(request->url()), false);
    return;
  }

  URLFetcher* fetcher = URLFetcher::Create(0 /* ID is not used */,
                                           GURL(kClientReportPhishingUrl),
                                           URLFetcher::POST,
                                           this);

  // Remember which callback and URL correspond to the current fetcher object.
  ClientReportInfo* info = new ClientReportInfo;
  info->callback.swap(cb);  // takes ownership of the callback.
  info->phishing_url = GURL(request->url());
  client_phishing_reports_[fetcher] = info;

  fetcher->set_load_flags(net::LOAD_DISABLE_CACHE);
  fetcher->set_request_context(request_context_getter_.get());
  fetcher->set_upload_data("application/octet-stream", request_data);
  fetcher->Start();

  // Record that we made a request
  phishing_report_times_.push(base::Time::Now());
}

void ClientSideDetectionService::HandleModelResponse(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  base::TimeDelta max_age;
  if (status.is_success() && RC_REQUEST_OK == response_code &&
      source->response_headers() &&
      source->response_headers()->GetMaxAgeValue(&max_age)) {
    tmp_model_max_age_.reset(new base::TimeDelta(max_age));
  }
  ClientSideModel model;
  ClientModelStatus model_status;
  if (!status.is_success() || RC_REQUEST_OK != response_code) {
    model_status = MODEL_FETCH_FAILED;
  } else if (data.empty()) {
    model_status = MODEL_EMPTY;
  } else if (data.size() > kMaxModelSizeBytes) {
    model_status = MODEL_TOO_LARGE;
  } else if (!model.ParseFromString(data)) {
    model_status = MODEL_PARSE_ERROR;
  } else if (!model.IsInitialized() || !model.has_version()) {
    model_status = MODEL_MISSING_FIELDS;
  } else if (model.version() < 0 ||
             (model_version_ > 0 && model.version() < model_version_)) {
    model_status = MODEL_INVALID_VERSION_NUMBER;
  } else if (model.version() == model_version_) {
    model_status = MODEL_NOT_CHANGED;
  } else {
    tmp_model_version_ = model.version();
    tmp_model_str_.assign(data);
    model_status = MODEL_SUCCESS;
  }
  EndFetchModel(model_status);
}

void ClientSideDetectionService::HandlePhishingVerdict(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  ClientPhishingResponse response;
  scoped_ptr<ClientReportInfo> info(client_phishing_reports_[source]);
  if (status.is_success() && RC_REQUEST_OK == response_code &&
      response.ParseFromString(data)) {
    // Cache response, possibly flushing an old one.
    cache_[info->phishing_url] =
        make_linked_ptr(new CacheState(response.phishy(), base::Time::Now()));
    info->callback->Run(info->phishing_url, response.phishy());
  } else {
    DLOG(ERROR) << "Unable to get the server verdict for URL: "
                << info->phishing_url << " status: " << status.status() << " "
                << "response_code:" << response_code;
    info->callback->Run(info->phishing_url, false);
  }
  client_phishing_reports_.erase(source);
  delete source;
}

bool ClientSideDetectionService::IsInCache(const GURL& url) {
  UpdateCache();

  return cache_.find(url) != cache_.end();
}

bool ClientSideDetectionService::GetValidCachedResult(const GURL& url,
                                                      bool* is_phishing) {
  UpdateCache();

  PhishingCache::iterator it = cache_.find(url);
  if (it == cache_.end()) {
    return false;
  }

  // We still need to check if the result is valid.
  const CacheState& cache_state = *it->second;
  if (cache_state.is_phishing ?
      cache_state.timestamp > base::Time::Now() - kPositiveCacheInterval :
      cache_state.timestamp > base::Time::Now() - kNegativeCacheInterval) {
    *is_phishing = cache_state.is_phishing;
    return true;
  }
  return false;
}

void ClientSideDetectionService::UpdateCache() {
  // Since we limit the number of requests but allow pass-through for cache
  // refreshes, we don't want to remove elements from the cache if they
  // could be used for this purpose even if we will not use the entry to
  // satisfy the request from the cache.
  base::TimeDelta positive_cache_interval =
      std::max(kPositiveCacheInterval, kReportsInterval);
  base::TimeDelta negative_cache_interval =
      std::max(kNegativeCacheInterval, kReportsInterval);

  // Remove elements from the cache that will no longer be used.
  for (PhishingCache::iterator it = cache_.begin(); it != cache_.end();) {
    const CacheState& cache_state = *it->second;
    if (cache_state.is_phishing ?
        cache_state.timestamp > base::Time::Now() - positive_cache_interval :
        cache_state.timestamp > base::Time::Now() - negative_cache_interval) {
      ++it;
    } else {
      cache_.erase(it++);
    }
  }
}

bool ClientSideDetectionService::OverReportLimit() {
  return GetNumReports() > kMaxReportsPerInterval;
}

int ClientSideDetectionService::GetNumReports() {
  base::Time cutoff = base::Time::Now() - kReportsInterval;

  // Erase items older than cutoff because we will never care about them again.
  while (!phishing_report_times_.empty() &&
         phishing_report_times_.front() < cutoff) {
    phishing_report_times_.pop();
  }

  // Return the number of elements that are above the cutoff.
  return phishing_report_times_.size();
}

bool ClientSideDetectionService::InitializePrivateNetworks() {
  static const char* const kPrivateNetworks[] = {
    "10.0.0.0/8",
    "127.0.0.0/8",
    "172.16.0.0/12",
    "192.168.0.0/16",
    // IPv6 address ranges
    "fc00::/7",
    "fec0::/10",
    "::1/128",
  };

  for (size_t i = 0; i < arraysize(kPrivateNetworks); ++i) {
    net::IPAddressNumber ip_number;
    size_t prefix_length;
    if (net::ParseCIDRBlock(kPrivateNetworks[i], &ip_number, &prefix_length)) {
      private_networks_.push_back(std::make_pair(ip_number, prefix_length));
    } else {
      DLOG(FATAL) << "Unable to parse IP address range: "
                  << kPrivateNetworks[i];
      return false;
    }
  }
  return true;
}
}  // namespace safe_browsing
