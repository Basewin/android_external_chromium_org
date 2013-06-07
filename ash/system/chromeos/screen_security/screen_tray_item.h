// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SCREEN_TRAY_ITEM_H_
#define ASH_SYSTEM_CHROMEOS_SCREEN_TRAY_ITEM_H_

#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace views {
class View;
}

namespace ash {
namespace internal {

class ScreenTrayItem;

namespace tray {

class ScreenTrayView : public TrayItemView {
 public:
  ScreenTrayView(ScreenTrayItem* screen_tray_item, int icon_id);
  virtual ~ScreenTrayView();

  void Update();

 private:
  ScreenTrayItem* screen_tray_item_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTrayView);
};

class ScreenStatusView : public views::View,
                         public views::ButtonListener {
 public:
  enum ViewType {
      VIEW_DEFAULT,
      VIEW_NOTIFICATION
  };

  ScreenStatusView(ScreenTrayItem* screen_tray_item,
                   ViewType view_type,
                   int icon_id,
                   const base::string16& label_text,
                   const base::string16& stop_button_text);
  virtual ~ScreenStatusView();

  // Overridden from views::View.
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  void CreateItems();
  void Update();

 private:
  ScreenTrayItem* screen_tray_item_;
  views::ImageView* icon_;
  views::Label* label_;
  TrayPopupLabelButton* stop_button_;
  ViewType view_type_;
  int icon_id_;
  base::string16 label_text_;
  base::string16 stop_button_text_;

  DISALLOW_COPY_AND_ASSIGN(ScreenStatusView);
};

class ScreenNotificationView : public TrayNotificationView {
 public:
  ScreenNotificationView(ScreenTrayItem* screen_tray_item,
                         int icon_id,
                         const base::string16& label_text,
                         const base::string16& stop_button_text);
  virtual ~ScreenNotificationView();

  void Update();

 private:
  ScreenTrayItem* screen_tray_item_;
  ScreenStatusView* screen_status_view_;

  DISALLOW_COPY_AND_ASSIGN(ScreenNotificationView);
};

}  // namespace tray


// The base tray item for screen capture and screen sharing. The
// Start method brings up a notification and a tray item, and the user
// can stop the screen capture/sharing by pressing the stop button.
class ASH_EXPORT ScreenTrayItem : public SystemTrayItem {
 public:
  explicit ScreenTrayItem(SystemTray* system_tray);
  virtual ~ScreenTrayItem();

  tray::ScreenTrayView* tray_view() { return tray_view_; }
  void set_tray_view(tray::ScreenTrayView* tray_view) {
    tray_view_ = tray_view;
  }

  tray::ScreenStatusView* default_view() { return default_view_; }
  void set_default_view(tray::ScreenStatusView* default_view) {
    default_view_ = default_view;
  }

  tray::ScreenNotificationView* notification_view() {
    return notification_view_;
  }
  void set_notification_view(tray::ScreenNotificationView* notification_view) {
    notification_view_ = notification_view;
  }

  bool is_started() const { return is_started_; }
  void set_is_started(bool is_started) { is_started_ = is_started; }

  void Update();
  void Start(const base::Closure& stop_callback);
  void Stop();

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE = 0;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE = 0;
  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE = 0;

  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyNotificationView() OVERRIDE;

 private:
  tray::ScreenTrayView* tray_view_;
  tray::ScreenStatusView* default_view_;
  tray::ScreenNotificationView* notification_view_;
  bool is_started_;
  base::Closure stop_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScreenTrayItem);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SCREEN_TRAY_ITEM_H_
