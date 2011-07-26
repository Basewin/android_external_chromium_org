// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/file_manager_util.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/media/media_player.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/browser/browser_thread.h"
#include "content/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_util.h"

#define FILEBROWSER_URL(PATH) \
    ("chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/" PATH)
// This is the "well known" url for the file manager extension from
// browser/resources/file_manager.  In the future we may provide a way to swap
// out this file manager for an aftermarket part, but not yet.
const char kFileBrowserExtensionUrl[] = FILEBROWSER_URL("");
const char kBaseFileBrowserUrl[] = FILEBROWSER_URL("main.html");
const char kMediaPlayerUrl[] = FILEBROWSER_URL("mediaplayer.html");
const char kMediaPlayerPlaylistUrl[] = FILEBROWSER_URL("playlist.html");
#undef FILEBROWSER_URL

// List of file extension we can open in tab.
const char* kBrowserSupportedExtensions[] = {
    ".jpg", ".jpeg", ".png", ".webp", ".gif", ".pdf", ".txt", ".html", ".htm"
};
// List of file extension that can be handled with the media player.
const char* kAVExtensions[] = {
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
    ".3gp", ".avi", ".mp3", ".mp4", ".m4v", ".mov", ".m4a",
#endif
    ".flac", ".ogm", ".ogv", ".ogx", ".ogg", ".oga", ".wav", ".webm",
/* TODO(zelidrag): Add unsupported ones as we enable them:
    ".mkv", ".divx", ".xvid", ".wmv", ".asf", ".mpeg", ".mpg",
    ".wma", ".aiff",
*/
};

bool IsSupportedBrowserExtension(const char* ext) {
  for (size_t i = 0; i < arraysize(kBrowserSupportedExtensions); i++) {
    if (base::strcasecmp(ext, kBrowserSupportedExtensions[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool IsSupportedAVExtension(const char* ext) {
  for (size_t i = 0; i < arraysize(kAVExtensions); i++) {
    if (base::strcasecmp(ext, kAVExtensions[i]) == 0) {
      return true;
    }
  }
  return false;
}

// static
GURL FileManagerUtil::GetFileBrowserExtensionUrl() {
  return GURL(kFileBrowserExtensionUrl);
}

// static
GURL FileManagerUtil::GetFileBrowserUrl() {
  return GURL(kBaseFileBrowserUrl);
}

// static
GURL FileManagerUtil::GetMediaPlayerUrl() {
  return GURL(kMediaPlayerUrl);
}

// static
GURL FileManagerUtil::GetMediaPlayerPlaylistUrl() {
  return GURL(kMediaPlayerPlaylistUrl);
}

// static
bool FileManagerUtil::ConvertFileToFileSystemUrl(
    Profile* profile, const FilePath& full_file_path, const GURL& origin_url,
    GURL* url) {
  FilePath virtual_path;
  if (!ConvertFileToRelativeFileSystemPath(profile, full_file_path,
                                           &virtual_path)) {
    return false;
  }

  GURL base_url = fileapi::GetFileSystemRootURI(origin_url,
      fileapi::kFileSystemTypeExternal);
  *url = GURL(base_url.spec() + virtual_path.value());
  return true;
}

// static
bool FileManagerUtil::ConvertFileToRelativeFileSystemPath(
    Profile* profile, const FilePath& full_file_path, FilePath* virtual_path) {
  fileapi::FileSystemPathManager* path_manager =
      profile->GetFileSystemContext()->path_manager();
  fileapi::ExternalFileSystemMountPointProvider* provider =
      path_manager->external_provider();
  if (!provider)
    return false;

  // Find if this file path is managed by the external provider.
  if (!provider->GetVirtualPath(full_file_path, virtual_path))
    return false;

  return true;
}

// static
GURL FileManagerUtil::GetFileBrowserUrlWithParams(
    SelectFileDialog::Type type,
    const string16& title,
    const FilePath& default_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension) {
  std::string json = GetArgumentsJson(type, title, default_path, file_types,
                                      file_type_index, default_extension);
  return GURL(FileManagerUtil::GetFileBrowserUrl().spec() + "?" +
              EscapeUrlEncodedData(json, false));

}

// static
void FileManagerUtil::ShowFullTabUrl(Profile*,
                                     const FilePath& default_path) {
  std::string json = GetArgumentsJson(SelectFileDialog::SELECT_NONE, string16(),
      default_path, NULL, 0, FilePath::StringType());
  GURL url(std::string(kBaseFileBrowserUrl) + "?" +
           EscapeUrlEncodedData(json, false));
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;

  UserMetrics::RecordAction(UserMetricsAction("ShowFileBrowserFullTab"));
  browser->ShowSingletonTab(GURL(url));
}

void FileManagerUtil::ViewItem(const FilePath& full_path, bool enqueue) {
  std::string ext = full_path.Extension();
  // For things supported natively by the browser, we should open it
  // in a tab.
  if (IsSupportedBrowserExtension(ext.data())) {
    std::string path;
    path = "file://";
    path.append(EscapeUrlEncodedData(full_path.value(), false));
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      bool result = BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableFunction(&ViewItem, full_path, enqueue));
      DCHECK(result);
      return;
    }
    Browser* browser = BrowserList::GetLastActive();
    if (browser)
      browser->AddSelectedTabWithURL(GURL(path), PageTransition::LINK);
    return;
  }
  if (IsSupportedAVExtension(ext.data())) {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      return;
    MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
    if (enqueue)
      mediaplayer->EnqueueMediaFile(browser->profile(), full_path, NULL);
    else
      mediaplayer->ForcePlayMediaFile(browser->profile(), full_path, NULL);
    return;
  }

  // Unknown file type. Show an error message.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &platform_util::SimpleErrorBox,
          static_cast<gfx::NativeWindow>(NULL),
          l10n_util::GetStringUTF16(IDS_FILEBROWSER_ERROR_TITLE),
          l10n_util::GetStringFUTF16(IDS_FILEBROWSER_ERROR_UNKNOWN_FILE_TYPE,
                                     UTF8ToUTF16(full_path.BaseName().value()))
          ));
}

// static
std::string FileManagerUtil::GetArgumentsJson(
    SelectFileDialog::Type type,
    const string16& title,
    const FilePath& default_path,
    const SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension) {
  DictionaryValue arg_value;
  arg_value.SetString("type", GetDialogTypeAsString(type));
  arg_value.SetString("title", title);
  // TODO(zelidrag): Convert local system path into virtual path for File API.
  arg_value.SetString("defaultPath", default_path.value());
  arg_value.SetString("defaultExtension", default_extension);


  if (file_types) {
    ListValue* types_list = new ListValue();
    for (size_t i = 0; i < file_types->extensions.size(); ++i) {
      ListValue* extensions_list = new ListValue();
      for (size_t j = 0; j < file_types->extensions[i].size(); ++j) {
        extensions_list->Set(
            i, Value::CreateStringValue(file_types->extensions[i][j]));
      }

      DictionaryValue* dict = new DictionaryValue();
      dict->Set("extensions", extensions_list);

      if (i < file_types->extension_description_overrides.size()) {
        string16 desc = file_types->extension_description_overrides[i];
        dict->SetString("description", desc);
      }

      dict->SetBoolean("selected",
                       (static_cast<size_t>(file_type_index) == i));

      types_list->Set(i, dict);
    }
    arg_value.Set("typeList", types_list);
  }

  std::string rv;
  base::JSONWriter::Write(&arg_value, false, &rv);

  return rv;
}

// static
std::string FileManagerUtil::GetDialogTypeAsString(
    SelectFileDialog::Type dialog_type) {
  std::string type_str;
  switch (dialog_type) {
    case SelectFileDialog::SELECT_NONE:
      type_str = "full-page";
      break;

    case SelectFileDialog::SELECT_FOLDER:
      type_str = "folder";
      break;

    case SelectFileDialog::SELECT_SAVEAS_FILE:
      type_str = "saveas-file";
      break;

    case SelectFileDialog::SELECT_OPEN_FILE:
      type_str = "open-file";
      break;

    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      type_str = "open-multi-file";
      break;

    default:
      NOTREACHED();
  }

  return type_str;
}
