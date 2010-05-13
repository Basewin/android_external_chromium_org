// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/app_launcher_handler.h"

#include "app/resource_bundle.h"
#include "base/base64.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"

namespace {

bool TreatAsApp(const Extension* extension) {
  return !extension->GetFullLaunchURL().is_empty();
}

}  // namespace

AppLauncherHandler::AppLauncherHandler(
    ExtensionsService* extension_service)
    : extensions_service_(extension_service) {
}

AppLauncherHandler::~AppLauncherHandler() {}

DOMMessageHandler* AppLauncherHandler::Attach(DOMUI* dom_ui) {
  // TODO(arv): Add initialization code to the Apps store etc.
  return DOMMessageHandler::Attach(dom_ui);
}

void AppLauncherHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getApps",
      NewCallback(this, &AppLauncherHandler::HandleGetApps));
  dom_ui_->RegisterMessageCallback("launchApp",
      NewCallback(this, &AppLauncherHandler::HandleLaunchApp));
}

void AppLauncherHandler::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
    case NotificationType::EXTENSION_UNLOADED:
      if (dom_ui_->tab_contents())
        HandleGetApps(NULL);
      break;

    default:
      NOTREACHED();
  }
}

// static
void AppLauncherHandler::CreateAppInfo(Extension* extension,
                                          DictionaryValue* value) {
  value->Clear();
  value->SetString(L"id", extension->id());
  value->SetString(L"name", extension->name());
  value->SetString(L"description", extension->description());
  value->SetString(L"launch_url", extension->GetFullLaunchURL().spec());

  FilePath relative_path =
      extension->GetIconPath(Extension::EXTENSION_ICON_LARGE).relative_path();

#if defined(OS_POSIX)
  std::string path = relative_path.value();
#elif defined(OS_WIN)
  std::string path = WideToUTF8(relative_path.value());
#endif  // OS_WIN

  GURL icon_url = extension->GetResourceURL(path);
  value->SetString(L"icon", icon_url.spec());
}

void AppLauncherHandler::HandleGetApps(const Value* value) {
  std::string gallery_title;
  std::string gallery_url;

  // TODO(aa): Decide the final values for these and remove the switches.
  gallery_title = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAppsGalleryTitle);
  gallery_url = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAppsGalleryURL);
  bool show_debug_link = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAppsDebug);

  DictionaryValue dictionary;
  dictionary.SetString(L"galleryTitle", gallery_title);
  dictionary.SetString(L"galleryURL", gallery_url);
  dictionary.SetBoolean(L"showDebugLink", show_debug_link);

  ListValue* list = new ListValue();
  const ExtensionList* extensions = extensions_service_->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
     if (TreatAsApp(*it)) {
       DictionaryValue* app_info = new DictionaryValue();
       CreateAppInfo(*it, app_info);
       list->Append(app_info);
     }
  }

  dictionary.Set(L"apps", list);
  dom_ui_->CallJavascriptFunction(L"getAppsCallback", dictionary);

  // First time we get here we set up the observer so that we can tell update
  // the apps as they change.
  if (registrar_.IsEmpty()) {
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
        NotificationService::AllSources());
    registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
        NotificationService::AllSources());
  }
}

void AppLauncherHandler::HandleLaunchApp(const Value* value) {
  if (!value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    return;
  }

  std::string extension_id;
  std::string launch_container;

  const ListValue* list = static_cast<const ListValue*>(value);
  if (!list->GetString(0, &extension_id) ||
      !list->GetString(1, &launch_container)) {
    NOTREACHED();
    return;
  }

  Profile* profile = extensions_service_->profile();
  if (launch_container.empty()) {
    Browser::OpenApplication(profile, extension_id);
    return;
  }

  // Override the default launch container.
  Extension* extension =
      extensions_service_->GetExtensionById(extension_id, false);
  DCHECK(extension);

  Extension::LaunchContainer container = Extension::LAUNCH_TAB;
  if (launch_container == "tab")
    container = Extension::LAUNCH_TAB;
  else if (launch_container == "panel")
    container = Extension::LAUNCH_PANEL;
  else if (launch_container == "window")
    container = Extension::LAUNCH_WINDOW;
  else
    NOTREACHED() << "Unexpected launch container: " << launch_container << ".";

  // To give a more "launchy" experience when using the NTP launcher, we close
  // it automatically.
  Browser* browser = BrowserList::GetLastActive();
  TabContents* old_contents = NULL;
  if (browser)
    old_contents = browser->GetSelectedTabContents();

  Browser::OpenApplication(profile, extension, container);

  if (old_contents &&
      old_contents->GetURL().GetOrigin() ==
          GURL(chrome::kChromeUINewTabURL).GetOrigin()) {
    browser->CloseTabContents(old_contents);
  }
}
