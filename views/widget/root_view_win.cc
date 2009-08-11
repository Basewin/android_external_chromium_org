// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/root_view.h"

#include "app/drag_drop_types.h"
#include "app/gfx/canvas_paint.h"
#include "base/base_drag_source.h"
#include "base/logging.h"

namespace views {

void RootView::OnPaint(HWND hwnd) {
  gfx::Rect original_dirty_region = GetScheduledPaintRectConstrainedToSize();
  if (!original_dirty_region.IsEmpty()) {
    // Invoke InvalidateRect so that the dirty region of the window includes the
    // region we need to paint. If we didn't do this and the region didn't
    // include the dirty region, ProcessPaint would incorrectly mark everything
    // as clean. This can happen if a WM_PAINT is generated by the system before
    // the InvokeLater schedule by RootView is processed.
    RECT win_version = original_dirty_region.ToRECT();
    InvalidateRect(hwnd, &win_version, FALSE);
  }
  gfx::CanvasPaint canvas(hwnd);
  if (!canvas.isEmpty()) {
    const PAINTSTRUCT& ps = canvas.paintStruct();
    SchedulePaint(gfx::Rect(ps.rcPaint), false);
    if (NeedsPainting(false))
      ProcessPaint(&canvas);
  }
}

void RootView::StartDragForViewFromMouseEvent(
    View* view,
    IDataObject* data,
    int operation) {
  drag_view_ = view;
  scoped_refptr<BaseDragSource> drag_source(new BaseDragSource);
  DWORD effects;
  DoDragDrop(data, drag_source,
             DragDropTypes::DragOperationToDropEffect(operation), &effects);
  // If the view is removed during the drag operation, drag_view_ is set to
  // NULL.
  if (drag_view_ == view) {
    View* drag_view = drag_view_;
    drag_view_ = NULL;
    drag_view->OnDragDone();
  }
}

}
