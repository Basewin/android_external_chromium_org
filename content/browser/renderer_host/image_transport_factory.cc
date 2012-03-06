// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_factory.h"

#include <map>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/image_transport_client.h"
#include "content/common/gpu/client/command_buffer_proxy.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/compositor_setup.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/scoped_make_current.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

namespace {

ImageTransportFactory* g_factory;

class DefaultTransportFactory
    : public ui::DefaultContextFactory,
      public ImageTransportFactory {
 public:
  DefaultTransportFactory() {
    ui::DefaultContextFactory::Initialize();
  }

  virtual ui::ContextFactory* AsContextFactory() OVERRIDE {
    return this;
  }

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle(
      ui::Compositor* compositor) OVERRIDE {
    return gfx::GLSurfaceHandle();
  }

  virtual void DestroySharedSurfaceHandle(
      gfx::GLSurfaceHandle surface) OVERRIDE {
  }

  virtual scoped_refptr<ImageTransportClient> CreateTransportClient(
      const gfx::Size& size,
      uint64* transport_handle) OVERRIDE {
    return NULL;
  }

  virtual gfx::ScopedMakeCurrent* GetScopedMakeCurrent() OVERRIDE {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultTransportFactory);
};

class TestTransportFactory : public DefaultTransportFactory {
 public:
  TestTransportFactory() {}

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle(
      ui::Compositor* compositor) OVERRIDE {
    return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, true);
  }

  virtual scoped_refptr<ImageTransportClient> CreateTransportClient(
      const gfx::Size& size,
      uint64* transport_handle) OVERRIDE {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
    scoped_refptr<ImageTransportClient> surface(
        ImageTransportClient::Create(this, size));
    if (!surface || !surface->Initialize(transport_handle)) {
      LOG(ERROR) << "Failed to create ImageTransportClient";
      return NULL;
    }
    return surface;
#else
    return NULL;
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTransportFactory);
};

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
class InProcessTransportFactory : public DefaultTransportFactory {
 public:
  InProcessTransportFactory() {
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1));
    CHECK(surface_.get()) << "Unable to create compositor GL surface.";

    context_ = gfx::GLContext::CreateGLContext(
        NULL, surface_.get(), gfx::PreferIntegratedGpu);
    CHECK(context_.get()) <<"Unable to create compositor GL context.";

    set_share_group(context_->share_group());
  }

  virtual ~InProcessTransportFactory() {}

  virtual gfx::ScopedMakeCurrent* GetScopedMakeCurrent() OVERRIDE {
    return new gfx::ScopedMakeCurrent(context_.get(), surface_.get());
  }

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle(
      ui::Compositor* compositor) OVERRIDE {
    return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, true);
  }

  virtual scoped_refptr<ImageTransportClient> CreateTransportClient(
      const gfx::Size& size,
      uint64* transport_handle) OVERRIDE {
    scoped_refptr<ImageTransportClient> surface(
        ImageTransportClient::Create(this, size));
    if (!surface || !surface->Initialize(transport_handle)) {
      LOG(ERROR) << "Failed to create ImageTransportClient";
      return NULL;
    }
    return surface;
  }

 private:
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(InProcessTransportFactory);
};
#endif

class ImageTransportClientTexture : public ImageTransportClient {
 public:
  explicit ImageTransportClientTexture(const gfx::Size& size)
      : ImageTransportClient(true, size) {
  }

  virtual bool Initialize(uint64* surface_id) OVERRIDE {
    set_texture_id(*surface_id);
    return true;
  }

  virtual ~ImageTransportClientTexture() {}
  virtual void Update() OVERRIDE {}
  virtual TransportDIB::Handle Handle() const OVERRIDE {
    return TransportDIB::DefaultHandleValue();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageTransportClientTexture);
};

class CompositorSwapClient : public base::SupportsWeakPtr<CompositorSwapClient>,
                             public WebGraphicsContext3DSwapBuffersClient {
 public:
  explicit CompositorSwapClient(ui::Compositor* compositor)
      : compositor_(compositor) {
  }

  ~CompositorSwapClient() {
  }

  virtual void OnViewContextSwapBuffersPosted() OVERRIDE {
    compositor_->OnSwapBuffersPosted();
  }

  virtual void OnViewContextSwapBuffersComplete() OVERRIDE {
    compositor_->OnSwapBuffersComplete();
  }

  virtual void OnViewContextSwapBuffersAborted() OVERRIDE {
    compositor_->OnSwapBuffersAborted();
  }

 private:
  ui::Compositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(CompositorSwapClient);
};

class GpuProcessTransportFactory : public ui::ContextFactory,
                                   public ImageTransportFactory {
 public:
  GpuProcessTransportFactory() {}
  virtual ~GpuProcessTransportFactory() {
    DCHECK(per_compositor_data_.empty());
  }

  virtual WebKit::WebGraphicsContext3D* CreateContext(
      ui::Compositor* compositor) OVERRIDE {
    PerCompositorData* data = per_compositor_data_[compositor];
    if (!data)
      data = CreatePerCompositorData(compositor);

    WebKit::WebGraphicsContext3D::Attributes attrs;
    attrs.shareResources = true;
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
        new WebGraphicsContext3DCommandBufferImpl(
            data->surface_id, GURL(), data->swap_client->AsWeakPtr()));
    if (!context->Initialize(attrs))
      return NULL;
    return context.release();
  }

  virtual void RemoveCompositor(ui::Compositor* compositor) OVERRIDE {
    PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
    if (it == per_compositor_data_.end())
      return;
    PerCompositorData* data = it->second;
    DCHECK(data);
    GpuSurfaceTracker::Get()->RemoveSurface(data->surface_id);
    delete data;
    per_compositor_data_.erase(it);
  }

  virtual ui::ContextFactory* AsContextFactory() OVERRIDE {
    return this;
  }

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle(
      ui::Compositor* compositor) OVERRIDE {
    PerCompositorData* data = per_compositor_data_[compositor];
    if (!data)
      data = CreatePerCompositorData(compositor);
    gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
        gfx::kNullPluginWindow, true);
    ContentGLContext* context = data->shared_context->content_gl_context();
    handle.parent_client_id = context->GetChannelID();
    handle.parent_context_id = context->GetContextID();
    handle.parent_texture_id[0] = data->shared_context->createTexture();
    handle.parent_texture_id[1] = data->shared_context->createTexture();
    // Finish is overkill, but flush semantics don't apply cross-channel.
    // TODO(piman): Use a cross-channel synchronization mechanism instead.
    data->shared_context->finish();

    // This handle will not be correct after a GPU process crash / context lost.
    // TODO(piman): Fix this.
    return handle;
  }

  virtual void DestroySharedSurfaceHandle(
      gfx::GLSurfaceHandle surface) OVERRIDE {
    for (PerCompositorDataMap::iterator it = per_compositor_data_.begin();
         it != per_compositor_data_.end(); ++it) {
      PerCompositorData* data = it->second;
      DCHECK(data);
      ContentGLContext* context = data->shared_context->content_gl_context();
      uint32 client_id = context->GetChannelID();
      uint32 context_id = context->GetContextID();
      if (surface.parent_client_id == client_id &&
          surface.parent_context_id == context_id) {
        data->shared_context->deleteTexture(surface.parent_texture_id[0]);
        data->shared_context->deleteTexture(surface.parent_texture_id[1]);
        // Finish is overkill, but flush semantics don't apply cross-channel.
        // TODO(piman): Use a cross-channel synchronization mechanism instead.
        data->shared_context->finish();
        break;
      }
    }
  }

  virtual scoped_refptr<ImageTransportClient> CreateTransportClient(
      const gfx::Size& size,
      uint64* transport_handle) {
    scoped_refptr<ImageTransportClientTexture> image(
        new ImageTransportClientTexture(size));
    image->Initialize(transport_handle);
    return image;
  }

  virtual gfx::ScopedMakeCurrent* GetScopedMakeCurrent() { return NULL; }

 private:
  struct PerCompositorData {
    int surface_id;
    scoped_ptr<CompositorSwapClient> swap_client;
    scoped_ptr<WebGraphicsContext3DCommandBufferImpl> shared_context;
  };

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor) {
    DCHECK(!per_compositor_data_[compositor]);

    gfx::AcceleratedWidget widget = compositor->widget();
    GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

    PerCompositorData* data = new PerCompositorData;
    data->surface_id = tracker->AddSurfaceForNativeWidget(widget);
    tracker->SetSurfaceHandle(
        data->surface_id,
        gfx::GLSurfaceHandle(widget, false));
    data->swap_client.reset(new CompositorSwapClient(compositor));

    WebKit::WebGraphicsContext3D::Attributes attrs;
    attrs.shareResources = true;
    data->shared_context.reset(new WebGraphicsContext3DCommandBufferImpl(
          data->surface_id, GURL(), data->swap_client->AsWeakPtr()));
    data->shared_context->Initialize(attrs);
    data->shared_context->makeContextCurrent();

    per_compositor_data_[compositor] = data;
    return data;
  }

  typedef std::map<ui::Compositor*, PerCompositorData*> PerCompositorDataMap;
  PerCompositorDataMap per_compositor_data_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

}  // anonymous namespace

// static
void ImageTransportFactory::Initialize() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestCompositor)) {
    ui::SetupTestCompositor();
  }
  if (ui::IsTestCompositorEnabled()) {
    g_factory = new TestTransportFactory();
  } else if (command_line->HasSwitch(switches::kUIUseGPUProcess)) {
    g_factory = new GpuProcessTransportFactory();
  } else {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
    g_factory = new InProcessTransportFactory();
#else
    g_factory = new DefaultTransportFactory();
#endif
  }
  ui::ContextFactory::SetInstance(g_factory->AsContextFactory());
}

// static
void ImageTransportFactory::Terminate() {
  ui::ContextFactory::SetInstance(NULL);
  delete g_factory;
  g_factory = NULL;
}

// static
ImageTransportFactory* ImageTransportFactory::GetInstance() {
  return g_factory;
}
