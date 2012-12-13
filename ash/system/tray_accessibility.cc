// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {
const int kPaddingAroundBottomRow = 5;

enum AccessibilityState {
  A11Y_NONE             = 0,
  A11Y_SPOKEN_FEEDBACK  = 1 << 0,
  A11Y_HIGH_CONTRAST    = 1 << 1,
  A11Y_SCREEN_MAGNIFIER = 1 << 2,
};

uint32 GetAccessibilityState() {
  ShellDelegate* shell_delegate = Shell::GetInstance()->delegate();
  uint32 state = A11Y_NONE;
  if (shell_delegate->IsSpokenFeedbackEnabled())
    state |= A11Y_SPOKEN_FEEDBACK;
  if (shell_delegate->IsHighContrastEnabled())
    state |= A11Y_HIGH_CONTRAST;
  if (shell_delegate->GetMagnifierType() != ash::MAGNIFIER_OFF)
    state |= A11Y_SCREEN_MAGNIFIER;
  return state;
}

user::LoginStatus GetCurrentLoginStatus() {
  return Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus();
}

}  // namespace

namespace tray {

class DefaultAccessibilityView : public TrayItemMore {
 public:
  explicit DefaultAccessibilityView(SystemTrayItem* owner)
      : TrayItemMore(owner, true) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK).
                    ToImageSkia());
    string16 label = bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY);
    SetLabel(label);
    SetAccessibleName(label);
  }

  virtual ~DefaultAccessibilityView() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultAccessibilityView);
};

class AccessibilityDetailedView : public TrayDetailsView,
                                  public ViewClickListener,
                                  public views::ButtonListener,
                                  public ShellObserver {
 public:
  explicit AccessibilityDetailedView(SystemTrayItem* owner,
                                     user::LoginStatus login) :
      TrayDetailsView(owner),
      spoken_feedback_view_(NULL),
      high_contrast_view_(NULL),
      screen_magnifier_view_(NULL),
      help_view_(NULL),
      login_(login) {

    Reset();

    AppendAccessibilityList();
    AppendHelpEntries();
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TITLE, this);

    Layout();
  }

  virtual ~AccessibilityDetailedView() {
  }

 private:
  // Add the accessibility feature list.
  void AppendAccessibilityList() {
    CreateScrollableList();
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    ShellDelegate* shell_delegate = Shell::GetInstance()->delegate();
    bool spoken_feedback_enabled = shell_delegate->IsSpokenFeedbackEnabled();
    spoken_feedback_view_ = AddScrollListItem(
        bundle.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SPOKEN_FEEDBACK),
        spoken_feedback_enabled ? gfx::Font::BOLD : gfx::Font::NORMAL,
        spoken_feedback_enabled);
    bool high_contrast_mode_enabled = shell_delegate->IsHighContrastEnabled();
    high_contrast_view_ = AddScrollListItem(
        bundle.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_HIGH_CONTRAST_MODE),
        high_contrast_mode_enabled ? gfx::Font::BOLD : gfx::Font::NORMAL,
        high_contrast_mode_enabled);
    bool screen_magnifier_enabled =
        shell_delegate->GetMagnifierType() == ash::MAGNIFIER_FULL;
    screen_magnifier_view_ = AddScrollListItem(
        bundle.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_SCREEN_MAGNIFIER),
        screen_magnifier_enabled ? gfx::Font::BOLD : gfx::Font::NORMAL,
        screen_magnifier_enabled);
  }

  // Add help entries.
  void AppendHelpEntries() {
    // Currently the help page requires a browser window.
    // TODO(yoshiki): show this even on login/lock screen. crbug.com/158286
    if (login_ == user::LOGGED_IN_NONE ||
        login_ == user::LOGGED_IN_LOCKED)
      return;

    views::View* bottom_row = new View();
    views::BoxLayout* layout = new
        views::BoxLayout(views::BoxLayout::kHorizontal,
                         kTrayMenuBottomRowPadding,
                         kTrayMenuBottomRowPadding,
                         kTrayMenuBottomRowPaddingBetweenItems);
    layout->set_spread_blank_space(true);
    bottom_row->SetLayoutManager(layout);

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    TrayPopupLabelButton* help = new TrayPopupLabelButton(
        this,
        bundle.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_ACCESSIBILITY_LEARN_MORE));
    bottom_row->AddChildView(help);
    help_view_ = help;

    // TODO(yoshiki): Add "Customize accessibility" button when the customize is
    // available. crbug.com/158281

    AddChildView(bottom_row);
  }

  HoverHighlightView* AddScrollListItem(const string16& text,
                                        gfx::Font::FontStyle style,
                                        bool checked) {
    HoverHighlightView* container = new HoverHighlightView(this);
    container->set_fixed_height(kTrayPopupItemHeight);
    container->AddCheckableLabel(text, style, checked);
    scroll_content()->AddChildView(container);
    return container;
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ShellDelegate* shell_delegate = Shell::GetInstance()->delegate();
    if (sender == footer()->content()) {
      owner()->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
    } else if (sender == spoken_feedback_view_) {
      shell_delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
    } else if (sender == high_contrast_view_) {
      shell_delegate->ToggleHighContrast();
    } else if (sender == screen_magnifier_view_) {
      bool screen_magnifier_enabled =
          shell_delegate->GetMagnifierType() == ash::MAGNIFIER_FULL;
      shell_delegate->SetMagnifier(
          screen_magnifier_enabled ? ash::MAGNIFIER_OFF : ash::MAGNIFIER_FULL);
    }
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    SystemTrayDelegate* tray_delegate =
        Shell::GetInstance()->system_tray_delegate();
    if (sender == help_view_)
      tray_delegate->ShowAccessibilityHelp();
  }

  views::View* spoken_feedback_view_;
  views::View* high_contrast_view_;
  views::View* screen_magnifier_view_;;
  views::View* help_view_;
  user::LoginStatus login_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityDetailedView);
};

class AccessibilityPopupView : public TrayNotificationView {
 public:
  AccessibilityPopupView(SystemTrayItem* owner)
      : TrayNotificationView(owner, IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK) {
    InitView(GetLabel());
  }

 private:
  views::Label* GetLabel() {
    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_SPOKEN_FEEDBACK_ENABLED_BUBBLE));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return label;
  }

  DISALLOW_COPY_AND_ASSIGN(AccessibilityPopupView);
};

}  // namespace tray


TrayAccessibility::TrayAccessibility(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_ACCESSIBILITY),
      default_(NULL),
      detailed_(NULL),
      request_popup_view_(false),
      login_(GetCurrentLoginStatus()),
      previous_accessibility_state_(GetAccessibilityState()) {
  DCHECK(Shell::GetInstance()->delegate());
  DCHECK(system_tray);
  Shell::GetInstance()->system_tray_notifier()->AddAccessibilityObserver(this);
}

TrayAccessibility::~TrayAccessibility() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveAccessibilityObserver(this);
}

bool TrayAccessibility::GetInitialVisibility() {
  // Shows accessibility icon if any accessibility feature is enabled.
  // Otherwise, doen't show it.
  return GetAccessibilityState() != A11Y_NONE;
}

views::View* TrayAccessibility::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);

  login_ = status;

  // Shows accessibility menu if:
  // - on login screen (not logged in);
  // - "Enable accessibility menu" on chrome://settings is checked;
  // - or any of accessibility features is enabled
  // Otherwise, not shows it.
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (login_ != user::LOGGED_IN_NONE &&
      !delegate->ShouldAlwaysShowAccessibilityMenu() &&
      GetAccessibilityState() == A11Y_NONE)
    return NULL;

  CHECK(default_ == NULL);
  default_ = new tray::DefaultAccessibilityView(this);

  return default_;
}

views::View* TrayAccessibility::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);

  login_ = status;

  if (request_popup_view_) {
    detailed_ = new tray::AccessibilityPopupView(this);
    request_popup_view_ = false;
  } else {
    detailed_ = new tray::AccessibilityDetailedView(this, status);
  }

  return detailed_;
}

void TrayAccessibility::DestroyDefaultView() {
  default_ = NULL;
}

void TrayAccessibility::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayAccessibility::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  login_ = status;

  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());
}

void TrayAccessibility::OnAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  uint32 accessibility_state = GetAccessibilityState();
  if ((notify == ash::A11Y_NOTIFICATION_SHOW)&&
      !(previous_accessibility_state_ & A11Y_SPOKEN_FEEDBACK) &&
      (accessibility_state & A11Y_SPOKEN_FEEDBACK)) {
    // Shows popup if |notify| is true and the spoken feedback is being enabled.
    request_popup_view_ = true;
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  } else if (detailed_) {
    detailed_->GetWidget()->Close();
  }

  previous_accessibility_state_ = accessibility_state;
}

}  // namespace internal
}  // namespace ash
