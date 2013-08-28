// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_MENU_MODEL_H_
#define CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_MENU_MODEL_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/base/models/simple_menu_model.h"

namespace gfx {
class Image;
}

class StatusIconMenuModelTest;

// StatusIconMenuModel contains the state of the SimpleMenuModel as well as that
// of its delegate. This is done so that we can easily identify when the menu
// model state has changed and can tell the status icon to update the menu. This
// is necessary some platforms which do not notify us before showing the menu
// (like Ubuntu Unity).
class StatusIconMenuModel
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate,
      public base::SupportsWeakPtr<StatusIconMenuModel> {
 public:
  class Delegate {
   public:
    // Notifies the delegate that the item with the specified command id was
    // visually highlighted within the menu.
    virtual void CommandIdHighlighted(int command_id);

    // Performs the action associates with the specified command id.
    // The passed |event_flags| are the flags from the event which issued this
    // command and they can be examined to find modifier keys.
    virtual void ExecuteCommand(int command_id, int event_flags) = 0;

   protected:
    virtual ~Delegate() {}
  };

  class Observer {
   public:
    // Invoked when the menu model has changed.
    virtual void OnMenuStateChanged() {}

   protected:
    virtual ~Observer() {}
  };

  // The Delegate can be NULL.
  explicit StatusIconMenuModel(Delegate* delegate);
  virtual ~StatusIconMenuModel();

  // Methods for seting the state of specific command ids.
  void SetCommandIdChecked(int command_id, bool checked);
  void SetCommandIdEnabled(int command_id, bool enabled);
  void SetCommandIdVisible(int command_id, bool visible);

  // Sets the accelerator for the specified command id.
  void SetAcceleratorForCommandId(
      int command_id, const ui::Accelerator* accelerator);

  // Calling any of these "change" methods will mark the menu item as "dynamic"
  // (see menu_model.h:IsItemDynamicAt) which many platforms take as a cue to
  // refresh the label, sublabel and icon of the menu item each time the menu is
  // shown.
  void ChangeLabelForCommandId(int command_id, const base::string16& label);
  void ChangeSublabelForCommandId(
      int command_id, const base::string16& sublabel);
  void ChangeIconForCommandId(int command_id, const gfx::Image& icon);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual base::string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual base::string16 GetSublabelForCommandId(int command_id) const OVERRIDE;
  virtual bool GetIconForCommandId(int command_id, gfx::Image* icon) const
      OVERRIDE;

 protected:
  // Overriden from ui::SimpleMenuModel:
  virtual void MenuItemsChanged() OVERRIDE;

  void NotifyMenuStateChanged();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() { return delegate_; }

 private:
  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual void CommandIdHighlighted(int command_id) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  struct ItemState;

  // Map the properties to the command id (used as key).
  typedef std::map<int, ItemState> ItemStateMap;

  ItemStateMap item_states_;

  ObserverList<Observer> observer_list_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconMenuModel);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_STATUS_ICON_MENU_MODEL_H_
