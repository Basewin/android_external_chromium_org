// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_gtk.h"

#include <fcntl.h>
#include <gtk/gtkpagesetupunixdialog.h>
#include <gtk/gtkprintjob.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "printing/metafile.h"
#include "printing/print_settings_initializer_gtk.h"

// static
printing::PrintDialogGtkInterface* PrintDialogGtk::CreatePrintDialog(
    PrintingContextCairo* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new PrintDialogGtk(context);
}

PrintDialogGtk::PrintDialogGtk(PrintingContextCairo* context)
    : callback_(NULL),
      context_(context),
      dialog_(NULL),
      gtk_settings_(NULL),
      page_setup_(NULL),
      printer_(NULL) {
  GtkWindow* parent = BrowserList::GetLastActive()->window()->GetNativeHandle();

  // TODO(estade): We need a window title here.
  dialog_ = gtk_print_unix_dialog_new(NULL, parent);
}

PrintDialogGtk::~PrintDialogGtk() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gtk_widget_destroy(dialog_);
  dialog_ = NULL;
  if (gtk_settings_) {
    g_object_unref(gtk_settings_);
    gtk_settings_ = NULL;
  }
  if (page_setup_) {
    g_object_unref(page_setup_);
    page_setup_ = NULL;
  }
  if (printer_) {
    g_object_unref(printer_);
    printer_ = NULL;
  }
}

void PrintDialogGtk::ShowDialog(
    PrintingContextCairo::PrintSettingsCallback* callback) {
  DCHECK(!save_document_event_.get());

  callback_ = callback;

  // Set modal so user cannot focus the same tab and press print again.
  gtk_window_set_modal(GTK_WINDOW(dialog_), TRUE);

  // Since we only generate PDF, only show printers that support PDF.
  // TODO(thestig) Add more capabilities to support?
  GtkPrintCapabilities cap = static_cast<GtkPrintCapabilities>(
      GTK_PRINT_CAPABILITY_GENERATE_PDF |
      GTK_PRINT_CAPABILITY_PAGE_SET |
      GTK_PRINT_CAPABILITY_COPIES |
      GTK_PRINT_CAPABILITY_COLLATE |
      GTK_PRINT_CAPABILITY_REVERSE);
  gtk_print_unix_dialog_set_manual_capabilities(GTK_PRINT_UNIX_DIALOG(dialog_),
                                                cap);
#if GTK_CHECK_VERSION(2, 18, 0)
  gtk_print_unix_dialog_set_embed_page_setup(GTK_PRINT_UNIX_DIALOG(dialog_),
                                             TRUE);
#endif
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_widget_show(dialog_);
}

void PrintDialogGtk::PrintDocument(const printing::Metafile* metafile,
                                   const string16& document_name) {
  // This runs on the print worker thread, does not block the UI thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The document printing tasks can outlive the PrintingContext that created
  // this dialog.
  AddRef();
  DCHECK(!save_document_event_.get());
  save_document_event_.reset(new base::WaitableEvent(false, false));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &PrintDialogGtk::SaveDocumentToDisk,
                        metafile,
                        document_name));
  // Wait for SaveDocumentToDisk() to finish.
  save_document_event_->Wait();
}

void PrintDialogGtk::AddRefToDialog() {
  AddRef();
}

void PrintDialogGtk::ReleaseDialog() {
  Release();
}

void PrintDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  gtk_widget_hide(dialog_);

  switch (response_id) {
    case GTK_RESPONSE_OK: {
      gtk_settings_ = gtk_print_unix_dialog_get_settings(
          GTK_PRINT_UNIX_DIALOG(dialog_));
      printer_ = gtk_print_unix_dialog_get_selected_printer(
          GTK_PRINT_UNIX_DIALOG(dialog_));
      g_object_ref(printer_);
      page_setup_ = gtk_print_unix_dialog_get_page_setup(
          GTK_PRINT_UNIX_DIALOG(dialog_));
      g_object_ref(page_setup_);

      // Handle page ranges.
      printing::PageRanges ranges_vector;
      gint num_ranges;
      GtkPageRange* gtk_range =
          gtk_print_settings_get_page_ranges(gtk_settings_, &num_ranges);
      if (gtk_range) {
        for (int i = 0; i < num_ranges; ++i) {
          printing::PageRange range;
          range.from = gtk_range[i].start;
          range.to = gtk_range[i].end;
          ranges_vector.push_back(range);
        }
        g_free(gtk_range);
      }

      printing::PrintSettings settings;
      printing::PrintSettingsInitializerGtk::InitPrintSettings(
          gtk_settings_, page_setup_, ranges_vector, false, &settings);
      context_->InitWithSettings(settings);
      callback_->Run(PrintingContextCairo::OK);
      callback_ = NULL;
      return;
    }
    case GTK_RESPONSE_DELETE_EVENT:  // Fall through.
    case GTK_RESPONSE_CANCEL: {
      callback_->Run(PrintingContextCairo::CANCEL);
      callback_ = NULL;
      return;
    }
    case GTK_RESPONSE_APPLY:
    default: {
      NOTREACHED();
    }
  }
}

void PrintDialogGtk::SaveDocumentToDisk(const printing::Metafile* metafile,
                                        const string16& document_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool error = false;
  if (!file_util::CreateTemporaryFile(&path_to_pdf_)) {
    LOG(ERROR) << "Creating temporary file failed";
    error = true;
  }

  if (!error && !metafile->SaveTo(path_to_pdf_)) {
    LOG(ERROR) << "Saving metafile failed";
    file_util::Delete(path_to_pdf_, false);
    error = true;
  }

  // Done saving, let PrintDialogGtk::PrintDocument() continue.
  save_document_event_->Signal();

  if (error) {
    // Matches AddRef() in PrintDocument();
    Release();
  } else {
    // No errors, continue printing.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &PrintDialogGtk::SendDocumentToPrinter,
                          document_name));
  }
}

void PrintDialogGtk::SendDocumentToPrinter(const string16& document_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GtkPrintJob* print_job = gtk_print_job_new(
      UTF16ToUTF8(document_name).c_str(),
      printer_,
      gtk_settings_,
      page_setup_);
  gtk_print_job_set_source_file(print_job, path_to_pdf_.value().c_str(), NULL);
  gtk_print_job_send(print_job, OnJobCompletedThunk, this, NULL);
}

// static
void PrintDialogGtk::OnJobCompletedThunk(GtkPrintJob* print_job,
                                         gpointer user_data,
                                         GError* error) {
  static_cast<PrintDialogGtk*>(user_data)->OnJobCompleted(print_job, error);
}

void PrintDialogGtk::OnJobCompleted(GtkPrintJob* print_job, GError* error) {
  if (error)
    LOG(ERROR) << "Printing failed: " << error->message;
  if (print_job)
    g_object_unref(print_job);
  base::FileUtilProxy::Delete(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      path_to_pdf_,
      false,
      NULL);
  // Printing finished. Matches AddRef() in PrintDocument();
  Release();
}
