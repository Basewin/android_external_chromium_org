// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
#define CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/browser/download/drag_download_file.h"
#include "ui/base/dragdrop/download_file_interface.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
class FileStream;
}

namespace content {

// Parse the download metadata set in DataTransfer.setData. The metadata
// consists of a set of the following values separated by ":"
// * MIME type
// * File name
// * URL
// If the file name contains special characters, they need to be escaped
// appropriately.
// For example, we can have
//   text/plain:example.txt:http://example.com/example.txt
bool ParseDownloadMetadata(const base::string16& metadata,
                           base::string16* mime_type,
                           base::FilePath* file_name,
                           GURL* url);

// Create a new file at the specified path. If the file already exists, try to
// insert the sequential unifier to produce a new file, like foo-01.txt.
// Return a FileStream if successful.
// |net_log| is a NetLog for the stream.
CONTENT_EXPORT net::FileStream* CreateFileStreamForDrop(
    base::FilePath* file_path, net::NetLog* net_log);

// Implementation of DownloadFileObserver to finalize the download process.
class PromiseFileFinalizer : public ui::DownloadFileObserver {
 public:
  explicit PromiseFileFinalizer(DragDownloadFile* drag_file_downloader);

  // DownloadFileObserver methods.
  virtual void OnDownloadCompleted(const base::FilePath& file_path) OVERRIDE;
  virtual void OnDownloadAborted() OVERRIDE;

 protected:
  virtual ~PromiseFileFinalizer();

 private:
  void Cleanup();

  scoped_refptr<DragDownloadFile> drag_file_downloader_;

  DISALLOW_COPY_AND_ASSIGN(PromiseFileFinalizer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
