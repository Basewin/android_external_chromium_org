// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/candidate_window_view.h"
#include "chrome/browser/chromeos/input_method/infolist_window_view.h"
#include "ui/base/ime/chromeos/ibus_bridge.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {
namespace input_method {

class CandidateWindow;
class DelayableWidget;
class ModeIndicatorController;

// The implementation of CandidateWindowController.
// CandidateWindowController controls the CandidateWindow.
class CandidateWindowControllerImpl
    : public CandidateWindowController,
      public CandidateWindowView::Observer,
      public IBusPanelCandidateWindowHandlerInterface {
 public:
  CandidateWindowControllerImpl();
  virtual ~CandidateWindowControllerImpl();

  // CandidateWindowController overrides:
  virtual bool Init() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void AddObserver(
      CandidateWindowController::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      CandidateWindowController::Observer* observer) OVERRIDE;

 protected:
  // Returns infolist window position. This function handles right or bottom
  // position overflow. Usually, infolist window is clipped with right-top of
  // candidate window, but the position should be changed based on position
  // overflow. If right of infolist window is out of screen, infolist window is
  // shown left-top of candidate window. If bottom of infolist window is out of
  // screen, infolist window is shown with clipping to bottom of screen.
  // Infolist window does not overflow top and left direction.
  static gfx::Point GetInfolistWindowPosition(
      const gfx::Rect& candidate_window_rect,
      const gfx::Rect& screen_rect,
      const gfx::Size& infolist_winodw_size);

  // Converts |candidate_window| to infolist entries. |focused_index| become
  // InfolistWindowView::InvalidFocusIndex if there is no selected entries.
  static void ConvertLookupTableToInfolistEntry(
      const CandidateWindow& candidate_window,
      std::vector<InfolistWindowView::Entry>* infolist_entries,
      size_t* focused_index);

  // Returns true if given |new_entries| is different from |old_entries| and
  // should update current window.
  static bool ShouldUpdateInfolist(
      const std::vector<InfolistWindowView::Entry>& old_entries,
      size_t old_focused_index,
      const std::vector<InfolistWindowView::Entry>& new_entries,
      size_t new_focused_index);

 private:
  // CandidateWindowView::Observer implementation.
  virtual void OnCandidateCommitted(int index, int button, int flags) OVERRIDE;
  virtual void OnCandidateWindowOpened() OVERRIDE;
  virtual void OnCandidateWindowClosed() OVERRIDE;

  // Creates the candidate window view.
  void CreateView();

  // IBusPanelCandidateWindowHandlerInterface overrides.
  virtual void HideAuxiliaryText() OVERRIDE;
  virtual void HideLookupTable() OVERRIDE;
  virtual void HidePreeditText() OVERRIDE;
  virtual void SetCursorBounds(const gfx::Rect& cursor_bounds,
                               const gfx::Rect& composition_head) OVERRIDE;
  virtual void UpdateAuxiliaryText(const std::string& utf8_text,
                                   bool visible) OVERRIDE;
  virtual void UpdateLookupTable(const CandidateWindow& candidate_window,
                                 bool visible) OVERRIDE;
  virtual void UpdatePreeditText(const std::string& utf8_text,
                                 unsigned int cursor, bool visible) OVERRIDE;
  virtual void FocusStateChanged(bool is_focused) OVERRIDE;

  // Updates infolist bounds, if current bounds is up-to-date, this function
  // does nothing.
  void UpdateInfolistBounds();

  // The candidate window view.
  CandidateWindowView* candidate_window_view_;

  // This is the outer frame of the candidate window view. The frame will
  // own |candidate_window_|.
  scoped_ptr<views::Widget> frame_;

  // This is the outer frame of the infolist window view. The frame will
  // own |infolist_window_|.
  scoped_ptr<DelayableWidget> infolist_window_;

  // This is the controller of the IME mode indicator.
  scoped_ptr<ModeIndicatorController> mode_indicator_controller_;

  // The infolist entries and its focused index which currently shown in
  // Infolist window.
  std::vector<InfolistWindowView::Entry> latest_infolist_entries_;
  size_t latest_infolist_focused_index_;

  ObserverList<CandidateWindowController::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(CandidateWindowControllerImpl);
};

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_IMPL_H_

}  // namespace input_method
}  // namespace chromeos
