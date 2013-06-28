// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_display.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

using message_center::Notification;

namespace ash {
namespace internal {
namespace {

static const char kDisplayNotificationId[] = "chrome://settings/display";

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

base::string16 GetDisplayName(int64 display_id) {
  return UTF8ToUTF16(GetDisplayManager()->GetDisplayNameForId(display_id));
}

// Returns 1-line information for the specified display, like
// "InternalDisplay: 1280x750"
base::string16 GetDisplayInfoLine(int64 display_id) {
  const DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display_id);

  base::string16 size_text = UTF8ToUTF16(
      display_info.size_in_pixel().ToString());
  base::string16 display_data;
  if (display_info.has_overscan()) {
    display_data = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION,
        size_text,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION_OVERSCAN));
  } else {
    display_data = size_text;
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_SINGLE_DISPLAY,
      GetDisplayName(display_id),
      display_data);
}

base::string16 GetAllDisplayInfo() {
  DisplayManager* display_manager = GetDisplayManager();
  std::vector<base::string16> lines;
  int64 internal_id = gfx::Display::kInvalidDisplayID;
  // Make sure to show the internal display first.
  if (display_manager->HasInternalDisplay() &&
      display_manager->IsInternalDisplayId(
          display_manager->first_display_id())) {
    internal_id = display_manager->first_display_id();
    lines.push_back(GetDisplayInfoLine(internal_id));
  }

  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64 id = display_manager->GetDisplayAt(i)->id();
    if (id == internal_id)
      continue;
    lines.push_back(GetDisplayInfoLine(id));
  }

  if (display_manager->IsMirrored()) {
    lines.push_back(GetDisplayInfoLine(
        display_manager->mirrored_display().id()));
  }

  return JoinString(lines, '\n');
}

// Returns the name of the currently connected external display.
base::string16 GetExternalDisplayName() {
  DisplayManager* display_manager = GetDisplayManager();
  int64 external_id = display_manager->mirrored_display().id();

  if (external_id == gfx::Display::kInvalidDisplayID) {
    int64 internal_display_id = gfx::Display::InternalDisplayId();
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
      int64 id = display_manager->GetDisplayAt(i)->id();
      if (id != internal_display_id) {
        external_id = id;
        break;
      }
    }
  }

  if (external_id == gfx::Display::kInvalidDisplayID)
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  // The external display name may have an annotation of "(width x height)" in
  // case that the display is rotated or its resolution is changed.
  base::string16 name = GetDisplayName(external_id);
  const DisplayInfo& display_info =
      display_manager->GetDisplayInfo(external_id);
  if (display_info.rotation() != gfx::Display::ROTATE_0 ||
      display_info.ui_scale() != 1.0f ||
      !display_info.overscan_insets_in_dip().empty()) {
    name += UTF8ToUTF16(
        " (" + display_info.size_in_pixel().ToString() + ")");
  } else if (display_info.overscan_insets_in_dip().empty() &&
             display_info.has_overscan()) {
    name += UTF8ToUTF16(" (") + l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION_OVERSCAN) + UTF8ToUTF16(")");
  }

  return name;
}

base::string16 GetTrayDisplayMessage() {
  DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->GetNumDisplays() > 1) {
    if (GetDisplayManager()->HasInternalDisplay()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetExternalDisplayName());
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL);
  }

  if (display_manager->IsMirrored()) {
    if (GetDisplayManager()->HasInternalDisplay()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING, GetExternalDisplayName());
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING_NO_INTERNAL);
  }

  int64 first_id = display_manager->first_display_id();
  if (display_manager->HasInternalDisplay() &&
      !display_manager->IsInternalDisplayId(first_id)) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED);
  }

  return base::string16();
}

void OpenSettings(user::LoginStatus login_status) {
  if (login_status == ash::user::LOGGED_IN_USER ||
      login_status == ash::user::LOGGED_IN_OWNER ||
      login_status == ash::user::LOGGED_IN_GUEST) {
    ash::Shell::GetInstance()->system_tray_delegate()->ShowDisplaySettings();
  }
}

class DisplayNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  DisplayNotificationDelegate(user::LoginStatus login_status)
      : login_status_(login_status) {}

  // message_center::NotificationDelegate overrides:
  virtual void Display() OVERRIDE {}
  virtual void Error() OVERRIDE {}
  virtual void Close(bool by_user) OVERRIDE {}
  virtual bool HasClickedListener() OVERRIDE { return true; }
  virtual void Click() OVERRIDE { OpenSettings(login_status_); }

 protected:
  virtual ~DisplayNotificationDelegate() {}

 private:
  user::LoginStatus login_status_;

  DISALLOW_COPY_AND_ASSIGN(DisplayNotificationDelegate);
};

void UpdateDisplayNotification(const base::string16& message) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(
      kDisplayNotificationId, false /* by_user */);

  if (message.empty())
    return;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kDisplayNotificationId,
      message,
      GetAllDisplayInfo(),
      bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY),
      base::string16(),  // display_source
      "",  // extension_id
      message_center::RichNotificationData(),
      new DisplayNotificationDelegate(
          Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus())));
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

}  // namespace

class DisplayView : public ash::internal::ActionableView {
 public:
  explicit DisplayView(user::LoginStatus login_status)
      : login_status_(login_status) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal,
        ash::kTrayPopupPaddingHorizontal, 0,
        ash::kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    image_ =
        new ash::internal::FixedSizedImageView(0, ash::kTrayPopupItemHeight);
    image_->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY).ToImageSkia());
    AddChildView(image_);

    label_ = new views::Label();
    label_->SetMultiLine(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(label_);
    Update();
  }

  virtual ~DisplayView() {}

  void Update() {
    base::string16 message = GetTrayDisplayMessage();
    if (message.empty() && ShouldShowFirstDisplayInfo())
      message = GetDisplayInfoLine(GetDisplayManager()->first_display_id());
    SetVisible(!message.empty());
    label_->SetText(message);
  }

  views::Label* label() { return label_; }

  // Overridden from views::View.
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE {
    base::string16 tray_message = GetTrayDisplayMessage();
    base::string16 display_message = GetAllDisplayInfo();
    if (tray_message.empty() && display_message.empty())
      return false;

    *tooltip = tray_message + ASCIIToUTF16("\n") + display_message;
    return true;
  }

 private:
  bool ShouldShowFirstDisplayInfo() const {
    const DisplayInfo& display_info = GetDisplayManager()->GetDisplayInfo(
        GetDisplayManager()->first_display_id());
    return display_info.rotation() != gfx::Display::ROTATE_0 ||
        display_info.ui_scale() != 1.0f ||
        !display_info.overscan_insets_in_dip().empty() ||
        display_info.has_overscan();
  }

  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    OpenSettings(login_status_);
    return true;
  }

  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    int label_max_width = bounds().width() - kTrayPopupPaddingHorizontal * 2 -
        kTrayPopupPaddingBetweenItems - image_->GetPreferredSize().width();
    label_->SizeToFit(label_max_width);
    PreferredSizeChanged();
  }

  user::LoginStatus login_status_;
  views::ImageView* image_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DisplayView);
};

class DisplayNotificationView : public TrayNotificationView {
 public:
  DisplayNotificationView(user::LoginStatus login_status,
                          TrayDisplay* tray_item,
                          const base::string16& message)
      : TrayNotificationView(tray_item, IDR_AURA_UBER_TRAY_DISPLAY),
        login_status_(login_status) {
    StartAutoCloseTimer(kTrayPopupAutoCloseDelayForTextInSeconds);
    Update(message);
  }

  virtual ~DisplayNotificationView() {}

  void Update(const base::string16& message) {
    if (message.empty()) {
      owner()->HideNotificationView();
    } else {
      views::Label* label = new views::Label(message);
      label->SetMultiLine(true);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      UpdateView(label);
      RestartAutoCloseTimer();
    }
  }

  // Overridden from TrayNotificationView:
  virtual void OnClickAction() OVERRIDE {
    OpenSettings(login_status_);
  }

 private:
  user::LoginStatus login_status_;

  DISALLOW_COPY_AND_ASSIGN(DisplayNotificationView);
};

TrayDisplay::TrayDisplay(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL) {
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

TrayDisplay::~TrayDisplay() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

base::string16 TrayDisplay::GetDisplayMessageForNotification() {
  DisplayManager* display_manager = GetDisplayManager();
  DisplayInfoMap old_info;
  old_info.swap(display_info_);
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64 id = display_manager->GetDisplayAt(i)->id();
    display_info_[id] = display_manager->GetDisplayInfo(id);
  }

  // Display is added or removed. Use the same message as the one in
  // the system tray.
  if (display_info_.size() != old_info.size())
    return GetTrayDisplayMessage();

  for (DisplayInfoMap::const_iterator iter = display_info_.begin();
       iter != display_info_.end(); ++iter) {
    DisplayInfoMap::const_iterator old_iter = old_info.find(iter->first);
    // A display is removed and added at the same time. It won't happen
    // in the actual environment, but falls back to the system tray's
    // message just in case.
    if (old_iter == old_info.end())
      return GetTrayDisplayMessage();

    if (iter->second.ui_scale() != old_iter->second.ui_scale()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
          GetDisplayName(iter->first),
          UTF8ToUTF16(iter->second.size_in_pixel().ToString()));
    }
    if (iter->second.rotation() != old_iter->second.rotation()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetDisplayName(iter->first));
    }
  }

  // Found nothing special
  return base::string16();
}

views::View* TrayDisplay::CreateDefaultView(user::LoginStatus status) {
  DCHECK(default_ == NULL);
  default_ = new DisplayView(status);
  return default_;
}

void TrayDisplay::DestroyDefaultView() {
  default_ = NULL;
}

void TrayDisplay::OnDisplayConfigurationChanged() {
  if (!Shell::GetInstance()->system_tray_delegate()->
          ShouldShowDisplayNotification()) {
    return;
  }

  UpdateDisplayNotification(GetDisplayMessageForNotification());
}

base::string16 TrayDisplay::GetDefaultViewMessage() {
  if (!default_ || !default_->visible())
    return base::string16();

  return static_cast<DisplayView*>(default_)->label()->text();
}

base::string16 TrayDisplay::GetNotificationMessage() {
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetNotifications();
  for (message_center::NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == kDisplayNotificationId)
      return (*iter)->title();
  }

  return base::string16();
}

void TrayDisplay::CloseNotificationForTest() {
  message_center::MessageCenter::Get()->RemoveNotification(
      kDisplayNotificationId, false);
}

}  // namespace internal
}  // namespace ash
