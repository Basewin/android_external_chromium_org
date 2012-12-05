// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/base_focus_rules.h"

#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/window_modality_controller.h"

namespace views {
namespace corewm {
namespace {

aura::Window* GetFocusedWindow(aura::Window* context) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(context);
  return focus_client ? focus_client->GetFocusedWindow() : NULL;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BaseFocusRules, protected:

BaseFocusRules::BaseFocusRules() {
}

BaseFocusRules::~BaseFocusRules() {
}

bool BaseFocusRules::IsWindowConsideredVisibleForActivation(
    aura::Window* window) const {
  return window->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////
// BaseFocusRules, FocusRules implementation:

bool BaseFocusRules::CanActivateWindow(aura::Window* window) const {
  // It is possible to activate a NULL window, it is equivalent to clearing
  // activation.
  if (!window)
    return true;

  // The window must in a valid hierarchy.
  if (!window->GetRootWindow())
    return false;

  // The window must be visible.
  if (!IsWindowConsideredVisibleForActivation(window))
    return false;

  // The window's activation delegate must allow this window to be activated.
  if (aura::client::GetActivationDelegate(window) &&
      !aura::client::GetActivationDelegate(window)->ShouldActivate()) {
    return false;
  }

  // The window must exist within a container that supports activation.
  // The window cannot be blocked by a modal transient.
  return SupportsChildActivation(window->parent()) &&
      !GetModalTransientForActivatableWindow(window);
}

bool BaseFocusRules::CanFocusWindow(aura::Window* window) const {
  // It is possible to focus a NULL window, it is equivalent to clearing focus.
  if (!window)
    return true;

  // The focused window is always inside the active window, so windows that
  // aren't activatable can't contain the focused window.
  aura::Window* activatable = GetActivatableWindow(window);
  if (!activatable->Contains(window))
    return false;
  return window->CanFocus();
}

aura::Window* BaseFocusRules::GetActivatableWindow(aura::Window* window) const {
  aura::Window* parent = window->parent();
  aura::Window* child = window;
  while (parent) {
    if (CanActivateWindow(child))
      return child;

    if (child->transient_parent())
      return GetActivatableWindow(child->transient_parent());

    parent = parent->parent();
    child = child->parent();
  }
  return NULL;
}

aura::Window* BaseFocusRules::GetFocusableWindow(aura::Window* window) const {
  if (CanFocusWindow(window))
    return window;

  // |window| may be in a hierarchy that is non-activatable, in which case we
  // need to cut over to the activatable hierarchy.
  aura::Window* activatable = GetActivatableWindow(window);
  if (!activatable)
    return GetFocusedWindow(window);

  if (!activatable->Contains(window)) {
    // If there's already a child window focused in the activatable hierarchy,
    // just use that (i.e. don't shift focus), otherwise we need to at least cut
    // over to the activatable hierarchy.
    aura::Window* focused = GetFocusedWindow(activatable);
    return activatable->Contains(focused) ? focused : activatable;
  }

  while (window && !CanFocusWindow(window))
    window = window->parent();
  return window;
}

aura::Window* BaseFocusRules::GetNextActivatableWindow(
    aura::Window* ignore) const {
  DCHECK(ignore);

  // Can be called from the RootWindow's destruction, which has a NULL parent.
  if (!ignore->parent())
    return NULL;

  // In the basic scenarios handled by BasicFocusRules, the pool of activatable
  // windows is limited to the |ignore|'s siblings.
  const aura::Window::Windows& siblings = ignore->parent()->children();
  DCHECK(!siblings.empty());

  for (aura::Window::Windows::const_reverse_iterator rit = siblings.rbegin();
       rit != siblings.rend();
       ++rit) {
    aura::Window* cur = *rit;
    if (cur == ignore)
      continue;
    if (CanActivateWindow(cur))
      return cur;
  }
  return NULL;
}

aura::Window* BaseFocusRules::GetNextFocusableWindow(
    aura::Window* ignore) const {
  DCHECK(ignore);

  // Focus cycling is currently very primitive: we just climb the tree.
  // If need be, we could add the ability to look at siblings, descend etc.
  // For test coverage, see FocusControllerImplicitTestBase subclasses in
  // focus_controller_unittest.cc.
  return GetFocusableWindow(ignore->parent());
}


}  // namespace corewm
}  // namespace views
