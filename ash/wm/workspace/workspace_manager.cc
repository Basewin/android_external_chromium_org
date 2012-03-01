// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/managed_workspace.h"
#include "ash/wm/workspace/maximized_workspace.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

namespace {

// The horizontal margein between workspaces in pixels.
const int kWorkspaceHorizontalMargin = 50;

// Minimum/maximum scale for overview mode.
const float kMaxOverviewScale = 0.9f;
const float kMinOverviewScale = 0.3f;

// Returns a list of all the windows with layers in |result|.
void BuildWindowList(const std::vector<aura::Window*>& windows,
                     std::vector<aura::Window*>* result) {
  for (size_t i = 0; i < windows.size(); ++i) {
    if (windows[i]->layer())
      result->push_back(windows[i]);
    BuildWindowList(windows[i]->transient_children(), result);
  }
}

gfx::Rect AlignRectToGrid(const gfx::Rect& rect, int grid_size) {
  if (grid_size <= 1)
    return rect;
  return gfx::Rect(ash::WindowResizer::AlignToGrid(rect.x(), grid_size),
                   ash::WindowResizer::AlignToGrid(rect.y(), grid_size),
                   ash::WindowResizer::AlignToGrid(rect.width(), grid_size),
                   ash::WindowResizer::AlignToGrid(rect.height(), grid_size));
}

}

namespace ash {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// WindowManager, public:

WorkspaceManager::WorkspaceManager(aura::Window* contents_view)
    : contents_view_(contents_view),
      active_workspace_(NULL),
      workspace_size_(
          gfx::Screen::GetMonitorAreaNearestWindow(contents_view_).size()),
      is_overview_(false),
      ignored_window_(NULL),
      grid_size_(0),
      shelf_(NULL) {
  DCHECK(contents_view);
}

WorkspaceManager::~WorkspaceManager() {
  for (size_t i = 0; i < workspaces_.size(); ++i) {
    Workspace* workspace = workspaces_[i];
    for (size_t j = 0; j < workspace->windows().size(); ++j)
      workspace->windows()[j]->RemoveObserver(this);
  }
  std::vector<Workspace*> copy_to_delete(workspaces_);
  STLDeleteElements(&copy_to_delete);
}

bool WorkspaceManager::IsManagedWindow(aura::Window* window) const {
  return window->type() == aura::client::WINDOW_TYPE_NORMAL &&
         !window->transient_parent();
}

void WorkspaceManager::AddWindow(aura::Window* window) {
  DCHECK(IsManagedWindow(window));

  Workspace* current_workspace = FindBy(window);
  if (current_workspace) {
    // Already know about this window. Make sure the workspace is active.
    if (active_workspace_ != current_workspace) {
      if (active_workspace_)
        window->layer()->GetAnimator()->StopAnimating();
      current_workspace->Activate();
    }
    return;
  }

  if (!wm::IsWindowMaximized(window) && !wm::IsWindowFullscreen(window)) {
    SetRestoreBounds(window, window->bounds());
  }

  if (!window->GetProperty(aura::client::kShowStateKey))
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);

  if (wm::IsWindowMaximized(window) || wm::IsWindowFullscreen(window)) {
    SetFullScreenOrMaximizedBounds(window);
  } else {
    if (grid_size_ > 1)
      SetWindowBounds(window, AlignBoundsToGrid(window->GetTargetBounds()));
  }

  // Add the observer after we change the state in anyway.
  window->AddObserver(this);

  Workspace* workspace = NULL;
  Workspace::Type type_for_window = Workspace::TypeForWindow(window);
  switch (type_for_window) {
    case Workspace::TYPE_MANAGED:
      // All normal windows go in the same workspace.
      workspace = GetManagedWorkspace();
      break;

    case Workspace::TYPE_MAXIMIZED:
      // All maximized windows go in their own workspace.
      break;
  }

  if (!workspace)
    workspace = CreateWorkspace(type_for_window);
  workspace->AddWindowAfter(window, NULL);
  workspace->Activate();
}

void WorkspaceManager::RemoveWindow(aura::Window* window) {
  Workspace* workspace = FindBy(window);
  if (!workspace)
    return;
  window->RemoveObserver(this);
  workspace->RemoveWindow(window);
  if (workspace->is_empty())
    delete workspace;
}

void WorkspaceManager::SetActiveWorkspaceByWindow(aura::Window* window) {
  Workspace* workspace = FindBy(window);
  if (workspace)
    workspace->Activate();
}

gfx::Rect WorkspaceManager::GetDragAreaBounds() {
  return GetWorkAreaBounds();
}

void WorkspaceManager::SetOverview(bool overview) {
  if (is_overview_ == overview)
    return;
  NOTIMPLEMENTED();
}

void WorkspaceManager::SetWorkspaceSize(const gfx::Size& workspace_size) {
  if (workspace_size == workspace_size_)
    return;
  workspace_size_ = workspace_size;
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end(); ++i) {
    (*i)->SetBounds(GetWorkAreaBounds());
  }
}

gfx::Rect WorkspaceManager::AlignBoundsToGrid(const gfx::Rect& bounds) {
  if (grid_size_ <= 1)
    return bounds;
  return AlignRectToGrid(bounds, grid_size_);
}

void WorkspaceManager::OnWindowPropertyChanged(aura::Window* window,
                                               const void* key,
                                               intptr_t old) {
  if (!IsManagedWindow(window))
    return;

  if (key != aura::client::kShowStateKey)
    return;

  DCHECK(FindBy(window));

  Workspace::Type old_type = FindBy(window)->type();
  Workspace::Type new_type = Workspace::TypeForWindow(window);
  if (new_type != old_type) {
    OnTypeOfWorkspacedNeededChanged(window);
  } else if (new_type == Workspace::TYPE_MAXIMIZED) {
    // Even though the type didn't change, the window may have gone from
    // maximized to fullscreen. Adjust the bounds appropriately.
    SetFullScreenOrMaximizedBounds(window);
  }
  UpdateShelfVisibility();
}

////////////////////////////////////////////////////////////////////////////////
// WorkspaceManager, private:

void WorkspaceManager::AddWorkspace(Workspace* workspace) {
  DCHECK(std::find(workspaces_.begin(), workspaces_.end(),
                   workspace) == workspaces_.end());
  workspace->SetBounds(GetWorkAreaBounds());
  if (active_workspace_) {
    // New workspaces go right after current workspace.
    Workspaces::iterator i = std::find(workspaces_.begin(), workspaces_.end(),
                                       active_workspace_);
    workspaces_.insert(++i, workspace);
  } else {
    workspaces_.push_back(workspace);
  }
}

void WorkspaceManager::RemoveWorkspace(Workspace* workspace) {
  Workspaces::iterator i = std::find(workspaces_.begin(),
                                     workspaces_.end(),
                                     workspace);
  DCHECK(i != workspaces_.end());
  i = workspaces_.erase(i);
  if (active_workspace_ == workspace) {
    // TODO: need mru order.
    if (i != workspaces_.end())
      SetActiveWorkspace(*i);
    else if (!workspaces_.empty())
      SetActiveWorkspace(workspaces_.back());
    else
      active_workspace_ = NULL;
  }
}

void WorkspaceManager::UpdateShelfVisibility() {
  if (!shelf_ || !active_workspace_)
    return;
  std::set<aura::Window*> windows;
  windows.insert(active_workspace_->windows().begin(),
                 active_workspace_->windows().end());
  shelf_->SetVisible(!wm::HasFullscreenWindow(windows));
}

void WorkspaceManager::SetVisibilityOfWorkspaceWindows(
    ash::internal::Workspace* workspace,
    AnimateChangeType change_type,
    bool value) {
  std::vector<aura::Window*> children;
  BuildWindowList(workspace->windows(), &children);
  SetWindowLayerVisibility(children, change_type, value);
}

void WorkspaceManager::SetWindowLayerVisibility(
    const std::vector<aura::Window*>& windows,
    AnimateChangeType change_type,
    bool value) {
  for (size_t i = 0; i < windows.size(); ++i) {
    ui::Layer* layer = windows[i]->layer();
    // Only show the layer for windows that want to be visible.
    if (layer && (!value || windows[i]->TargetVisibility())) {
      windows[i]->SetProperty(aura::client::kAnimationsDisabledKey,
                              change_type == DONT_ANIMATE);
      bool update_layer = true;
      if (change_type == ANIMATE) {
        ash::SetWindowVisibilityAnimationType(
            windows[i],
            value ? ash::WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_SHOW :
                    ash::WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_HIDE);
        if (ash::internal::AnimateOnChildWindowVisibilityChanged(
                windows[i], value))
          update_layer = false;
      }
      if (update_layer)
        layer->SetVisible(value);
      // Reset the animation type so it isn't used in a future hide/show.
      ash::SetWindowVisibilityAnimationType(
          windows[i], ash::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
    }
  }
}

Workspace* WorkspaceManager::GetActiveWorkspace() const {
  return active_workspace_;
}

Workspace* WorkspaceManager::FindBy(aura::Window* window) const {
  int index = GetWorkspaceIndexContaining(window);
  return index < 0 ? NULL : workspaces_[index];
}

void WorkspaceManager::SetActiveWorkspace(Workspace* workspace) {
  if (active_workspace_ == workspace)
    return;
  DCHECK(std::find(workspaces_.begin(), workspaces_.end(),
                   workspace) != workspaces_.end());
  if (active_workspace_)
    SetVisibilityOfWorkspaceWindows(active_workspace_, ANIMATE, false);
  Workspace* last_active = active_workspace_;
  active_workspace_ = workspace;
  if (active_workspace_) {
    SetVisibilityOfWorkspaceWindows(active_workspace_,
                                    last_active ? ANIMATE : DONT_ANIMATE, true);
    UpdateShelfVisibility();
  }

  is_overview_ = false;
}

gfx::Rect WorkspaceManager::GetWorkAreaBounds() {
  gfx::Rect bounds(workspace_size_);
  bounds.Inset(Shell::GetRootWindow()->screen()->work_area_insets());
  return bounds;
}

// Returns the index of the workspace that contains the |window|.
int WorkspaceManager::GetWorkspaceIndexContaining(aura::Window* window) const {
  for (Workspaces::const_iterator i = workspaces_.begin();
       i != workspaces_.end();
       ++i) {
    if ((*i)->Contains(window))
      return i - workspaces_.begin();
  }
  return -1;
}

void WorkspaceManager::SetWindowBounds(aura::Window* window,
                                       const gfx::Rect& bounds) {
  ignored_window_ = window;
  window->SetBounds(bounds);
  ignored_window_ = NULL;
}

void WorkspaceManager::SetWindowBoundsFromRestoreBounds(aura::Window* window) {
  const gfx::Rect* restore = GetRestoreBounds(window);
  gfx::Rect bounds;
  if (restore)
    bounds = restore->AdjustToFit(GetWorkAreaBounds());
  else
    bounds = window->bounds().AdjustToFit(GetWorkAreaBounds());
  SetWindowBounds(window, AlignRectToGrid(bounds, grid_size_));
  ash::ClearRestoreBounds(window);
}

void WorkspaceManager::SetFullScreenOrMaximizedBounds(aura::Window* window) {
  if (!GetRestoreBounds(window))
    SetRestoreBounds(window, window->GetTargetBounds());
  if (wm::IsWindowMaximized(window))
    SetWindowBounds(window, GetWorkAreaBounds());
  else if (wm::IsWindowFullscreen(window))
    SetWindowBounds(window, gfx::Screen::GetMonitorAreaNearestWindow(window));
}

void WorkspaceManager::OnTypeOfWorkspacedNeededChanged(aura::Window* window) {
  DCHECK(IsManagedWindow(window));
  Workspace* current_workspace = FindBy(window);
  DCHECK(current_workspace);
  Workspace* new_workspace = NULL;
  if (Workspace::TypeForWindow(window) == Workspace::TYPE_MAXIMIZED) {
    // Unmaximized -> maximized; create a new workspace.
    SetRestoreBounds(window, window->bounds());
    current_workspace->RemoveWindow(window);
    new_workspace = CreateWorkspace(Workspace::TYPE_MAXIMIZED);
    new_workspace->AddWindowAfter(window, NULL);
    SetFullScreenOrMaximizedBounds(window);
  } else {
    // Maximized -> unmaximized; move window to unmaximized workspace.
    wm::SetOpenWindowSplit(window, false);
    new_workspace = GetManagedWorkspace();
    current_workspace->RemoveWindow(window);
    if (!new_workspace)
      new_workspace = CreateWorkspace(Workspace::TYPE_MANAGED);
    SetWindowBoundsFromRestoreBounds(window);
    new_workspace->AddWindowAfter(window, NULL);
  }
  SetActiveWorkspace(new_workspace);
  if (current_workspace->is_empty()) {
    // Delete at the end so that we don't attempt to switch to another
    // workspace in RemoveWorkspace().
    delete current_workspace;
  }
}

Workspace* WorkspaceManager::GetManagedWorkspace() {
  for (size_t i = 0; i < workspaces_.size(); ++i) {
    if (workspaces_[i]->type() == Workspace::TYPE_MANAGED)
      return workspaces_[i];
  }
  return NULL;
}

Workspace* WorkspaceManager::CreateWorkspace(Workspace::Type type) {
  Workspace* workspace = NULL;
  if (type == Workspace::TYPE_MAXIMIZED)
    workspace = new MaximizedWorkspace(this);
  else
    workspace = new ManagedWorkspace(this);
  AddWorkspace(workspace);
  return workspace;
}

}  // namespace internal
}  // namespace ash
