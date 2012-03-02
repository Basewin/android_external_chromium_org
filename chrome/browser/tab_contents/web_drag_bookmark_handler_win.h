// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_WIN_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_WIN_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "content/browser/tab_contents/web_drag_dest_delegate.h"

class TabContentsWrapper;

// Chrome needs to intercept content drag events so it can dispatch them to the
// bookmarks and extensions system.
class WebDragBookmarkHandlerWin : public content::WebDragDestDelegate {
 public:
  WebDragBookmarkHandlerWin();
  virtual ~WebDragBookmarkHandlerWin();

  // Overridden from content::WebDragDestDelegate:
  virtual void DragInitialize(content::WebContents* contents) OVERRIDE;
  virtual void OnDragOver(IDataObject* data_object) OVERRIDE;
  virtual void OnDragEnter(IDataObject* data_object) OVERRIDE;
  virtual void OnDrop(IDataObject* data_object) OVERRIDE;
  virtual void OnDragLeave(IDataObject* data_object) OVERRIDE;
  virtual bool AddDragData(const WebDropData& drop_data,
                           ui::OSExchangeData* data) OVERRIDE;

 private:
  // The TabContentsWrapper for the drag.
  // Weak reference; may be NULL if the contents aren't contained in a wrapper
  // (e.g. WebUI dialogs).
  TabContentsWrapper* tab_;

  DISALLOW_COPY_AND_ASSIGN(WebDragBookmarkHandlerWin);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_DRAG_BOOKMARK_HANDLER_WIN_H_
