// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shared_module_service.h"

#include <set>
#include <vector>

#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

typedef std::vector<SharedModuleInfo::ImportInfo> ImportInfoVector;
typedef std::list<SharedModuleInfo::ImportInfo> ImportInfoList;

}  // namespace

SharedModuleService::SharedModuleService(content::BrowserContext* context)
    : extension_registry_observer_(this), browser_context_(context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

SharedModuleService::~SharedModuleService() {
}

SharedModuleService::ImportStatus SharedModuleService::CheckImports(
    const Extension* extension,
    ImportInfoList* missing_modules,
    ImportInfoList* outdated_modules) {
  DCHECK(extension);
  DCHECK(missing_modules && missing_modules->empty());
  DCHECK(outdated_modules && outdated_modules->empty());

  ImportStatus status = IMPORT_STATUS_OK;

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const ImportInfoVector& imports = SharedModuleInfo::GetImports(extension);
  for (ImportInfoVector::const_iterator iter = imports.begin();
       iter != imports.end();
       ++iter) {
    base::Version version_required(iter->minimum_version);
    const Extension* imported_module =
        registry->GetExtensionById(iter->extension_id,
                                   ExtensionRegistry::EVERYTHING);
    if (!imported_module) {
      if (extension->from_webstore()) {
        status = IMPORT_STATUS_UNSATISFIED;
        missing_modules->push_back(*iter);
      } else {
        return IMPORT_STATUS_UNRECOVERABLE;
      }
    } else if (!SharedModuleInfo::IsSharedModule(imported_module)) {
      return IMPORT_STATUS_UNRECOVERABLE;
    } else if (version_required.IsValid() &&
               imported_module->version()->CompareTo(version_required) < 0) {
      if (imported_module->from_webstore()) {
        outdated_modules->push_back(*iter);
        status = IMPORT_STATUS_UNSATISFIED;
      } else {
        return IMPORT_STATUS_UNRECOVERABLE;
      }
    }
  }

  return status;
}

SharedModuleService::ImportStatus SharedModuleService::SatisfyImports(
    const Extension* extension) {
  ImportInfoList missing_modules;
  ImportInfoList outdated_modules;
  ImportStatus status =
      CheckImports(extension, &missing_modules, &outdated_modules);

  ExtensionService* service =
      ExtensionSystem::Get(browser_context_)->extension_service();

  PendingExtensionManager* pending_extension_manager =
      service->pending_extension_manager();
  DCHECK(pending_extension_manager);

  if (status == IMPORT_STATUS_UNSATISFIED) {
    for (ImportInfoList::const_iterator iter = missing_modules.begin();
         iter != missing_modules.end();
         ++iter) {
      pending_extension_manager->AddFromExtensionImport(
          iter->extension_id,
          extension_urls::GetWebstoreUpdateUrl(),
          SharedModuleInfo::IsSharedModule);
    }
    service->CheckForUpdatesSoon();
  }
  return status;
}

scoped_ptr<ExtensionSet> SharedModuleService::GetDependentExtensions(
    const Extension* extension) {
  scoped_ptr<ExtensionSet> dependents(new ExtensionSet());

  if (SharedModuleInfo::IsSharedModule(extension)) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
    ExtensionService* service =
        ExtensionSystem::Get(browser_context_)->extension_service();

    ExtensionSet set_to_check;
    set_to_check.InsertAll(registry->enabled_extensions());
    set_to_check.InsertAll(registry->disabled_extensions());
    set_to_check.InsertAll(*service->delayed_installs());

    for (ExtensionSet::const_iterator iter = set_to_check.begin();
         iter != set_to_check.end();
         ++iter) {
      if (SharedModuleInfo::ImportsExtensionById(iter->get(),
                                                 extension->id())) {
        dependents->Insert(*iter);
      }
    }
  }
  return dependents.PassAs<ExtensionSet>();
}

void SharedModuleService::PruneSharedModules() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  ExtensionService* service =
      ExtensionSystem::Get(browser_context_)->extension_service();

  ExtensionSet set_to_check;
  set_to_check.InsertAll(registry->enabled_extensions());
  set_to_check.InsertAll(registry->disabled_extensions());
  set_to_check.InsertAll(*service->delayed_installs());

  std::vector<std::string> shared_modules;
  std::set<std::string> used_shared_modules;

  for (ExtensionSet::const_iterator iter = set_to_check.begin();
       iter != set_to_check.end();
       ++iter) {
    if (SharedModuleInfo::IsSharedModule(iter->get()))
      shared_modules.push_back(iter->get()->id());

    const ImportInfoVector& imports = SharedModuleInfo::GetImports(iter->get());
    for (ImportInfoVector::const_iterator imports_iter = imports.begin();
         imports_iter != imports.end();
         ++imports_iter) {
      used_shared_modules.insert(imports_iter->extension_id);
    }
  }

  std::vector<std::string>::const_iterator shared_modules_iter;
  for (shared_modules_iter = shared_modules.begin();
       shared_modules_iter != shared_modules.end();
       shared_modules_iter++) {
    if (used_shared_modules.count(*shared_modules_iter))
      continue;
    service->UninstallExtension(
        *shared_modules_iter,
        extensions::UNINSTALL_REASON_ORPHANED_SHARED_MODULE,
        base::Bind(&base::DoNothing),
        NULL);  // Ignore error.
  }
}

void SharedModuleService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  if (is_update)
    PruneSharedModules();
}

void SharedModuleService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  PruneSharedModules();
}

}  // namespace extensions
