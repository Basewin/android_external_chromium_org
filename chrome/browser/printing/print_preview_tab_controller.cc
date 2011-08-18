// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"

#include <vector>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/webplugininfo.h"

using webkit::npapi::PluginGroup;
using webkit::npapi::PluginList;
using webkit::WebPluginInfo;

namespace {

void EnableInternalPDFPluginForTab(TabContentsWrapper* preview_tab) {
  // Always enable the internal PDF plugin for the print preview page.
  string16 internal_pdf_group_name(
      ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName));
  PluginGroup* internal_pdf_group = NULL;
  std::vector<PluginGroup> plugin_groups;
  PluginList::Singleton()->GetPluginGroups(false, &plugin_groups);
  for (size_t i = 0; i < plugin_groups.size(); ++i) {
    if (plugin_groups[i].GetGroupName() == internal_pdf_group_name) {
      internal_pdf_group = &plugin_groups[i];
      break;
    }
  }
  if (internal_pdf_group) {
    std::vector<WebPluginInfo> plugins = internal_pdf_group->web_plugin_infos();
    DCHECK_EQ(plugins.size(), 1U);

    PluginService::OverriddenPlugin plugin;
    plugin.render_process_id = preview_tab->render_view_host()->process()->id();
    plugin.render_view_id = preview_tab->render_view_host()->routing_id();
    plugin.plugin = plugins[0];
    plugin.plugin.enabled = WebPluginInfo::USER_ENABLED;

    PluginService::GetInstance()->OverridePluginForTab(plugin);
  }
}

}  // namespace

namespace printing {

PrintPreviewTabController::PrintPreviewTabController()
    : waiting_for_new_preview_page_(false) {
}

PrintPreviewTabController::~PrintPreviewTabController() {}

// static
PrintPreviewTabController* PrintPreviewTabController::GetInstance() {
  if (!g_browser_process)
    return NULL;
  return g_browser_process->print_preview_tab_controller();
}

// static
void PrintPreviewTabController::PrintPreview(TabContents* tab) {
  if (tab->showing_interstitial_page())
    return;

  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return;
  tab_controller->GetOrCreatePreviewTab(tab);
}

TabContents* PrintPreviewTabController::GetOrCreatePreviewTab(
    TabContents* initiator_tab) {
  DCHECK(initiator_tab);

  // Get the print preview tab for |initiator_tab|.
  TabContents* preview_tab = GetPrintPreviewForTab(initiator_tab);
  if (preview_tab) {
    // Show current preview tab.
    static_cast<RenderViewHostDelegate*>(preview_tab)->Activate();
    return preview_tab;
  }
  return CreatePrintPreviewTab(initiator_tab);
}

TabContents* PrintPreviewTabController::GetPrintPreviewForTab(
    TabContents* tab) const {
  // |preview_tab_map_| is keyed by the preview tab, so if find() succeeds, then
  // |tab| is the preview tab.
  PrintPreviewTabMap::const_iterator it = preview_tab_map_.find(tab);
  if (it != preview_tab_map_.end())
    return tab;

  for (it = preview_tab_map_.begin(); it != preview_tab_map_.end(); ++it) {
    // If |tab| is an initiator tab.
    if (tab == it->second) {
      // Return the associated preview tab.
      return it->first;
    }
  }
  return NULL;
}

void PrintPreviewTabController::Observe(int type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      OnRendererProcessClosed(Source<RenderProcessHost>(source).ptr());
      break;
    }
    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      OnTabContentsDestroyed(Source<TabContents>(source).ptr());
      break;
    }
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      content::LoadCommittedDetails* load_details =
          Details<content::LoadCommittedDetails>(details).ptr();
      OnNavEntryCommitted(controller->tab_contents(), load_details);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintPreviewTabController::OnRendererProcessClosed(
    RenderProcessHost* rph) {
  for (PrintPreviewTabMap::iterator iter = preview_tab_map_.begin();
       iter != preview_tab_map_.end(); ++iter) {
    if (iter->second != NULL &&
        iter->second->render_view_host()->process() == rph) {
      TabContents* preview_tab = GetPrintPreviewForTab(iter->second);
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(preview_tab->web_ui());
      print_preview_ui->OnInitiatorTabCrashed();
    }
  }
}

void PrintPreviewTabController::OnTabContentsDestroyed(TabContents* tab) {
  TabContents* preview_tab = GetPrintPreviewForTab(tab);
  if (!preview_tab)
    return;

  if (tab == preview_tab) {
    // Remove the initiator tab's observers before erasing the mapping.
    TabContents* initiator_tab = GetInitiatorTab(tab);
    if (initiator_tab)
      RemoveObservers(initiator_tab);

    // Print preview tab contents are destroyed. Notify |PrintPreviewUI| to
    // abort the initiator tab preview request.
    if (IsPrintPreviewTab(tab) && tab->web_ui()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(tab->web_ui());
      print_preview_ui->OnTabDestroyed();
    }

    // Erase the map entry.
    preview_tab_map_.erase(tab);
  } else {
    // Initiator tab is closed. Disable the controls in preview tab.
    if (preview_tab->web_ui()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(preview_tab->web_ui());
      print_preview_ui->OnInitiatorTabClosed();
    }

    // |tab| is an initiator tab, update the map entry and remove observers.
    preview_tab_map_[preview_tab] = NULL;
  }

  RemoveObservers(tab);
}

void PrintPreviewTabController::OnNavEntryCommitted(
    TabContents* tab, content::LoadCommittedDetails* details) {
  TabContents* preview_tab = GetPrintPreviewForTab(tab);
  bool source_tab_is_preview_tab = (tab == preview_tab);
  if (details) {
    PageTransition::Type transition_type = details->entry->transition_type();
    NavigationType::Type nav_type = details->type;

    // Don't update/erase the map entry if the page has not changed.
    if (transition_type == PageTransition::RELOAD ||
        nav_type == NavigationType::SAME_PAGE) {
      return;
    }

    // New |preview_tab| is created. Don't update/erase map entry.
    if (waiting_for_new_preview_page_ &&
        transition_type == PageTransition::LINK &&
        nav_type == NavigationType::NEW_PAGE &&
        source_tab_is_preview_tab) {
      waiting_for_new_preview_page_ = false;
      // Set the initiator tab url and title.
      TabContents* initiator_tab = GetInitiatorTab(tab);
      if (initiator_tab && preview_tab->web_ui()) {
        PrintPreviewUI* print_preview_ui =
            static_cast<PrintPreviewUI*>(preview_tab->web_ui());
        TabContentsWrapper* wrapper =
            TabContentsWrapper::GetCurrentWrapperForContents(initiator_tab);
        print_preview_ui->SetInitiatorTabURLAndTitle(
            initiator_tab->GetURL().spec(),
            wrapper->print_view_manager()->RenderSourceName());
      }
      return;
    }

    // User navigated to a preview tab using forward/back button.
    if (source_tab_is_preview_tab &&
        transition_type == PageTransition::FORWARD_BACK &&
        nav_type == NavigationType::EXISTING_PAGE) {
      return;
    }
  }

  RemoveObservers(tab);
  if (source_tab_is_preview_tab) {
    // Remove the initiator tab's observers before erasing the mapping.
    TabContents* initiator_tab = GetInitiatorTab(tab);
    if (initiator_tab)
      RemoveObservers(initiator_tab);
    preview_tab_map_.erase(tab);
  } else {
    preview_tab_map_[preview_tab] = NULL;

    // Initiator tab is closed. Disable the controls in preview tab.
    if (preview_tab->web_ui()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(preview_tab->web_ui());
      print_preview_ui->OnInitiatorTabClosed();
    }
  }
}

// static
bool PrintPreviewTabController::IsPrintPreviewTab(TabContents* tab) {
  const GURL& url = tab->GetURL();
  return (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIPrintHost);
}

void PrintPreviewTabController::EraseInitiatorTabInfo(
    TabContents* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it == preview_tab_map_.end())
    return;

  RemoveObservers(it->second);
  preview_tab_map_[preview_tab] = NULL;
}

TabContents* PrintPreviewTabController::GetInitiatorTab(
    TabContents* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it != preview_tab_map_.end())
    return preview_tab_map_[preview_tab];
  return NULL;
}

TabContents* PrintPreviewTabController::CreatePrintPreviewTab(
    TabContents* initiator_tab) {
  // TODO: this should be converted to TabContentsWrapper.
  TabContentsWrapper* tab =
      TabContentsWrapper::GetCurrentWrapperForContents(initiator_tab);
  DCHECK(tab);
  Browser* current_browser = BrowserList::FindBrowserWithID(
      tab->restore_tab_helper()->window_id().id());
  if (!current_browser) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
      Profile* profile =
          Profile::FromBrowserContext(initiator_tab->browser_context());
      current_browser = Browser::CreateForType(Browser::TYPE_POPUP, profile);
      if (!current_browser) {
        NOTREACHED() << "Failed to create popup browser window";
        return NULL;
      }
    } else {
      return NULL;
    }
  }

  // Add a new tab next to initiator tab.
  browser::NavigateParams params(current_browser,
                                 GURL(chrome::kChromeUIPrintURL),
                                 PageTransition::LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame))
    params.disposition = NEW_POPUP;
  params.tabstrip_index = current_browser->tabstrip_model()->
      GetWrapperIndex(initiator_tab) + 1;
  browser::Navigate(&params);
  TabContentsWrapper* preview_tab = params.target_contents;
  EnableInternalPDFPluginForTab(preview_tab);
  static_cast<RenderViewHostDelegate*>(preview_tab->tab_contents())->Activate();

  // Add an entry to the map.
  preview_tab_map_[preview_tab->tab_contents()] = initiator_tab;
  waiting_for_new_preview_page_ = true;

  AddObservers(initiator_tab);
  AddObservers(preview_tab->tab_contents());

  return preview_tab->tab_contents();
}

void PrintPreviewTabController::AddObservers(TabContents* tab) {
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&tab->controller()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  RenderProcessHost* rph = tab->render_view_host()->process();
  if (!registrar_.IsRegistered(this,
                               content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                               Source<RenderProcessHost>(rph))) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   Source<RenderProcessHost>(rph));
  }
}

void PrintPreviewTabController::RemoveObservers(TabContents* tab) {
  registrar_.Remove(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(tab));
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                    Source<NavigationController>(&tab->controller()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  RenderProcessHost* rph = tab->render_view_host()->process();
  if (registrar_.IsRegistered(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              Source<RenderProcessHost>(rph))) {
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      Source<RenderProcessHost>(rph));
  }
}

}  // namespace printing
