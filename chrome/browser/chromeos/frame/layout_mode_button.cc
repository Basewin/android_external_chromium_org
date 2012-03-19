// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/layout_mode_button.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace {
const int kHorizontalPaddingPixels = 2;
}  // namespace

namespace chromeos {

LayoutModeButton::LayoutModeButton()
    : ALLOW_THIS_IN_INITIALIZER_LIST(ImageButton(this)) {
  set_id(VIEW_ID_LAYOUT_MODE_BUTTON);
}

LayoutModeButton::~LayoutModeButton() {
}

gfx::Size LayoutModeButton::GetPreferredSize() {
  gfx::Size size = ImageButton::GetPreferredSize();
  size.Enlarge(2 * kHorizontalPaddingPixels, 0);
  return size;
}

bool LayoutModeButton::HitTest(const gfx::Point& l) const {
  // Handle events above our bounds so that we'll see clicks occuring in the
  // padding between the top of the screen and the button.
  const gfx::Point point(l.x(), std::max(y(), l.y()));
  return ImageButton::HitTest(point);
}

void LayoutModeButton::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_LAYOUT_MODE_CHANGED);
  UpdateForCurrentLayoutMode();
}

void LayoutModeButton::Init() {
  UpdateForCurrentLayoutMode();
}

void LayoutModeButton::ButtonPressed(views::Button* sender,
                                     const views::Event& event) {
  DCHECK_EQ(sender, this);
}

void LayoutModeButton::UpdateForCurrentLayoutMode() {
}

}  // namespace chromeos
