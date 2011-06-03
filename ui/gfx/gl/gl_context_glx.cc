// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <X11/Xlib.h>
}

#include "ui/gfx/gl/gl_context_glx.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/mesa/MesaLib/include/GL/osmesa.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface_glx.h"

namespace gfx {

namespace {

// scoped_ptr functor for XFree(). Use as follows:
//   scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> foo(...);
// where "XVisualInfo" is any X type that is freed with XFree.
class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

bool IsCompositingWindowManagerActive(Display* display) {
  // The X macro "None" has been undefined by gl_bindings.h.
  const int kNone = 0;
  static Atom net_wm_cm_s0 = kNone;
  if (net_wm_cm_s0 == kNone) {
    net_wm_cm_s0 = XInternAtom(display, "_NET_WM_CM_S0", True);
  }
  if (net_wm_cm_s0 == kNone) {
    return false;
  }
  return XGetSelectionOwner(display, net_wm_cm_s0) != kNone;
}

}  // namespace anonymous

GLContextGLX::GLContextGLX()
  : context_(NULL) {
}

GLContextGLX::~GLContextGLX() {
  Destroy();
}

bool GLContextGLX::Initialize(GLContext* shared_context,
                              GLSurface* compatible_surface) {
  GLSurfaceGLX* surface_glx = static_cast<GLSurfaceGLX*>(compatible_surface);

  GLXFBConfig config = static_cast<GLXFBConfig>(surface_glx->GetConfig());

  // The means by which the context is created depends on whether the drawable
  // type works reliably with GLX 1.3. If it does not then fall back to GLX 1.2.
  if (config) {
    context_ = glXCreateNewContext(
        GLSurfaceGLX::GetDisplay(),
        static_cast<GLXFBConfig>(surface_glx->GetConfig()),
        GLX_RGBA_TYPE,
        static_cast<GLXContext>(
            shared_context ? shared_context->GetHandle() : NULL),
        True);
  } else {
    Display* display = GLSurfaceGLX::GetDisplay();

    // Get the visuals for the X drawable.
    XWindowAttributes attributes;
    XGetWindowAttributes(
        display,
        reinterpret_cast<GLXDrawable>(surface_glx->GetHandle()),
        &attributes);

    XVisualInfo visual_info_template;
    visual_info_template.visualid = XVisualIDFromVisual(attributes.visual);

    int visual_info_count = 0;
    scoped_ptr_malloc<XVisualInfo, ScopedPtrXFree> visual_info_list(
        XGetVisualInfo(display, VisualIDMask,
                       &visual_info_template,
                       &visual_info_count));

    DCHECK(visual_info_list.get());
    if (visual_info_count == 0) {
      LOG(ERROR) << "No visual info for visual ID.";
      return false;
    }

    // Attempt to create a context with each visual in turn until one works.
    context_ = glXCreateContext(display, visual_info_list.get(), 0, True);
  }

  if (!context_) {
    LOG(ERROR) << "Couldn't create GL context.";
    Destroy();
    return false;
  }

  return true;
}

void GLContextGLX::Destroy() {
  if (context_) {
    glXDestroyContext(GLSurfaceGLX::GetDisplay(),
                      static_cast<GLXContext>(context_));
    context_ = NULL;
  }
}

bool GLContextGLX::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  GLSurfaceGLX* surface_glx = static_cast<GLSurfaceGLX*>(surface);

  if (!glXMakeCurrent(
      GLSurfaceGLX::GetDisplay(),
      reinterpret_cast<GLXDrawable>(surface->GetHandle()),
      static_cast<GLXContext>(context_))) {
    LOG(ERROR) << "Couldn't make context current with X drawable.";
    Destroy();
    return false;
  }

  return true;
}

void GLContextGLX::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  glXMakeContextCurrent(GLSurfaceGLX::GetDisplay(), 0, 0, NULL);
}

bool GLContextGLX::IsCurrent(GLSurface* surface) {
  if (glXGetCurrentContext() != static_cast<GLXContext>(context_))
    return false;

  if (surface) {
    if (glXGetCurrentDrawable() !=
        reinterpret_cast<GLXDrawable>(surface->GetHandle())) {
      return false;
    }
  }

  return true;
}

void* GLContextGLX::GetHandle() {
  return context_;
}

void GLContextGLX::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  if (HasExtension("GLX_EXT_swap_control") && glXSwapIntervalEXT) {
    // Only enable vsync if we aren't using a compositing window
    // manager. At the moment, compositing window managers don't
    // respect this setting anyway (tearing still occurs) and it
    // dramatically increases latency.
    if (interval == 1 &&
        IsCompositingWindowManagerActive(GLSurfaceGLX::GetDisplay())) {
      LOG(INFO) <<
          "Forcing vsync off because compositing window manager was detected.";
      interval = 0;
    }
    glXSwapIntervalEXT(
        GLSurfaceGLX::GetDisplay(),
        glXGetCurrentDrawable(),
        interval);
  } else {
    if(interval == 0)
      LOG(WARNING) <<
          "Could not disable vsync: driver does not "
          "support GLX_EXT_swap_control";
  }
}

std::string GLContextGLX::GetExtensions() {
  DCHECK(IsCurrent(NULL));
  const char* extensions = glXQueryExtensionsString(
      GLSurfaceGLX::GetDisplay(),
      0);
  if (extensions) {
    return GLContext::GetExtensions() + " " + extensions;
  }

  return GLContext::GetExtensions();
}

}  // namespace gfx
