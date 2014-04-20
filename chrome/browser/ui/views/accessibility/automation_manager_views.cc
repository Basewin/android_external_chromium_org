// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/automation_manager_views.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/automation_internal/automation_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// static
AutomationManagerViews* AutomationManagerViews::GetInstance() {
  return Singleton<AutomationManagerViews>::get();
}

void AutomationManagerViews::HandleEvent(Profile* profile,
                                         views::View* view,
                                         ui::AXEvent event_type) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomationAPI)) {
    return;
  }

  // TODO(dtseng): Events should only be delivered to extensions with the
  // desktop permission.
  views::Widget* widget = view->GetWidget();
  if (!widget)
    return;

  if (!profile && g_browser_process->profile_manager()) {
    profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  }
  if (!profile) {
    LOG(WARNING) << "Accessibility notification but no profile";
    return;
  }

  if (!current_tree_.get() ||
      current_tree_->GetRoot()->GetWidget() != widget) {
    current_tree_.reset(new views::AXTreeSourceViews(widget));
    current_tree_serializer_.reset(
        new ui::AXTreeSerializer<views::View*>(current_tree_.get()));
    // TODO(dtseng): Need to send a load complete and clear any previous desktop
    // trees.
  }

  ui::AXTreeUpdate out_update;
  current_tree_serializer_->SerializeChanges(view, &out_update);

  // Route this event to special process/routing ids recognized by the
  // Automation API as the desktop tree.

  // TODO(dtseng): Would idealy define these special desktop constants in idl.
  content::AXEventNotificationDetails detail(out_update.nodes,
                                             event_type,
                                             current_tree_->GetId(view),
                                             0, /* process_id */
                                             0 /* routing_id */);
  std::vector<content::AXEventNotificationDetails> details;
  details.push_back(detail);
  extensions::automation_util::DispatchAccessibilityEventsToAutomation(
      details, profile);
}

AutomationManagerViews::AutomationManagerViews() {}

AutomationManagerViews::  ~AutomationManagerViews() {}
