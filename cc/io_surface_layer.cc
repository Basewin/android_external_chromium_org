// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/io_surface_layer.h"

#include "CCIOSurfaceLayerImpl.h"

namespace cc {

scoped_refptr<IOSurfaceLayerChromium> IOSurfaceLayerChromium::create()
{
    return make_scoped_refptr(new IOSurfaceLayerChromium());
}

IOSurfaceLayerChromium::IOSurfaceLayerChromium()
    : LayerChromium()
    , m_ioSurfaceId(0)
{
}

IOSurfaceLayerChromium::~IOSurfaceLayerChromium()
{
}

void IOSurfaceLayerChromium::setIOSurfaceProperties(uint32_t ioSurfaceId, const IntSize& size)
{
    m_ioSurfaceId = ioSurfaceId;
    m_ioSurfaceSize = size;
    setNeedsCommit();
}

scoped_ptr<CCLayerImpl> IOSurfaceLayerChromium::createCCLayerImpl()
{
    return CCIOSurfaceLayerImpl::create(m_layerId).PassAs<CCLayerImpl>();
}

bool IOSurfaceLayerChromium::drawsContent() const
{
    return m_ioSurfaceId && LayerChromium::drawsContent();
}

void IOSurfaceLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCIOSurfaceLayerImpl* textureLayer = static_cast<CCIOSurfaceLayerImpl*>(layer);
    textureLayer->setIOSurfaceProperties(m_ioSurfaceId, m_ioSurfaceSize);
}

}
