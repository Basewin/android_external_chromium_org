// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CURSOR_MANAGER_CHROMEOS_H_
#define ASH_WM_CURSOR_MANAGER_CHROMEOS_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace ui {
class KeyEvent;
}

namespace wm {
class NativeCursorManager;
}

namespace ash {

// This class overrides the cursor hiding behaviour on ChromeOS. The cursor is
// hidden on certain key events only if the accessibility keyboard is not
// enabled.
class ASH_EXPORT CursorManager : public ::wm::CursorManager {
 public:
  explicit CursorManager(
      scoped_ptr< ::wm::NativeCursorManager> delegate);
  virtual ~CursorManager();

  // aura::client::CursorClient:
  virtual bool ShouldHideCursorOnKeyEvent(
      const ui::KeyEvent& event) const OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(CursorManager);
};

}  // namespace ash

#endif  // ASH_WM_CURSOR_MANAGER_CHROMEOS_H_
