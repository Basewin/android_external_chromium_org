// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppb_url_response_info_impl.h"

using appcache::WebApplicationCacheHostImpl;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_URLLoader_API;
using ppapi::thunk::PPB_URLRequestInfo_API;
using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderOptions;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

#ifdef _MSC_VER
// Do not warn about use of std::copy with raw pointers.
#pragma warning(disable : 4996)
#endif

namespace webkit {
namespace ppapi {

PPB_URLLoader_Impl::PPB_URLLoader_Impl(PluginInstance* instance,
                                       bool main_document_loader)
    : Resource(instance),
      main_document_loader_(main_document_loader),
      pending_callback_(),
      bytes_sent_(0),
      total_bytes_to_be_sent_(-1),
      bytes_received_(0),
      total_bytes_to_be_received_(-1),
      user_buffer_(NULL),
      user_buffer_size_(0),
      done_status_(PP_OK_COMPLETIONPENDING),
      is_streaming_to_file_(false),
      is_asynchronous_load_suspended_(false),
      has_universal_access_(false),
      status_callback_(NULL) {
}

PPB_URLLoader_Impl::~PPB_URLLoader_Impl() {
}

PPB_URLLoader_API* PPB_URLLoader_Impl::AsPPB_URLLoader_API() {
  return this;
}

void PPB_URLLoader_Impl::ClearInstance() {
  Resource::ClearInstance();
  loader_.reset();
}

int32_t PPB_URLLoader_Impl::Open(PP_Resource request_id,
                                 PP_CompletionCallback callback) {
  EnterResourceNoLock<PPB_URLRequestInfo_API> enter_request(request_id, true);
  if (enter_request.failed())
    return PP_ERROR_BADARGUMENT;
  PPB_URLRequestInfo_Impl* request = static_cast<PPB_URLRequestInfo_Impl*>(
      enter_request.object());

  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  if (request->RequiresUniversalAccess() && !has_universal_access_)
    return PP_ERROR_NOACCESS;

  if (loader_.get())
    return PP_ERROR_INPROGRESS;

  WebFrame* frame = instance()->container()->element().document().frame();
  if (!frame)
    return PP_ERROR_FAILED;
  WebURLRequest web_request(request->ToWebURLRequest(frame));

  WebURLLoaderOptions options;
  if (has_universal_access_) {
    // Universal access allows cross-origin requests and sends credentials.
    options.crossOriginRequestPolicy =
        WebURLLoaderOptions::CrossOriginRequestPolicyAllow;
    options.allowCredentials = true;
  } else if (request->allow_cross_origin_requests()) {
    // Otherwise, allow cross-origin requests with access control.
    options.crossOriginRequestPolicy =
        WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl;
    options.allowCredentials = request->allow_credentials();
  }

  is_asynchronous_load_suspended_ = false;
  loader_.reset(frame->createAssociatedURLLoader(options));
  if (!loader_.get())
    return PP_ERROR_FAILED;

  loader_->loadAsynchronously(web_request, this);
  // TODO(bbudge) Remove this code when AssociatedURLLoader is changed to
  // return errors asynchronously.
  if (done_status_ == PP_ERROR_FAILED ||
      done_status_ == PP_ERROR_NOACCESS)
    return done_status_;

  request_info_ = scoped_refptr<PPB_URLRequestInfo_Impl>(request);

  // Notify completion when we receive a redirect or response headers.
  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_URLLoader_Impl::FollowRedirect(PP_CompletionCallback callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  WebURL redirect_url = GURL(response_info_->redirect_url());

  loader_->setDefersLoading(false);  // Allow the redirect to continue.
  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool PPB_URLLoader_Impl::GetUploadProgress(int64_t* bytes_sent,
                                              int64_t* total_bytes_to_be_sent) {
  if (!RecordUploadProgress()) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return PP_FALSE;
  }
  *bytes_sent = bytes_sent_;
  *total_bytes_to_be_sent = total_bytes_to_be_sent_;
  return PP_TRUE;
}

PP_Bool PPB_URLLoader_Impl::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) {
  if (!RecordDownloadProgress()) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return PP_FALSE;
  }
  *bytes_received = bytes_received_;
  *total_bytes_to_be_received = total_bytes_to_be_received_;
  return PP_TRUE;
}

PP_Resource PPB_URLLoader_Impl::GetResponseInfo() {
  if (!response_info_)
    return 0;
  return response_info_->GetReference();
}

int32_t PPB_URLLoader_Impl::ReadResponseBody(void* buffer,
                                             int32_t bytes_to_read,
                                             PP_CompletionCallback callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (!response_info_ || response_info_->body())
    return PP_ERROR_FAILED;
  if (bytes_to_read <= 0 || !buffer)
    return PP_ERROR_BADARGUMENT;

  user_buffer_ = static_cast<char*>(buffer);
  user_buffer_size_ = bytes_to_read;

  if (!buffer_.empty())
    return FillUserBuffer();

  // We may have already reached EOF.
  if (done_status_ != PP_OK_COMPLETIONPENDING) {
    user_buffer_ = NULL;
    user_buffer_size_ = 0;
    return done_status_;
  }

  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_URLLoader_Impl::FinishStreamingToFile(
    PP_CompletionCallback callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (!response_info_ || !response_info_->body())
    return PP_ERROR_FAILED;

  // We may have already reached EOF.
  if (done_status_ != PP_OK_COMPLETIONPENDING)
    return done_status_;

  is_streaming_to_file_ = true;
  if (is_asynchronous_load_suspended_) {
    loader_->setDefersLoading(false);
    is_asynchronous_load_suspended_ = false;
  }

  // Wait for didFinishLoading / didFail.
  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

void PPB_URLLoader_Impl::Close() {
  if (loader_.get()) {
    loader_->cancel();
  } else if (main_document_loader_) {
    WebFrame* frame = instance()->container()->element().document().frame();
    frame->stopLoading();
  }
  // TODO(viettrungluu): Check what happens to the callback (probably the
  // wrong thing). May need to post abort here. crbug.com/69457
}

void PPB_URLLoader_Impl::GrantUniversalAccess() {
  has_universal_access_ = true;
}

void PPB_URLLoader_Impl::SetStatusCallback(
    PP_URLLoaderTrusted_StatusCallback cb) {
  status_callback_ = cb;
}

void PPB_URLLoader_Impl::willSendRequest(
    WebURLLoader* loader,
    WebURLRequest& new_request,
    const WebURLResponse& redirect_response) {
  if (!request_info_->follow_redirects()) {
    SaveResponse(redirect_response);
    loader_->setDefersLoading(true);
    RunCallback(PP_OK);
  }
}

void PPB_URLLoader_Impl::didSendData(
    WebURLLoader* loader,
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  // TODO(darin): Bounds check input?
  bytes_sent_ = static_cast<int64_t>(bytes_sent);
  total_bytes_to_be_sent_ = static_cast<int64_t>(total_bytes_to_be_sent);
  UpdateStatus();
}

void PPB_URLLoader_Impl::didReceiveResponse(WebURLLoader* loader,
                                            const WebURLResponse& response) {
  SaveResponse(response);

  // Sets -1 if the content length is unknown.
  total_bytes_to_be_received_ = response.expectedContentLength();
  UpdateStatus();

  RunCallback(PP_OK);
}

void PPB_URLLoader_Impl::didDownloadData(WebURLLoader* loader,
                                         int data_length) {
  bytes_received_ += data_length;
  UpdateStatus();
}

void PPB_URLLoader_Impl::didReceiveData(WebURLLoader* loader,
                                        const char* data,
                                        int data_length,
                                        int encoded_data_length) {
  bytes_received_ += data_length;

  buffer_.insert(buffer_.end(), data, data + data_length);
  if (user_buffer_) {
    RunCallback(FillUserBuffer());
  } else {
    DCHECK(!pending_callback_.get() || pending_callback_->completed());
  }

  // To avoid letting the network stack download an entire stream all at once,
  // defer loading when we have enough buffer.
  // Check the buffer size after potentially moving some to the user buffer.
  DCHECK(!request_info_ ||
         (request_info_->prefetch_buffer_lower_threshold() <
          request_info_->prefetch_buffer_upper_threshold()));
  if (!is_streaming_to_file_ &&
      !is_asynchronous_load_suspended_ &&
      request_info_ &&
      (buffer_.size() >= static_cast<size_t>(
          request_info_->prefetch_buffer_upper_threshold()))) {
    DVLOG(1) << "Suspending async load - buffer size: " << buffer_.size();
    loader->setDefersLoading(true);
    is_asynchronous_load_suspended_ = true;
  }
}

void PPB_URLLoader_Impl::didFinishLoading(WebURLLoader* loader,
                                          double finish_time) {
  done_status_ = PP_OK;
  RunCallback(done_status_);
}

void PPB_URLLoader_Impl::didFail(WebURLLoader* loader,
                                 const WebURLError& error) {
  if (error.domain.equals(WebString::fromUTF8(net::kErrorDomain))) {
    // TODO(bbudge): Extend pp_errors.h to cover interesting network errors
    // from the net error domain.
    switch (error.reason) {
      case net::ERR_ACCESS_DENIED:
      case net::ERR_NETWORK_ACCESS_DENIED:
        done_status_ = PP_ERROR_NOACCESS;
        break;
      default:
        done_status_ = PP_ERROR_FAILED;
        break;
    }
  } else {
    // It's a WebKit error.
    done_status_ = PP_ERROR_NOACCESS;
  }

  RunCallback(done_status_);
}

int32_t PPB_URLLoader_Impl::ValidateCallback(PP_CompletionCallback callback) {
  // We only support non-blocking calls.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  if (pending_callback_.get() && !pending_callback_->completed())
    return PP_ERROR_INPROGRESS;

  return PP_OK;
}

void PPB_URLLoader_Impl::RegisterCallback(PP_CompletionCallback callback) {
  DCHECK(callback.func);
  DCHECK(!pending_callback_.get() || pending_callback_->completed());

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  pending_callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id, callback);
}

void PPB_URLLoader_Impl::RunCallback(int32_t result) {
  // This may be null only when this is a main document loader.
  if (!pending_callback_.get()) {
    // TODO(viettrungluu): put this CHECK back when the callback race condition
    // is fixed: http://code.google.com/p/chromium/issues/detail?id=70347.
    //CHECK(main_document_loader_);
    return;
  }

  scoped_refptr<TrackedCompletionCallback> callback;
  callback.swap(pending_callback_);
  callback->Run(result);  // Will complete abortively if necessary.
}

size_t PPB_URLLoader_Impl::FillUserBuffer() {
  DCHECK(user_buffer_);
  DCHECK(user_buffer_size_);

  size_t bytes_to_copy = std::min(buffer_.size(), user_buffer_size_);
  std::copy(buffer_.begin(), buffer_.begin() + bytes_to_copy, user_buffer_);
  buffer_.erase(buffer_.begin(), buffer_.begin() + bytes_to_copy);

  // If the buffer is getting too empty, resume asynchronous loading.
  DCHECK(!is_asynchronous_load_suspended_ || request_info_);
  if (is_asynchronous_load_suspended_ &&
      buffer_.size() <= static_cast<size_t>(
          request_info_->prefetch_buffer_lower_threshold())) {
    DVLOG(1) << "Resuming async load - buffer size: " << buffer_.size();
    loader_->setDefersLoading(false);
    is_asynchronous_load_suspended_ = false;
  }

  // Reset for next time.
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  return bytes_to_copy;
}

void PPB_URLLoader_Impl::SaveResponse(const WebURLResponse& response) {
  scoped_refptr<PPB_URLResponseInfo_Impl> response_info(
      new PPB_URLResponseInfo_Impl(instance()));
  if (response_info->Initialize(response))
    response_info_ = response_info;
}

void PPB_URLLoader_Impl::UpdateStatus() {
  if (status_callback_ &&
      (RecordDownloadProgress() || RecordUploadProgress())) {
    PP_Resource pp_resource = GetReferenceNoAddRef();
    if (pp_resource) {
      // The PP_Resource on the plugin will be NULL if the plugin has no
      // reference to this object. That's fine, because then we don't need to
      // call UpdateStatus.
      //
      // Here we go through some effort to only send the exact information that
      // the requestor wanted in the request flags. It would be just as
      // efficient to send all of it, but we don't want people to rely on
      // getting download progress when they happen to set the upload progress
      // flag.
      status_callback_(
          instance()->pp_instance(), pp_resource,
          RecordUploadProgress() ? bytes_sent_ : -1,
          RecordUploadProgress() ?  total_bytes_to_be_sent_ : -1,
          RecordDownloadProgress() ? bytes_received_ : -1,
          RecordDownloadProgress() ? total_bytes_to_be_received_ : -1);
    }
  }
}

bool PPB_URLLoader_Impl::RecordDownloadProgress() const {
  return request_info_ && request_info_->record_download_progress();
}

bool PPB_URLLoader_Impl::RecordUploadProgress() const {
  return request_info_ && request_info_->record_upload_progress();
}

}  // namespace ppapi
}  // namespace webkit
