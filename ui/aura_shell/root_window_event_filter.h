// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_ROOT_WINDOW_EVENT_FILTER_H_
#define UI_AURA_SHELL_ROOT_WINDOW_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/aura/event_filter.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura_shell {
namespace internal {

// RootWindowEventFilter gets all root window events first and can provide
// actions to those events. It implements root window features such as click to
// activate a window and cursor change when moving mouse.
// Additional event filters can be added to RootWIndowEventFilter. Events will
// pass through those additional filters in their addition order and could be
// consumed by any of those filters. If an event is consumed by a filter, the
// rest of the filter(s) and RootWindowEventFilter will not see the consumed
// event.
class AURA_SHELL_EXPORT RootWindowEventFilter : public aura::EventFilter {
 public:
  RootWindowEventFilter();
  virtual ~RootWindowEventFilter();

  // Adds/removes additional event filters.
  void AddFilter(aura::EventFilter* filter);
  void RemoveFilter(aura::EventFilter* filter);

  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;

 private:
  // Updates the cursor if the target provides a custom one, and provides
  // default resize cursors for window edges.
  void UpdateCursor(aura::Window* target, aura::MouseEvent* event);

  // Dispatches event to additional filters. Returns false or
  // ui::TOUCH_STATUS_UNKNOWN if event is consumed.
  bool FilterKeyEvent(aura::Window* target, aura::KeyEvent* event);
  bool FilterMouseEvent(aura::Window* target, aura::MouseEvent* event);
  ui::TouchStatus FilterTouchEvent(aura::Window* target,
                                   aura::TouchEvent* event);

  // Additional event filters that pre-handles events.
  ObserverList<aura::EventFilter, true> filters_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowEventFilter);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_ROOT_WINDOW_EVENT_FILTER_H_
