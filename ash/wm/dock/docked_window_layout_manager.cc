// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace internal {

// Minimum, maximum width of the dock area and a width of the gap
// static
const int DockedWindowLayoutManager::kMaxDockWidth = 360;
// static
const int DockedWindowLayoutManager::kMinDockWidth = 200;
// static
const int DockedWindowLayoutManager::kMinDockGap = 2;
//static
const int DockedWindowLayoutManager::kIdealWidth = 250;
//static
const int kMaxVisibleWindows = 2;
const int kSlideDurationMs = 120;
const int kFadeDurationMs = 720;

namespace {

const SkColor kDockBackgroundColor = SkColorSetARGB(0xff, 0x10, 0x10, 0x10);
const float kDockBackgroundOpacity = 0.5f;

class DockedBackgroundWidget : public views::Widget {
 public:
  explicit DockedBackgroundWidget(aura::Window* container) {
    InitWidget(container);
  }

 private:
  void InitWidget(aura::Window* parent) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
    params.can_activate = false;
    params.keep_on_top = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = parent;
    params.accept_events = false;
    set_focus_on_creation(false);
    Init(params);
    DCHECK_EQ(GetNativeView()->GetRootWindow(), parent->GetRootWindow());
    views::View* content_view = new views::View;
    content_view->set_background(
        views::Background::CreateSolidBackground(kDockBackgroundColor));
    SetContentsView(content_view);
    Hide();
  }

  DISALLOW_COPY_AND_ASSIGN(DockedBackgroundWidget);
};

DockedWindowLayoutManager* GetDockLayoutManager(aura::Window* window,
                                                const gfx::Point& location) {
  gfx::Rect near_location(location, gfx::Size());
  aura::Window* dock = Shell::GetContainer(
      wm::GetRootWindowMatching(near_location),
      kShellWindowId_DockedContainer);
  return static_cast<internal::DockedWindowLayoutManager*>(
      dock->layout_manager());
}

// Certain windows (minimized, hidden or popups) do not matter to docking.
bool IsUsedByLayout(aura::Window* window) {
  return (window->IsVisible() &&
          !wm::GetWindowState(window)->IsMinimized() &&
          window->type() != aura::client::WINDOW_TYPE_POPUP);
}

// A functor used to sort the windows in order of their center Y position.
// |delta| is a pre-calculated distance from the bottom of one window to the top
// of the next. Its value can be positive (gap) or negative (overlap).
// Half of |delta| is used as a transition point at which windows could ideally
// swap positions.
struct CompareWindowPos {
  CompareWindowPos(aura::Window* dragged_window, float delta)
      : dragged_window_(dragged_window),
        delta_(delta / 2) {}

  bool operator()(aura::Window* win1, aura::Window* win2) {
    // Use target coordinates since animations may be active when windows are
    // reordered.
    const gfx::Rect win1_bounds = ScreenAsh::ConvertRectToScreen(
        win1->parent(), win1->GetTargetBounds());
    const gfx::Rect win2_bounds = ScreenAsh::ConvertRectToScreen(
        win2->parent(), win2->GetTargetBounds());
    // If one of the windows is the |dragged_window_| attempt to make an
    // earlier swap between the windows than just based on their centers.
    // This is possible if the dragged window is at least as tall as the other
    // window.
    if (win1 == dragged_window_)
      return compare_two_windows(win1_bounds, win2_bounds);
    if (win2 == dragged_window_)
      return !compare_two_windows(win2_bounds, win1_bounds);
    // Otherwise just compare the centers.
    return win1_bounds.CenterPoint().y() < win2_bounds.CenterPoint().y();
  }

  // Based on center point tries to deduce where the drag is coming from.
  // When dragging from below up the transition point is lower.
  // When dragging from above down the transition point is higher.
  bool compare_bounds(const gfx::Rect dragged, const gfx::Rect other) {
    if (dragged.CenterPoint().y() < other.CenterPoint().y())
      return dragged.CenterPoint().y() < other.y() - delta_;
    return dragged.CenterPoint().y() < other.bottom() + delta_;
  }

  // Performs comparison both ways and selects stable result.
  bool compare_two_windows(const gfx::Rect bounds1, const gfx::Rect bounds2) {
    // Try comparing windows in both possible orders and see if the comparison
    // is stable.
    bool result1 = compare_bounds(bounds1, bounds2);
    bool result2 = compare_bounds(bounds2, bounds1);
    if (result1 != result2)
      return result1;

    // Otherwise it is not possible to be sure that the windows will not bounce.
    // In this case just compare the centers.
    return bounds1.CenterPoint().y() < bounds2.CenterPoint().y();
  }

 private:
  aura::Window* dragged_window_;
  float delta_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager public implementation:
DockedWindowLayoutManager::DockedWindowLayoutManager(
    aura::Window* dock_container)
    : dock_container_(dock_container),
      in_layout_(false),
      dragged_window_(NULL),
      is_dragged_window_docked_(false),
      is_dragged_from_dock_(false),
      launcher_(NULL),
      shelf_layout_manager_(NULL),
      shelf_hidden_(false),
      docked_width_(0),
      max_visible_windows_(kMaxVisibleWindows),
      alignment_(DOCKED_ALIGNMENT_NONE),
      last_active_window_(NULL),
      background_widget_(new DockedBackgroundWidget(dock_container_)) {
  DCHECK(dock_container);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

DockedWindowLayoutManager::~DockedWindowLayoutManager() {
  Shutdown();
}

void DockedWindowLayoutManager::Shutdown() {
  if (shelf_layout_manager_) {
    shelf_layout_manager_->RemoveObserver(this);
  }
  shelf_layout_manager_ = NULL;
  launcher_ = NULL;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* child = dock_container_->children()[i];
    child->RemoveObserver(this);
    wm::GetWindowState(child)->RemoveObserver(this);
  }
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void DockedWindowLayoutManager::AddObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DockedWindowLayoutManager::RemoveObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DockedWindowLayoutManager::StartDragging(aura::Window* window) {
  DCHECK(!dragged_window_);
  dragged_window_ = window;
  // Start observing a window unless it is docked container's child in which
  // case it is already observed.
  if (dragged_window_->parent() != dock_container_) {
    dragged_window_->AddObserver(this);
    wm::GetWindowState(dragged_window_)->AddObserver(this);
  }
  is_dragged_from_dock_ = window->parent() == dock_container_;
  DCHECK(!is_dragged_window_docked_);
}

void DockedWindowLayoutManager::DockDraggedWindow(aura::Window* window) {
  OnWindowDocked(window);
  Relayout();
}

void DockedWindowLayoutManager::UndockDraggedWindow() {
  OnWindowUndocked();
  Relayout();
  UpdateDockBounds();
  is_dragged_from_dock_ = false;
}

void DockedWindowLayoutManager::FinishDragging() {
  DCHECK(dragged_window_);
  if (is_dragged_window_docked_)
    OnWindowUndocked();
  DCHECK (!is_dragged_window_docked_);
  // Stop observing a window unless it is docked container's child in which
  // case it needs to keep being observed after the drag completes.
  if (dragged_window_->parent() != dock_container_) {
    dragged_window_->RemoveObserver(this);
    wm::GetWindowState(dragged_window_)->RemoveObserver(this);
    if (last_active_window_ == dragged_window_)
      last_active_window_ = NULL;
  } else {
    // A window is no longer dragged and is a child.
    // When a window becomes a child at drag start this is
    // the only opportunity we will have to enforce a window
    // count limit so do it here.
    MaybeMinimizeChildrenExcept(dragged_window_);
  }
  dragged_window_ = NULL;
  dragged_bounds_ = gfx::Rect();
  Relayout();
  UpdateDockBounds();
}

void DockedWindowLayoutManager::SetLauncher(ash::Launcher* launcher) {
  DCHECK(!launcher_);
  DCHECK(!shelf_layout_manager_);
  launcher_ = launcher;
  if (launcher_->shelf_widget()) {
    shelf_layout_manager_ = ash::internal::ShelfLayoutManager::ForLauncher(
        launcher_->shelf_widget()->GetNativeWindow());
    WillChangeVisibilityState(shelf_layout_manager_->visibility_state());
    shelf_layout_manager_->AddObserver(this);
  }
}

DockedAlignment DockedWindowLayoutManager::GetAlignmentOfWindow(
    const aura::Window* window) const {
  const gfx::Rect& bounds(window->GetBoundsInScreen());

  // Test overlap with an existing docked area first.
  if (docked_bounds_.Intersects(bounds) &&
      alignment_ != DOCKED_ALIGNMENT_NONE) {
    // A window is being added to other docked windows (on the same side).
    return alignment_;
  }

  const gfx::Rect container_bounds = dock_container_->GetBoundsInScreen();
  if (bounds.x() <= container_bounds.x() &&
      bounds.right() > container_bounds.x()) {
    return DOCKED_ALIGNMENT_LEFT;
  } else if (bounds.x() < container_bounds.right() &&
             bounds.right() >= container_bounds.right()) {
    return DOCKED_ALIGNMENT_RIGHT;
  }
  return DOCKED_ALIGNMENT_NONE;
}

DockedAlignment DockedWindowLayoutManager::CalculateAlignment() const {
  // Find a child that is not being dragged and is not a popup.
  // If such exists the current alignment is returned - even if some of the
  // children are hidden or minimized (so they can be restored without losing
  // the docked state).
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);
    if (window != dragged_window_ &&
        window->type() != aura::client::WINDOW_TYPE_POPUP) {
      return alignment_;
    }
  }
  // No docked windows remain other than possibly the window being dragged.
  // Return |NONE| to indicate that windows may get docked on either side.
  return DOCKED_ALIGNMENT_NONE;
}

bool DockedWindowLayoutManager::CanDockWindow(aura::Window* window,
                                              SnapType edge) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableDockedWindows)) {
    return false;
  }
  // If a window is wide and cannot be resized down to maximum width allowed
  // then it cannot be docked.
  if (window->bounds().width() > kMaxDockWidth &&
      (!wm::GetWindowState(window)->CanResize() ||
       (window->delegate() &&
        window->delegate()->GetMinimumSize().width() != 0 &&
        window->delegate()->GetMinimumSize().width() > kMaxDockWidth))) {
    return false;
  }
  // Cannot dock on the other size from an existing dock.
  const DockedAlignment alignment = CalculateAlignment();
  if ((edge == SNAP_LEFT && alignment == DOCKED_ALIGNMENT_RIGHT) ||
      (edge == SNAP_RIGHT && alignment == DOCKED_ALIGNMENT_LEFT)) {
    return false;
  }
  // Do not allow docking on the same side as launcher shelf.
  ShelfAlignment shelf_alignment = SHELF_ALIGNMENT_BOTTOM;
  if (launcher_)
    shelf_alignment = launcher_->alignment();
  if ((edge == SNAP_LEFT && shelf_alignment == SHELF_ALIGNMENT_LEFT) ||
      (edge == SNAP_RIGHT && shelf_alignment == SHELF_ALIGNMENT_RIGHT)) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, aura::LayoutManager implementation:
void DockedWindowLayoutManager::OnWindowResized() {
  Relayout();
  // When screen resizes update the insets even when dock width or alignment
  // does not change.
  UpdateDockBounds();
}

void DockedWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  // Dragged windows are already observed by StartDragging and do not change
  // docked alignment during the drag.
  if (child == dragged_window_)
    return;
  // If this is the first window getting docked - update alignment.
  if (alignment_ == DOCKED_ALIGNMENT_NONE) {
    alignment_ = GetAlignmentOfWindow(child);
    DCHECK(alignment_ != DOCKED_ALIGNMENT_NONE);
  }
  MaybeMinimizeChildrenExcept(child);
  child->AddObserver(this);
  wm::GetWindowState(child)->AddObserver(this);
  Relayout();
  UpdateDockBounds();
}

void DockedWindowLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  // Dragged windows are stopped being observed by FinishDragging and do not
  // change alignment during the drag. They also cannot be set to be the
  // |last_active_window_|.
  if (child == dragged_window_)
    return;
  // If this is the last window, set alignment and maximize the workspace.
  if (!IsAnyWindowDocked()) {
    alignment_ = DOCKED_ALIGNMENT_NONE;
    docked_width_ = 0;
  }
  if (last_active_window_ == child)
    last_active_window_ = NULL;
  child->RemoveObserver(this);
  wm::GetWindowState(child)->RemoveObserver(this);
  Relayout();
}

void DockedWindowLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  if (visible)
    wm::GetWindowState(child)->Restore();
  Relayout();
  UpdateDockBounds();
}

void DockedWindowLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // Whenever one of our windows is moved or resized enforce layout.
  SetChildBoundsDirect(child, requested_bounds);
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, ash::ShellObserver implementation:

void DockedWindowLayoutManager::OnShelfAlignmentChanged(
    aura::RootWindow* root_window) {
  if (dock_container_->GetRootWindow() != root_window)
    return;

  if (!launcher_ || !launcher_->shelf_widget())
    return;

  if (alignment_ == DOCKED_ALIGNMENT_NONE)
    return;

  // Do not allow launcher and dock on the same side. Switch side that
  // the dock is attached to and move all dock windows to that new side.
  ShelfAlignment shelf_alignment = launcher_->shelf_widget()->GetAlignment();
  if (alignment_ == DOCKED_ALIGNMENT_LEFT &&
      shelf_alignment == SHELF_ALIGNMENT_LEFT) {
    alignment_ = DOCKED_ALIGNMENT_RIGHT;
  } else if (alignment_ == DOCKED_ALIGNMENT_RIGHT &&
             shelf_alignment == SHELF_ALIGNMENT_RIGHT) {
    alignment_ = DOCKED_ALIGNMENT_LEFT;
  }
  Relayout();
  UpdateDockBounds();
}

/////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, WindowStateObserver implementation:

void DockedWindowLayoutManager::OnWindowShowTypeChanged(
    wm::WindowState* window_state,
    wm::WindowShowType old_type) {
  // The window property will still be set, but no actual change will occur
  // until WillChangeVisibilityState is called when the shelf is visible again
  if (shelf_hidden_)
    return;
  if (window_state->IsMinimized())
    MinimizeDockedWindow(window_state);
  else
    RestoreDockedWindow(window_state);
}

/////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, WindowObserver implementation:

void DockedWindowLayoutManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  // Only relayout if the dragged window would get docked.
  if (window == dragged_window_ && is_dragged_window_docked_)
    Relayout();
}

void DockedWindowLayoutManager::OnWindowVisibilityChanging(
    aura::Window* window, bool visible) {
  int animation_type = WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE;
  if (visible) {
    animation_type = views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT;
    views::corewm::SetWindowVisibilityAnimationDuration(
        window, base::TimeDelta::FromMilliseconds(kFadeDurationMs));
  }
  views::corewm::SetWindowVisibilityAnimationType(window, animation_type);
}

void DockedWindowLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (dragged_window_ == window) {
    FinishDragging();
    DCHECK(!dragged_window_);
    DCHECK (!is_dragged_window_docked_);
  }
  if (window == last_active_window_)
    last_active_window_ = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, aura::client::ActivationChangeObserver implementation:

void DockedWindowLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  // Ignore if the window that is not managed by this was activated.
  aura::Window* ancestor = NULL;
  for (aura::Window* parent = gained_active;
       parent; parent = parent->parent()) {
    if (parent->parent() == dock_container_) {
      ancestor = parent;
      break;
    }
  }
  if (ancestor)
    UpdateStacking(ancestor);
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager, ShelfLayoutManagerObserver implementation:

void DockedWindowLayoutManager::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  // On entering / leaving full screen mode the shelf visibility state is
  // changed to / from SHELF_HIDDEN. In this state, docked windows should hide
  // to allow the full-screen application to use the full screen.

  // TODO(varkha): ShelfLayoutManager::UpdateVisibilityState sets state to
  // SHELF_AUTO_HIDE when in immersive mode. Distinguish this from
  // when shelf enters auto-hide state based on mouse hover when auto-hide
  // setting is enabled and hide all windows (immersive mode should hide docked
  // windows).
  shelf_hidden_ = new_state == ash::SHELF_HIDDEN;
  {
    // prevent Relayout from getting called multiple times during this
    base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
    for (size_t i = 0; i < dock_container_->children().size(); ++i) {
      aura::Window* window = dock_container_->children()[i];
      if (window->type() == aura::client::WINDOW_TYPE_POPUP)
        continue;
      wm::WindowState* window_state = wm::GetWindowState(window);
      if (shelf_hidden_) {
        if (window->IsVisible())
          MinimizeDockedWindow(window_state);
      } else {
        if (!window_state->IsMinimized())
          RestoreDockedWindow(window_state);
      }
    }
  }
  Relayout();
  UpdateDockBounds();
}

////////////////////////////////////////////////////////////////////////////////
// DockLayoutManager private implementation:

void DockedWindowLayoutManager::MaybeMinimizeChildrenExcept(
    aura::Window* child) {
  // Account for the |child| by initializing to 1.
  int child_index = 1;
  aura::Window::Windows children(dock_container_->children());
  aura::Window::Windows::const_reverse_iterator iter = children.rbegin();
  while (iter != children.rend()) {
    aura::Window* window(*iter++);
    if (window == child || !IsUsedByLayout(window))
      continue;
    if (++child_index > max_visible_windows_)
      wm::GetWindowState(window)->Minimize();
  }
}

void DockedWindowLayoutManager::MinimizeDockedWindow(
    wm::WindowState* window_state) {
  DCHECK_NE(window_state->window()->type(), aura::client::WINDOW_TYPE_POPUP);
  window_state->window()->Hide();
  if (window_state->IsActive())
    window_state->Deactivate();
}

void DockedWindowLayoutManager::RestoreDockedWindow(
    wm::WindowState* window_state) {
  aura::Window* window = window_state->window();
  DCHECK_NE(window->type(), aura::client::WINDOW_TYPE_POPUP);
  // Always place restored window at the top shuffling the other windows down.
  // TODO(varkha): add a separate container for docked windows to keep track
  // of ordering.
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      dock_container_);
  const gfx::Rect work_area = display.work_area();
  gfx::Rect bounds(window->bounds());
  bounds.set_y(work_area.y() - bounds.height());
  window->SetBounds(bounds);
  window->Show();
  MaybeMinimizeChildrenExcept(window);
}

void DockedWindowLayoutManager::OnWindowDocked(aura::Window* window) {
  DCHECK(!is_dragged_window_docked_);
  is_dragged_window_docked_ = true;

  // If there are no other docked windows update alignment.
  if (!IsAnyWindowDocked())
    alignment_ = DOCKED_ALIGNMENT_NONE;
}

void DockedWindowLayoutManager::OnWindowUndocked() {
  // If this is the first window getting docked - update alignment.
  if (!IsAnyWindowDocked())
    alignment_ = GetAlignmentOfWindow(dragged_window_);

  DCHECK (is_dragged_window_docked_);
  is_dragged_window_docked_ = false;
}

bool DockedWindowLayoutManager::IsAnyWindowDocked() {
  return CalculateAlignment() != DOCKED_ALIGNMENT_NONE;
}

// static
int DockedWindowLayoutManager::GetWindowWidthCloseTo(aura::Window* window,
                                                     int target_width) {
  if (!wm::GetWindowState(window)->CanResize()) {
    DCHECK_LE(window->bounds().width(), kMaxDockWidth);
    return window->bounds().width();
  }
  int width = std::max(kMinDockWidth, std::min(target_width, kMaxDockWidth));
  if (window->delegate()) {
    if (window->delegate()->GetMinimumSize().width() != 0)
      width = std::max(width, window->delegate()->GetMinimumSize().width());
    if (window->delegate()->GetMaximumSize().width() != 0)
      width = std::min(width, window->delegate()->GetMaximumSize().width());
  }
  DCHECK_LE(width, kMaxDockWidth);
  return width;
}

void DockedWindowLayoutManager::Relayout() {
  if (in_layout_)
    return;
  if (alignment_ == DOCKED_ALIGNMENT_NONE && !is_dragged_window_docked_)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  gfx::Rect dock_bounds = dock_container_->GetBoundsInScreen();
  aura::Window* active_window = NULL;
  aura::Window::Windows visible_windows;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);

    if (!IsUsedByLayout(window) || window == dragged_window_)
      continue;

    // If the shelf is currently hidden (full-screen mode), hide window until
    // full-screen mode is exited.
    if (shelf_hidden_) {
      // The call to Hide does not set the minimize property, so the window will
      // be restored when the shelf becomes visible again.
      window->Hide();
      continue;
    }
    if (window->HasFocus() ||
        window->Contains(
            aura::client::GetFocusClient(window)->GetFocusedWindow())) {
      DCHECK(!active_window);
      active_window = window;
    }
    visible_windows.push_back(window);
  }

  // Consider docked dragged_window_ when fanning out other child windows.
  if (is_dragged_window_docked_) {
    visible_windows.push_back(dragged_window_);
    DCHECK(!active_window);
    active_window = dragged_window_;
  }

  // Calculate free space or overlap.
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      dock_container_);
  const gfx::Rect work_area = display.work_area();
  int available_room = work_area.height();
  int ideal_docked_width = 0;
  for (aura::Window::Windows::const_iterator iter = visible_windows.begin();
      iter != visible_windows.end(); ++iter) {
    aura::Window* window = *iter;
    available_room -= window->bounds().height();

    // Adjust the dragged window to the dock. If that is not possible then
    // other docked windows area adjusted to the one that is being dragged.
    int adjusted_docked_width = window->bounds().width();
    if (window == dragged_window_) {
      // Adjust the dragged window width to the current dock size or ideal when
      // there are no other docked windows.
      adjusted_docked_width = GetWindowWidthCloseTo(
          window, (docked_width_ > 0) ? docked_width_ : kIdealWidth);
    } else if (!is_dragged_from_dock_ && is_dragged_window_docked_) {
      // When a docked window is dragged-in other docked windows' widths are
      // adjusted to the new width if necessary.
      // When there is no dragged docked window the docked windows retain their
      // widths.
      // When a dragged window is simply being reordered in the docked area the
      // other windows are not resized (but the dragged window can be).
      adjusted_docked_width = GetWindowWidthCloseTo(window, 0);
    }
    ideal_docked_width = std::max(ideal_docked_width, adjusted_docked_width);

    // Restrict docked area width regardless of window restrictions.
    ideal_docked_width = std::max(std::min(ideal_docked_width, kMaxDockWidth),
                                  kMinDockWidth);
  }
  const int num_windows = visible_windows.size();
  const float delta = (float)available_room /
      ((available_room > 0 || num_windows <= 1) ?
          num_windows + 1 : num_windows - 1);
  float y_pos = work_area.y() + ((available_room > 0) ? delta : 0);

  // Docked area is shown only if there is at least one non-dragged visible
  // docked window.
  docked_width_ = ideal_docked_width;
  if (visible_windows.empty() ||
      (visible_windows.size() == 1 && visible_windows[0] == dragged_window_)) {
    docked_width_ = 0;
  }

  // Sort windows by their center positions and fan out overlapping
  // windows.
  std::sort(visible_windows.begin(),
            visible_windows.end(),
            CompareWindowPos(is_dragged_from_dock_ ? dragged_window_ : NULL,
                             delta));
  for (aura::Window::Windows::const_iterator iter = visible_windows.begin();
      iter != visible_windows.end(); ++iter) {
    aura::Window* window = *iter;
    gfx::Rect bounds = ScreenAsh::ConvertRectToScreen(
        window->parent(), window->GetTargetBounds());
    // A window is extended or shrunk to be as close as possible to the docked
    // area width. Windows other than the dragged window are kept at their
    // existing size when the dragged window is just being reordered.
    // This also enforces the min / max restrictions on the docked area width.
    bounds.set_width(GetWindowWidthCloseTo(
        window,
        (!is_dragged_from_dock_ || window == dragged_window_) ?
            ideal_docked_width : bounds.width()));
    DCHECK_LE(bounds.width(), ideal_docked_width);

    DockedAlignment alignment = alignment_;
    if (alignment == DOCKED_ALIGNMENT_NONE && window == dragged_window_) {
      alignment = GetAlignmentOfWindow(window);
      if (alignment == DOCKED_ALIGNMENT_NONE)
        bounds.set_size(gfx::Size());
    }

    // Fan out windows evenly distributing the overlap or remaining free space.
    bounds.set_y(std::max(work_area.y(),
                          std::min(work_area.bottom() - bounds.height(),
                                   static_cast<int>(y_pos + 0.5))));
    y_pos += bounds.height() + delta;

    // All docked windows other than the one currently dragged remain stuck
    // to the screen edge (flush with the edge or centered in the dock area).
    switch (alignment) {
      case DOCKED_ALIGNMENT_LEFT:
        bounds.set_x(dock_bounds.x() +
                     (ideal_docked_width - bounds.width()) / 2);
        break;
      case DOCKED_ALIGNMENT_RIGHT:
        bounds.set_x(dock_bounds.right() -
                     (ideal_docked_width + bounds.width()) / 2);
        break;
      case DOCKED_ALIGNMENT_NONE:
        break;
    }
    if (window == dragged_window_) {
      dragged_bounds_ = bounds;
      continue;
    }
    // If the following asserts it is probably because not all the children
    // have been removed when dock was closed.
    DCHECK_NE(alignment_, DOCKED_ALIGNMENT_NONE);
    bounds = ScreenAsh::ConvertRectFromScreen(dock_container_, bounds);
    if (bounds != window->GetTargetBounds()) {
      ui::Layer* layer = window->layer();
      ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
      slide_settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      slide_settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kSlideDurationMs));
      SetChildBoundsDirect(window, bounds);
    }
  }
  is_dragged_from_dock_ = true;
  UpdateStacking(active_window);
}

void DockedWindowLayoutManager::UpdateDockBounds() {
  int dock_inset = docked_width_ + (docked_width_ > 0 ? kMinDockGap : 0);
  gfx::Rect bounds = gfx::Rect(
      alignment_ == DOCKED_ALIGNMENT_RIGHT && dock_inset > 0 ?
          dock_container_->bounds().right() - dock_inset:
          dock_container_->bounds().x(),
      dock_container_->bounds().y(),
      dock_inset,
      dock_container_->bounds().height());
  docked_bounds_ = bounds +
      dock_container_->GetBoundsInScreen().OffsetFromOrigin();
  FOR_EACH_OBSERVER(
      DockedWindowLayoutManagerObserver,
      observer_list_,
      OnDockBoundsChanging(bounds));

  // Show or hide background for docked area.
  gfx::Rect background_bounds(docked_bounds_);
  const gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(dock_container_).work_area();
  background_bounds.set_height(work_area.height());
  background_widget_->SetBounds(background_bounds);
  if (docked_width_ > 0) {
    background_widget_->Show();
    background_widget_->GetNativeWindow()->layer()->SetOpacity(
        kDockBackgroundOpacity);
  } else {
    background_widget_->Hide();
  }
}

void DockedWindowLayoutManager::UpdateStacking(aura::Window* active_window) {
  if (!active_window) {
    if (!last_active_window_)
      return;
    active_window = last_active_window_;
  }

  // Windows are stacked like a deck of cards:
  //  ,------.
  // |,------.|
  // |,------.|
  // | active |
  // | window |
  // |`------'|
  // |`------'|
  //  `------'
  // Use the middle of each window to figure out how to stack the window.
  // This allows us to update the stacking when a window is being dragged around
  // by the titlebar.
  std::map<int, aura::Window*> window_ordering;
  for (aura::Window::Windows::const_iterator it =
           dock_container_->children().begin();
       it != dock_container_->children().end(); ++it) {
    if (!IsUsedByLayout(*it) ||
        ((*it) == dragged_window_ && !is_dragged_window_docked_)) {
      continue;
    }
    gfx::Rect bounds = (*it)->bounds();
    window_ordering.insert(std::make_pair(bounds.y() + bounds.height() / 2,
                                          *it));
  }
  int active_center_y = active_window->bounds().CenterPoint().y();

  aura::Window* previous_window = NULL;
  for (std::map<int, aura::Window*>::const_iterator it =
       window_ordering.begin();
       it != window_ordering.end() && it->first < active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }
  for (std::map<int, aura::Window*>::const_reverse_iterator it =
       window_ordering.rbegin();
       it != window_ordering.rend() && it->first > active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }

  if (previous_window && active_window->parent() == dock_container_)
    dock_container_->StackChildAbove(active_window, previous_window);
  if (active_window != dragged_window_)
    last_active_window_ = active_window;
}

////////////////////////////////////////////////////////////////////////////////
// keyboard::KeyboardControllerObserver implementation:

void DockedWindowLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& keyboard_bounds) {
  // This bounds change will have caused a change to the Shelf which does not
  // propagate automatically to this class, so manually recalculate bounds.
  UpdateDockBounds();
}

}  // namespace internal
}  // namespace ash
