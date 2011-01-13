// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pk11_password_dialog.h"

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/crypto/pk11_blocking_password_delegate.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"

namespace {

class PK11BlockingDialogDelegate : public base::PK11BlockingPasswordDelegate {
 public:
  PK11BlockingDialogDelegate(browser::PK11PasswordReason reason,
                             const std::string& server)
      : event_(false, false),
        reason_(reason),
        server_(server),
        password_(),
        cancelled_(false) {
  }

  ~PK11BlockingDialogDelegate() {
    password_.replace(0, password_.size(), password_.size(), 0);
  }

  // base::PK11BlockingDialogDelegate implementation.
  virtual std::string RequestPassword(const std::string& slot_name, bool retry,
                                      bool* cancelled) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!event_.IsSignaled());
    event_.Reset();
    if (BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            NewRunnableMethod(this, &PK11BlockingDialogDelegate::ShowDialog,
                              slot_name, retry))) {
      event_.Wait();
    }
    *cancelled = cancelled_;
    return password_;
  }

 private:
  void ShowDialog(const std::string& slot_name, bool retry) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ShowPK11PasswordDialog(
        slot_name, retry, reason_, server_,
        NewCallback(this, &PK11BlockingDialogDelegate::GotPassword));
  }
  void GotPassword(const char* password) {
    if (password)
      password_ = password;
    else
      cancelled_ = true;
    event_.Signal();
  }
  base::WaitableEvent event_;
  browser::PK11PasswordReason reason_;
  std::string server_;
  std::string password_;
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(PK11BlockingDialogDelegate);
};

// TODO(mattm): change into a constrained dialog.
class PK11PasswordDialog {
 public:
  PK11PasswordDialog(const std::string& slot_name,
                     bool retry,
                     browser::PK11PasswordReason reason,
                     const std::string& server,
                     browser::PK11PasswordCallback* callback);

  void Show();

 private:
  CHROMEGTK_CALLBACK_1(PK11PasswordDialog, void, OnResponse, int);
  CHROMEGTK_CALLBACK_0(PK11PasswordDialog, void, OnWindowDestroy);

  scoped_ptr<browser::PK11PasswordCallback> callback_;

  GtkWidget* dialog_;
  GtkWidget* password_entry_;

  DISALLOW_COPY_AND_ASSIGN(PK11PasswordDialog);
};

PK11PasswordDialog::PK11PasswordDialog(const std::string& slot_name,
                                       bool retry,
                                       browser::PK11PasswordReason reason,
                                       const std::string& server,
                                       browser::PK11PasswordCallback* callback)
    : callback_(callback) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_PK11_AUTH_DIALOG_TITLE).c_str(),
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      NULL);  // Populate the buttons later, for control over the OK button.
  gtk_dialog_add_button(GTK_DIALOG(dialog_),
                        GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
  GtkWidget* ok_button = gtk_util::AddButtonToDialog(
      dialog_,
      l10n_util::GetStringUTF8(IDS_PK11_AUTH_DIALOG_OK_BUTTON_LABEL).c_str(),
      GTK_STOCK_OK,
      GTK_RESPONSE_ACCEPT);
  GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);

  // Select an appropriate text for the reason.
  std::string text;
  const string16& server16 = UTF8ToUTF16(server);
  const string16& slot16 = UTF8ToUTF16(slot_name);
  switch (reason) {
    case browser::kPK11PasswordKeygen:
      text = l10n_util::GetStringFUTF8(IDS_PK11_AUTH_DIALOG_TEXT_KEYGEN,
                                       slot16, server16);
      break;
    case browser::kPK11PasswordCertEnrollment:
      text = l10n_util::GetStringFUTF8(
          IDS_PK11_AUTH_DIALOG_TEXT_CERT_ENROLLMENT, slot16, server16);
      break;
    case browser::kPK11PasswordClientAuth:
      text = l10n_util::GetStringFUTF8(IDS_PK11_AUTH_DIALOG_TEXT_CLIENT_AUTH,
                                       slot16, server16);
      break;
    case browser::kPK11PasswordCertImport:
      text = l10n_util::GetStringFUTF8(IDS_PK11_AUTH_DIALOG_TEXT_CERT_IMPORT,
                                       slot16);
      break;
    case browser::kPK11PasswordCertExport:
      text = l10n_util::GetStringFUTF8(IDS_PK11_AUTH_DIALOG_TEXT_CERT_EXPORT,
                                       slot16);
      break;
    default:
      NOTREACHED();
  }
  GtkWidget* label = gtk_label_new(text.c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_util::LeftAlignMisc(label);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), label,
                     FALSE, FALSE, 0);

  password_entry_ = gtk_entry_new();
  gtk_entry_set_activates_default(GTK_ENTRY(password_entry_), TRUE);
  gtk_entry_set_visibility(GTK_ENTRY(password_entry_), FALSE);

  GtkWidget* password_box = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  gtk_box_pack_start(GTK_BOX(password_box),
                     gtk_label_new(l10n_util::GetStringUTF8(
                         IDS_PK11_AUTH_DIALOG_PASSWORD_FIELD).c_str()),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(password_box), password_entry_,
                     TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), password_box,
                     FALSE, FALSE, 0);

  g_signal_connect(dialog_, "response",
                   G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "destroy",
                   G_CALLBACK(OnWindowDestroyThunk), this);
}

void PK11PasswordDialog::Show() {
  gtk_util::ShowDialog(dialog_);
}

void PK11PasswordDialog::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_ACCEPT)
    callback_->Run(gtk_entry_get_text(GTK_ENTRY(password_entry_)));
  else
    callback_->Run(static_cast<const char*>(NULL));

  // This will cause gtk to zero out the buffer.  (see
  // gtk_entry_buffer_normal_delete_text:
  // http://git.gnome.org/browse/gtk+/tree/gtk/gtkentrybuffer.c#n187)
  gtk_editable_delete_text(GTK_EDITABLE(password_entry_), 0, -1);
  gtk_widget_destroy(GTK_WIDGET(dialog_));
}

void PK11PasswordDialog::OnWindowDestroy(GtkWidget* widget) {
  delete this;
}

}  // namespace

// Every post-task we do blocks, so there's no need to ref-count.
DISABLE_RUNNABLE_METHOD_REFCOUNT(PK11BlockingDialogDelegate);

namespace browser {

void ShowPK11PasswordDialog(const std::string& slot_name,
                            bool retry,
                            PK11PasswordReason reason,
                            const std::string& server,
                            PK11PasswordCallback* callback) {
  (new PK11PasswordDialog(slot_name, retry, reason, server, callback))->Show();
}

base::PK11BlockingPasswordDelegate* NewPK11BlockingDialogDelegate(
    PK11PasswordReason reason,
    const std::string& server) {
  return new PK11BlockingDialogDelegate(reason, server);
}

}  // namespace browser
