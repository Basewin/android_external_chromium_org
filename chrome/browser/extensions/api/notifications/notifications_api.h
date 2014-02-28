// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/notifications.h"
#include "extensions/browser/extension_function.h"
#include "ui/message_center/notification_types.h"

class Notification;

namespace extensions {

class NotificationsApiFunction : public ChromeAsyncExtensionFunction {
 public:
  // Whether the current extension and channel allow the API. Public for
  // testing.
  bool IsNotificationsApiAvailable();

 protected:
  NotificationsApiFunction();
  virtual ~NotificationsApiFunction();

  bool CreateNotification(const std::string& id,
                          api::notifications::NotificationOptions* options);
  bool UpdateNotification(const std::string& id,
                          api::notifications::NotificationOptions* options,
                          Notification* notification);

  bool IsNotificationsApiEnabled() const;

  bool AreExtensionNotificationsAllowed() const;

  // Returns true if the API function is still allowed to run even when the
  // notifications for a notifier have been disabled.
  virtual bool CanRunWhileDisabled() const;

  // Called inside of RunImpl.
  virtual bool RunNotificationsApi() = 0;

  // UITHreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  message_center::NotificationType MapApiTemplateTypeToType(
      api::notifications::TemplateType type);
};

class NotificationsCreateFunction : public NotificationsApiFunction {
 public:
  NotificationsCreateFunction();

  // NotificationsApiFunction:
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsCreateFunction();

 private:
  scoped_ptr<api::notifications::Create::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.create", NOTIFICATIONS_CREATE)
};

class NotificationsUpdateFunction : public NotificationsApiFunction {
 public:
  NotificationsUpdateFunction();

  // NotificationsApiFunction:
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsUpdateFunction();

 private:
  scoped_ptr<api::notifications::Update::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.update", NOTIFICATIONS_UPDATE)
};

class NotificationsClearFunction : public NotificationsApiFunction {
 public:
  NotificationsClearFunction();

  // NotificationsApiFunction:
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsClearFunction();

 private:
  scoped_ptr<api::notifications::Clear::Params> params_;

  DECLARE_EXTENSION_FUNCTION("notifications.clear", NOTIFICATIONS_CLEAR)
};

class NotificationsGetAllFunction : public NotificationsApiFunction {
 public:
  NotificationsGetAllFunction();

  // NotificationsApiFunction:
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsGetAllFunction();

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.getAll", NOTIFICATIONS_GET_ALL)
};

class NotificationsGetPermissionLevelFunction
    : public NotificationsApiFunction {
 public:
  NotificationsGetPermissionLevelFunction();

  // NotificationsApiFunction:
  virtual bool CanRunWhileDisabled() const OVERRIDE;
  virtual bool RunNotificationsApi() OVERRIDE;

 protected:
  virtual ~NotificationsGetPermissionLevelFunction();

 private:
  DECLARE_EXTENSION_FUNCTION("notifications.getPermissionLevel",
                             NOTIFICATIONS_GET_ALL)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATIONS_API_H_
