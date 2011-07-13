// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_views.h"

#include "ui/gfx/compositor/compositor.h"
#include "views/desktop/desktop_window_view.h"
#include "views/view.h"
#include "views/views_delegate.h"
#include "views/widget/native_widget_view.h"
#include "views/widget/root_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, public:

NativeWidgetViews::NativeWidgetViews(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      view_(NULL),
      active_(false),
      minimized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      hosting_widget_(NULL),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
}

NativeWidgetViews::~NativeWidgetViews() {
  delegate_->OnNativeWidgetDestroying();
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  view_.reset();
}

View* NativeWidgetViews::GetView() {
  return view_.get();
}

const View* NativeWidgetViews::GetView() const {
  return view_.get();
}

void NativeWidgetViews::OnActivate(bool active) {
  active_ = active;
  view_->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, NativeWidget implementation:

void NativeWidgetViews::InitNativeWidget(const Widget::InitParams& params) {
  ownership_ = params.ownership;
  view_.reset(new internal::NativeWidgetView(this));
  view_->SetBoundsRect(params.bounds);
  view_->SetPaintToLayer(true);

  View* parent_view = NULL;
  if (params.parent_widget) {
    hosting_widget_ = params.parent_widget;
    parent_view = hosting_widget_->GetChildViewParent();
  } else {
    parent_view = ViewsDelegate::views_delegate->GetDefaultParentView();
    hosting_widget_ = parent_view->GetWidget();
  }
  parent_view->AddChildView(view_.get());

  // TODO(beng): SetInitParams().
}

NonClientFrameView* NativeWidgetViews::CreateNonClientFrameView() {
  return NULL;
}

void NativeWidgetViews::UpdateFrameAfterFrameChange() {
}

bool NativeWidgetViews::ShouldUseNativeFrame() const {
//  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetViews::FrameTypeChanged() {
}

Widget* NativeWidgetViews::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetViews::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetViews::GetNativeView() const {
  return GetParentNativeWidget()->GetNativeView();
}

gfx::NativeWindow NativeWidgetViews::GetNativeWindow() const {
  return GetParentNativeWidget()->GetNativeWindow();
}

Widget* NativeWidgetViews::GetTopLevelWidget() {
  if (view_->parent() == ViewsDelegate::views_delegate->GetDefaultParentView())
    return GetWidget();
  // During Widget destruction, this function may be called after |view_| is
  // detached from a Widget, at which point this NativeWidget's Widget becomes
  // the effective toplevel.
  Widget* containing_widget = view_->GetWidget();
  return containing_widget ? containing_widget->GetTopLevelWidget()
                           : GetWidget();
}

const ui::Compositor* NativeWidgetViews::GetCompositor() const {
  return hosting_widget_->GetCompositor();
}

ui::Compositor* NativeWidgetViews::GetCompositor() {
  return hosting_widget_->GetCompositor();
}

void NativeWidgetViews::MarkLayerDirty() {
  view_->MarkLayerDirty();
}

void NativeWidgetViews::CalculateOffsetToAncestorWithLayer(gfx::Point* offset,
                                                           View** ancestor) {
  view_->CalculateOffsetToAncestorWithLayer(offset, ancestor);
}

void NativeWidgetViews::ViewRemoved(View* view) {
  internal::NativeWidgetPrivate* parent = GetParentNativeWidget();
  if (parent)
    parent->ViewRemoved(view);
}

void NativeWidgetViews::SetNativeWindowProperty(const char* name, void* value) {
  NOTIMPLEMENTED();
}

void* NativeWidgetViews::GetNativeWindowProperty(const char* name) const {
  NOTIMPLEMENTED();
  return NULL;
}

TooltipManager* NativeWidgetViews::GetTooltipManager() const {
  const internal::NativeWidgetPrivate* parent = GetParentNativeWidget();
  return parent ? parent->GetTooltipManager() : NULL;
}

bool NativeWidgetViews::IsScreenReaderActive() const {
  return GetParentNativeWidget()->IsScreenReaderActive();
}

void NativeWidgetViews::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  return GetParentNativeWidget()->SendNativeAccessibilityEvent(view,
                                                               event_type);
}

void NativeWidgetViews::SetMouseCapture() {
  View* parent_root_view = GetParentNativeWidget()->GetWidget()->GetRootView();
  static_cast<internal::RootView*>(parent_root_view)->set_capture_view(
      view_.get());
  GetParentNativeWidget()->SetMouseCapture();
}

void NativeWidgetViews::ReleaseMouseCapture() {
  View* parent_root_view = GetParentNativeWidget()->GetWidget()->GetRootView();
  static_cast<internal::RootView*>(parent_root_view)->set_capture_view(NULL);
  GetParentNativeWidget()->ReleaseMouseCapture();
}

bool NativeWidgetViews::HasMouseCapture() const {
  // NOTE: we may need to tweak this to only return true if the parent native
  // widget's RootView has us as the capture view.
  return GetParentNativeWidget()->HasMouseCapture();
}

void NativeWidgetViews::SetKeyboardCapture() {
  GetParentNativeWidget()->SetKeyboardCapture();
}

void NativeWidgetViews::ReleaseKeyboardCapture() {
  GetParentNativeWidget()->ReleaseKeyboardCapture();
}

bool NativeWidgetViews::HasKeyboardCapture() {
  return GetParentNativeWidget()->HasKeyboardCapture();
}

InputMethod* NativeWidgetViews::GetInputMethodNative() {
  return GetParentNativeWidget()->GetInputMethodNative();
}

void NativeWidgetViews::ReplaceInputMethod(InputMethod* input_method) {
  GetParentNativeWidget()->ReplaceInputMethod(input_method);
}

void NativeWidgetViews::CenterWindow(const gfx::Size& size) {
  // TODO(beng): actually center.
  GetView()->SetBounds(0, 0, size.width(), size.height());
}

void NativeWidgetViews::GetWindowBoundsAndMaximizedState(
    gfx::Rect* bounds,
    bool* maximized) const {
  *bounds = GetView()->bounds();
  *maximized = false;
}

void NativeWidgetViews::SetWindowTitle(const std::wstring& title) {
}

void NativeWidgetViews::SetWindowIcons(const SkBitmap& window_icon,
                                       const SkBitmap& app_icon) {
}

void NativeWidgetViews::SetAccessibleName(const std::wstring& name) {
}

void NativeWidgetViews::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
}

void NativeWidgetViews::SetAccessibleState(
    ui::AccessibilityTypes::State state) {
}

void NativeWidgetViews::BecomeModal() {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetViews::GetWindowScreenBounds() const {
  if (GetWidget() == GetWidget()->GetTopLevelWidget())
    return view_->bounds();
  gfx::Point origin = view_->bounds().origin();
  View::ConvertPointToScreen(view_->parent(), &origin);
  return gfx::Rect(origin.x(), origin.y(), view_->width(), view_->height());
}

gfx::Rect NativeWidgetViews::GetClientAreaScreenBounds() const {
  return GetWindowScreenBounds();
}

gfx::Rect NativeWidgetViews::GetRestoredBounds() const {
  return GetWindowScreenBounds();
}

void NativeWidgetViews::SetBounds(const gfx::Rect& bounds) {
  // |bounds| are supplied in the coordinates of the parent.
  view_->SetBoundsRect(bounds);
}

void NativeWidgetViews::SetSize(const gfx::Size& size) {
  view_->SetSize(size);
}

void NativeWidgetViews::SetBoundsConstrained(const gfx::Rect& bounds,
                                             Widget* other_widget) {
  // TODO(beng): honor other_widget.
  SetBounds(bounds);
}

void NativeWidgetViews::MoveAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::MoveToTop() {
  view_->parent()->ReorderChildView(view_.get(), -1);
}

void NativeWidgetViews::SetShape(gfx::NativeRegion region) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::Close() {
  Hide();
  if (close_widget_factory_.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE,
        close_widget_factory_.NewRunnableMethod(&NativeWidgetViews::CloseNow));
  }
}

void NativeWidgetViews::CloseNow() {
  // TODO(beng): what about the other case??
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetViews::EnableClose(bool enable) {
}

void NativeWidgetViews::Show() {
  view_->SetVisible(true);
}

void NativeWidgetViews::Hide() {
  view_->SetVisible(false);
}

void NativeWidgetViews::ShowNativeWidget(ShowState state) {
}

bool NativeWidgetViews::IsVisible() const {
  return view_->IsVisible();
}

void NativeWidgetViews::Activate() {
  // Enable WidgetObserverTest.ActivationChange when this is implemented.
  NOTIMPLEMENTED();
}

void NativeWidgetViews::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetViews::IsActive() const {
  return active_;
}

void NativeWidgetViews::SetAlwaysOnTop(bool on_top) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::Maximize() {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::Minimize() {
  gfx::Rect view_bounds = view_->bounds();
  gfx::Rect parent_bounds = view_->parent()->bounds();

  restored_bounds_ = view_bounds;
  restored_transform_ = view_->GetTransform();

  float aspect_ratio = static_cast<float>(view_bounds.width()) /
                       static_cast<float>(view_bounds.height());
  int target_size = 100;
  int target_height = target_size;
  int target_width = static_cast<int>(aspect_ratio * target_height);
  if (target_width > target_size) {
    target_width = target_size;
    target_height = static_cast<int>(target_width / aspect_ratio);
  }

  int target_x = 20;
  int target_y = parent_bounds.height() - target_size - 20;

  view_->SetBounds(
      target_x, target_y, view_bounds.width(), view_bounds.height());

  ui::Transform transform;
  transform.SetScale((float)target_width / (float)view_bounds.width(),
                     (float)target_height / (float)view_bounds.height());
  view_->SetTransform(transform);

  minimized_ = true;
}

bool NativeWidgetViews::IsMaximized() const {
  // NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetViews::IsMinimized() const {
  return minimized_;
}

void NativeWidgetViews::Restore() {
  minimized_ = false;
  view_->SetBoundsRect(restored_bounds_);
  view_->SetTransform(restored_transform_);
}

void NativeWidgetViews::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool NativeWidgetViews::IsFullscreen() const {
  // NOTIMPLEMENTED();
  return false;
}

void NativeWidgetViews::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

bool NativeWidgetViews::IsAccessibleWidget() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetViews::RunShellDrag(View* view,
                                     const ui::OSExchangeData& data,
                                     int operation) {
  GetParentNativeWidget()->RunShellDrag(view, data, operation);
}

void NativeWidgetViews::SchedulePaintInRect(const gfx::Rect& rect) {
  view_->SchedulePaintInternal(rect);
}

void NativeWidgetViews::SetCursor(gfx::NativeCursor cursor) {
  GetParentNativeWidget()->SetCursor(cursor);
}

void NativeWidgetViews::ClearNativeFocus() {
  GetParentNativeWidget()->ClearNativeFocus();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, private:

internal::NativeWidgetPrivate* NativeWidgetViews::GetParentNativeWidget() {
  Widget* containing_widget = view_->GetWidget();
  return containing_widget ? static_cast<internal::NativeWidgetPrivate*>(
      containing_widget->native_widget()) :
      NULL;
}

const internal::NativeWidgetPrivate*
    NativeWidgetViews::GetParentNativeWidget() const {
  const Widget* containing_widget = view_->GetWidget();
  return containing_widget ? static_cast<const internal::NativeWidgetPrivate*>(
      containing_widget->native_widget()) :
      NULL;
}

}  // namespace views
