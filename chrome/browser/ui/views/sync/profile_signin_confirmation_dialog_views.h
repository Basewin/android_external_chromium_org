// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace views {
class StyledLabel;
}

// A tab-modal dialog to allow a user signing in with a managed account
// to create a new Chrome profile.
class ProfileSigninConfirmationDialogViews : public views::DialogDelegateView,
                                             public views::LinkListener,
                                             public views::StyledLabelListener {
 public:
  // Create and show the dialog, which owns itself.
  static void ShowDialog(Browser* browser,
                         Profile* profile,
                         const std::string& username,
                         const base::Closure& cancel_signin,
                         const base::Closure& signin_with_new_profile,
                         const base::Closure& continue_signin);

 private:
  ProfileSigninConfirmationDialogViews(
      Browser* browser,
      Profile* profile,
      const std::string& username,
      const base::Closure& cancel_signin,
      const base::Closure& signin_with_new_profile,
      const base::Closure& continue_signin);
  virtual ~ProfileSigninConfirmationDialogViews();

  // views::DialogDelegateView:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual views::View* CreateExtraView() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual void OnClose() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::StyledLabelListener:
  virtual void StyledLabelLinkClicked(const ui::Range& range,
                                      int event_flags) OVERRIDE;

  // Shows the dialog and releases ownership of this object. It will
  // delete itself when the dialog is closed. If |prompt_for_new_profile|
  // is true, the dialog will offer to create a new profile before signin.
  void Show(bool prompt_for_new_profile);

  // Resets all the user response handling callbacks.
  void ResetCallbacks();

  // Weak ptr to label for dialog explanation text.
  views::StyledLabel* explanation_label_;

  // Weak ptr to parent view.
  Browser* browser_;

  // The profile being signed in.
  Profile* profile_;

  // The GAIA username being signed in.
  std::string username_;

  // Dialog button callbacks.
  base::Closure cancel_signin_;
  base::Closure signin_with_new_profile_;
  base::Closure continue_signin_;

  // Whether the user should be prompted to create a new profile.
  bool prompt_for_new_profile_;

  // The link to create a new profile.
  views::Link* link_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
