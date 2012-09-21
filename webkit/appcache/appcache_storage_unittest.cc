// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/appcache/mock_appcache_service.h"
#include "webkit/quota/mock_quota_manager.h"

namespace appcache {

namespace {
const quota::StorageType kTemp = quota::kStorageTypeTemporary;
}

class AppCacheStorageTest : public testing::Test {
 public:
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
  };
};

TEST_F(AppCacheStorageTest, AddRemoveCache) {
  MockAppCacheService service;
  scoped_refptr<AppCache> cache(new AppCache(&service, 111));

  EXPECT_EQ(cache.get(),
            service.storage()->working_set()->GetCache(111));

  service.storage()->working_set()->RemoveCache(cache);

  EXPECT_TRUE(!service.storage()->working_set()->GetCache(111));

  // Removing non-existing cache from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveCache(cache);
}

TEST_F(AppCacheStorageTest, AddRemoveGroup) {
  MockAppCacheService service;
  scoped_refptr<AppCacheGroup> group(new AppCacheGroup(&service, GURL(), 111));

  EXPECT_EQ(group.get(), service.storage()->working_set()->GetGroup(GURL()));

  service.storage()->working_set()->RemoveGroup(group);

  EXPECT_TRUE(!service.storage()->working_set()->GetGroup(GURL()));

  // Removing non-existing group from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveGroup(group);
}

TEST_F(AppCacheStorageTest, AddRemoveResponseInfo) {
  MockAppCacheService service;
  scoped_refptr<AppCacheResponseInfo> info(
      new AppCacheResponseInfo(&service, GURL(),
                               111, new net::HttpResponseInfo,
                               kUnkownResponseDataSize));

  EXPECT_EQ(info.get(),
            service.storage()->working_set()->GetResponseInfo(111));

  service.storage()->working_set()->RemoveResponseInfo(info);

  EXPECT_TRUE(!service.storage()->working_set()->GetResponseInfo(111));

  // Removing non-existing info from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveResponseInfo(info);
}

TEST_F(AppCacheStorageTest, DelegateReferences) {
  typedef scoped_refptr<AppCacheStorage::DelegateReference>
      ScopedDelegateReference;
  MockAppCacheService service;
  MockStorageDelegate delegate;
  ScopedDelegateReference delegate_reference1;
  ScopedDelegateReference delegate_reference2;

  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference1 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  EXPECT_TRUE(delegate_reference1.get());
  EXPECT_TRUE(delegate_reference1->HasOneRef());
  EXPECT_TRUE(service.storage()->GetDelegateReference(&delegate));
  EXPECT_EQ(&delegate,
            service.storage()->GetDelegateReference(&delegate)->delegate);
  EXPECT_EQ(service.storage()->GetDelegateReference(&delegate),
            service.storage()->GetOrCreateDelegateReference(&delegate));
  delegate_reference1 = NULL;
  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference1 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  service.storage()->CancelDelegateCallbacks(&delegate);
  EXPECT_TRUE(delegate_reference1.get());
  EXPECT_TRUE(delegate_reference1->HasOneRef());
  EXPECT_FALSE(delegate_reference1->delegate);
  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference2 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  EXPECT_TRUE(delegate_reference2.get());
  EXPECT_TRUE(delegate_reference2->HasOneRef());
  EXPECT_EQ(&delegate, delegate_reference2->delegate);
  EXPECT_NE(delegate_reference1.get(), delegate_reference2.get());
}

TEST_F(AppCacheStorageTest, UsageMap) {
  const GURL kOrigin("http://origin/");
  const GURL kOrigin2("http://origin2/");

  MockAppCacheService service;
  scoped_refptr<quota::MockQuotaManagerProxy> mock_proxy(
      new quota::MockQuotaManagerProxy(NULL, NULL));
  service.set_quota_manager_proxy(mock_proxy);

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 0);
  EXPECT_EQ(0, mock_proxy->notify_storage_modified_count());

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 10);
  EXPECT_EQ(1, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(10, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 100);
  EXPECT_EQ(2, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(90, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->UpdateUsageMapAndNotify(kOrigin, 0);
  EXPECT_EQ(3, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(-100, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->NotifyStorageAccessed(kOrigin2);
  EXPECT_EQ(0, mock_proxy->notify_storage_accessed_count());

  service.storage()->usage_map_[kOrigin2] = 1;
  service.storage()->NotifyStorageAccessed(kOrigin2);
  EXPECT_EQ(1, mock_proxy->notify_storage_accessed_count());
  EXPECT_EQ(kOrigin2, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());

  service.storage()->usage_map_.clear();
  service.storage()->usage_map_[kOrigin] = 5000;
  service.storage()->ClearUsageMapAndNotify();
  EXPECT_EQ(4, mock_proxy->notify_storage_modified_count());
  EXPECT_EQ(-5000, mock_proxy->last_notified_delta());
  EXPECT_EQ(kOrigin, mock_proxy->last_notified_origin());
  EXPECT_EQ(kTemp, mock_proxy->last_notified_type());
  EXPECT_TRUE(service.storage()->usage_map_.empty());
}

}  // namespace appcache
