// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl_map.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/histogram_internals_request_job.h"
#include "content/browser/net/view_blob_internals_job_factory.h"
#include "content/browser/net/view_http_cache_job_factory.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/tcmalloc_internals_request_job.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "crypto/sha2.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context.h"
#include "webkit/appcache/view_appcache_internals_job.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_url_request_job_factory.h"
#include "webkit/fileapi/file_system_url_request_job_factory.h"

using appcache::AppCacheService;
using fileapi::FileSystemContext;
using webkit_blob::BlobStorageController;

namespace content {

namespace {

class BlobProtocolHandler : public webkit_blob::BlobProtocolHandler {
 public:
  BlobProtocolHandler(
      webkit_blob::BlobStorageController* blob_storage_controller,
      fileapi::FileSystemContext* file_system_context,
      base::MessageLoopProxy* loop_proxy)
      : webkit_blob::BlobProtocolHandler(blob_storage_controller,
                                         file_system_context,
                                         loop_proxy) {}

  virtual ~BlobProtocolHandler() {}

 private:
  virtual scoped_refptr<webkit_blob::BlobData>
      LookupBlobData(net::URLRequest* request) const {
    const ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request);
    if (!info)
      return NULL;
    return info->requested_blob_data();
  }

  DISALLOW_COPY_AND_ASSIGN(BlobProtocolHandler);
};

// Adds a bunch of debugging urls. We use an interceptor instead of a protocol
// handler because we want to reuse the chrome://scheme (everyone is familiar
// with it, and no need to expose the content/chrome separation through our UI).
class DeveloperProtocolHandler
    : public net::URLRequestJobFactory::Interceptor {
 public:
  DeveloperProtocolHandler(
      AppCacheService* appcache_service,
      BlobStorageController* blob_storage_controller)
      : appcache_service_(appcache_service),
        blob_storage_controller_(blob_storage_controller) {}
  virtual ~DeveloperProtocolHandler() {}

  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    // Check for chrome://view-http-cache/*, which uses its own job type.
    if (ViewHttpCacheJobFactory::IsSupportedURL(request->url()))
      return ViewHttpCacheJobFactory::CreateJobForRequest(request,
                                                          network_delegate);

    // Next check for chrome://appcache-internals/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUIAppCacheInternalsHost) {
      return appcache::ViewAppCacheInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, appcache_service_);
    }

    // Next check for chrome://blob-internals/, which uses its own job type.
    if (ViewBlobInternalsJobFactory::IsSupportedURL(request->url())) {
      return ViewBlobInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, blob_storage_controller_);
    }

#if defined(USE_TCMALLOC)
    // Next check for chrome://tcmalloc/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUITcmallocHost) {
      return new TcmallocInternalsRequestJob(request, network_delegate);
    }
#endif

    // Next check for chrome://histograms/, which uses its own job type.
    if (request->url().SchemeIs(chrome::kChromeUIScheme) &&
        request->url().host() == chrome::kChromeUIHistogramHost) {
      return new HistogramInternalsRequestJob(request, network_delegate);
    }

    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptRedirect(
        const GURL& location,
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const OVERRIDE {
    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return NULL;
  }

  virtual bool WillHandleProtocol(const std::string& protocol) const {
    return protocol == chrome::kChromeUIScheme;
  }

 private:
  AppCacheService* appcache_service_;
  BlobStorageController* blob_storage_controller_;
};

void InitializeURLRequestContext(
    net::URLRequestContextGetter* context_getter,
    AppCacheService* appcache_service,
    FileSystemContext* file_system_context,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context_getter)
    return;  // tests.

  // This code only modifies the URLRequestJobFactory on the context
  // to handle blob: URLs, filesystem: URLs, and to let AppCache intercept
  // the appropriate requests.  This is in addition to the slew of other
  // initializtion that is done in during creation of the URLRequestContext.
  // We cannot yet centralize this code because URLRequestContext needs
  // to be created before the StoragePartition context.
  //
  // TODO(ajwong): Fix the ordering so all the initialization is in one spot.
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  net::URLRequestJobFactory* job_factory =
      const_cast<net::URLRequestJobFactory*>(context->job_factory());

  // Note: if this is called twice with 2 request contexts that share one job
  // factory (as is the case with a media request context and its related
  // normal request context) then this will early exit.
  if (job_factory->IsHandledProtocol(chrome::kBlobScheme))
    return;  // Already initialized this JobFactory.

  bool set_protocol = job_factory->SetProtocolHandler(
      chrome::kBlobScheme,
      new BlobProtocolHandler(
          blob_storage_context->controller(),
          file_system_context,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  DCHECK(set_protocol);
  set_protocol = job_factory->SetProtocolHandler(
      chrome::kFileSystemScheme,
      CreateFileSystemProtocolHandler(file_system_context));
  DCHECK(set_protocol);

  job_factory->AddInterceptor(
      new DeveloperProtocolHandler(appcache_service,
                                   blob_storage_context->controller()));

  // TODO(jam): Add the ProtocolHandlerRegistryIntercepter here!
}

// These constants are used to create the directory structure under the profile
// where renderers with a non-default storage partition keep their persistent
// state. This will contain a set of directories that partially mirror the
// directory structure of BrowserContext::GetPath().
//
// The kStoragePartitionDirname contains an extensions directory which is
// further partitioned by extension id, followed by another level of directories
// for the "default" extension storage partition and one directory for each
// persistent partition used by a webview tag. Example:
//
//   Storage/ext/ABCDEF/def
//   Storage/ext/ABCDEF/hash(partition name)
//
// The code in GetStoragePartitionPath() constructs these path names.
//
// TODO(nasko): Move extension related path code out of content.
const FilePath::CharType kStoragePartitionDirname[] =
    FILE_PATH_LITERAL("Storage");
const FilePath::CharType kExtensionsDirname[] =
    FILE_PATH_LITERAL("ext");
const FilePath::CharType kDefaultPartitionDirname[] =
    FILE_PATH_LITERAL("def");

// Because partition names are user specified, they can be arbitrarily long
// which makes them unsuitable for paths names. We use a truncation of a
// SHA256 hash to perform a deterministic shortening of the string. The
// kPartitionNameHashBytes constant controls the length of the truncation.
// We use 6 bytes, which gives us 99.999% reliability against collisions over
// 1 million partition domains.
//
// Analysis:
// We assume that all partition names within one partition domain are
// controlled by the the same entity. Thus there is no chance for adverserial
// attack and all we care about is accidental collision. To get 5 9s over
// 1 million domains, we need the probability of a collision in any one domain
// to be
//
//    p < nroot(1000000, .99999) ~= 10^-11
//
// We use the following birthday attack approximation to calculate the max
// number of unique names for this probability:
//
//    n(p,H) = sqrt(2*H * ln(1/(1-p)))
//
// For a 6-byte hash, H = 2^(6*8).  n(10^-11, H) ~= 75
//
// An average partition domain is likely to have less than 10 unique
// partition names which is far lower than 75.
//
// Note, that for 4 9s of reliability, the limit is 237 partition names per
// partition domain.
const int kPartitionNameHashBytes = 6;

}  // namespace

// static
FilePath StoragePartitionImplMap::GetStoragePartitionPath(
    const std::string& partition_domain,
    const std::string& partition_name) {
  if (partition_domain.empty())
    return FilePath();

  CHECK(IsStringUTF8(partition_domain));

  FilePath path = FilePath(kStoragePartitionDirname).Append(kExtensionsDirname)
      .Append(FilePath::FromUTF8Unsafe(partition_domain));

  if (!partition_name.empty()) {
    // For analysis of why we can ignore collisions, see the comment above
    // kPartitionNameHashBytes.
    char buffer[kPartitionNameHashBytes];
    crypto::SHA256HashString(partition_name, &buffer[0],
                             sizeof(buffer));
    return path.AppendASCII(base::HexEncode(buffer, sizeof(buffer)));
  }

  return path.Append(kDefaultPartitionDirname);
}


StoragePartitionImplMap::StoragePartitionImplMap(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      resource_context_initialized_(false) {
}

StoragePartitionImplMap::~StoragePartitionImplMap() {
  STLDeleteContainerPairSecondPointers(partitions_.begin(),
                                       partitions_.end());
}

StoragePartitionImpl* StoragePartitionImplMap::Get(
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory) {
  // TODO(ajwong): ResourceContexts no longer have any storage related state.
  // We should move this into a place where it is called once per
  // BrowserContext creation rather than piggybacking off the default context
  // creation.
  if (!resource_context_initialized_) {
    resource_context_initialized_ = true;
    InitializeResourceContext(browser_context_);
  }

  // Find the previously created partition if it's available.
  StoragePartitionConfig partition_config(
      partition_domain, partition_name, in_memory);

  PartitionMap::const_iterator it = partitions_.find(partition_config);
  if (it != partitions_.end())
    return it->second;

  FilePath partition_path =
      browser_context_->GetPath().Append(
          GetStoragePartitionPath(partition_domain, partition_name));
  StoragePartitionImpl* partition =
      StoragePartitionImpl::Create(browser_context_, in_memory,
                                   partition_path);
  partitions_[partition_config] = partition;

  // These calls must happen after StoragePartitionImpl::Create().
  partition->SetURLRequestContext(
      partition_domain.empty() ?
      browser_context_->GetRequestContext() :
      browser_context_->GetRequestContextForStoragePartition(
          partition->GetPath(), in_memory));
  partition->SetMediaURLRequestContext(
      partition_domain.empty() ?
      browser_context_->GetMediaRequestContext() :
      browser_context_->GetMediaRequestContextForStoragePartition(
          partition->GetPath(), in_memory));

  PostCreateInitialization(partition);

  return partition;
}

void StoragePartitionImplMap::ForEach(
    const BrowserContext::StoragePartitionCallback& callback) {
  for (PartitionMap::const_iterator it = partitions_.begin();
       it != partitions_.end();
       ++it) {
    callback.Run(it->second);
  }
}

void StoragePartitionImplMap::PostCreateInitialization(
    StoragePartitionImpl* partition) {
  // Check first to avoid memory leak in unittests.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeAppCacheService::InitializeOnIOThread,
                   partition->GetAppCacheService(),
                   browser_context_->IsOffTheRecord() ? FilePath() :
                       partition->GetPath().Append(kAppCacheDirname),
                   browser_context_->GetResourceContext(),
                   make_scoped_refptr(partition->GetURLRequestContext()),
                   make_scoped_refptr(
                       browser_context_->GetSpecialStoragePolicy())));

    // Add content's URLRequestContext's hooks.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &InitializeURLRequestContext,
            make_scoped_refptr(partition->GetURLRequestContext()),
            make_scoped_refptr(partition->GetAppCacheService()),
            make_scoped_refptr(partition->GetFileSystemContext()),
            make_scoped_refptr(
                ChromeBlobStorageContext::GetFor(browser_context_))));

    // We do not call InitializeURLRequestContext() for media contexts because,
    // other than the HTTP cache, the media contexts share the same backing
    // objects as their associated "normal" request context.  Thus, the previous
    // call serves to initialize the media request context for this storage
    // partition as well.
  }
}

}  // namespace content
