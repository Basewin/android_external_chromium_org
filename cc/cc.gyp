# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'use_libcc_for_compositor%': 0,
    'cc_source_files': [
      'BitmapCanvasLayerTextureUpdater.cpp',
      'BitmapCanvasLayerTextureUpdater.h',
      'BitmapSkPictureCanvasLayerTextureUpdater.cpp',
      'BitmapSkPictureCanvasLayerTextureUpdater.h',
      'CCActiveAnimation.cpp',
      'CCActiveAnimation.h',
      'CCAppendQuadsData.h',
      'CCAnimationCurve.cpp',
      'CCAnimationCurve.h',
      'CCAnimationEvents.h',
      'CCCheckerboardDrawQuad.cpp',
      'CCCheckerboardDrawQuad.h',
      'CCCompletionEvent.h',
      'CCDamageTracker.cpp',
      'CCDamageTracker.h',
      'CCDebugBorderDrawQuad.cpp',
      'CCDebugBorderDrawQuad.h',
      'CCDebugRectHistory.cpp',
      'CCDebugRectHistory.h',
      'CCDelayBasedTimeSource.cpp',
      'CCDelayBasedTimeSource.h',
      'CCDirectRenderer.cpp',
      'CCDirectRenderer.h',
      'CCDrawQuad.cpp',
      'CCDrawQuad.h',
      'CCFontAtlas.cpp',
      'CCFontAtlas.h',
      'CCFrameRateController.cpp',
      'CCFrameRateController.h',
      'CCFrameRateCounter.cpp',
      'CCFrameRateCounter.h',
      'CCGraphicsContext.h',
      'CCHeadsUpDisplayLayerImpl.cpp',
      'CCHeadsUpDisplayLayerImpl.h',
      'CCIOSurfaceDrawQuad.cpp',
      'CCIOSurfaceDrawQuad.h',
      'CCIOSurfaceLayerImpl.cpp',
      'CCIOSurfaceLayerImpl.h',
      'CCInputHandler.h',
      'CCKeyframedAnimationCurve.cpp',
      'CCKeyframedAnimationCurve.h',
      'CCLayerAnimationController.cpp',
      'CCLayerAnimationController.h',
      'CCLayerImpl.cpp',
      'CCLayerImpl.h',
      'CCLayerIterator.cpp',
      'CCLayerIterator.h',
      'CCLayerQuad.cpp',
      'CCLayerQuad.h',
      'CCLayerSorter.cpp',
      'CCLayerSorter.h',
      'CCLayerTilingData.cpp',
      'CCLayerTilingData.h',
      'CCLayerTreeHost.cpp',
      'CCLayerTreeHost.h',
      'CCLayerTreeHostClient.h',
      'CCLayerTreeHostCommon.cpp',
      'CCLayerTreeHostCommon.h',
      'CCLayerTreeHostImpl.cpp',
      'CCLayerTreeHostImpl.h',
      'CCMathUtil.cpp',
      'CCMathUtil.h',
      'CCOcclusionTracker.cpp',
      'CCOcclusionTracker.h',
      'CCOverdrawMetrics.cpp',
      'CCOverdrawMetrics.h',
      'CCPageScaleAnimation.cpp',
      'CCPageScaleAnimation.h',
      'CCPrioritizedTexture.cpp',
      'CCPrioritizedTexture.h',
      'CCPrioritizedTextureManager.cpp',
      'CCPrioritizedTextureManager.h',
      'CCPriorityCalculator.cpp',
      'CCPriorityCalculator.h',
      'CCProxy.cpp',
      'CCProxy.h',
      'CCQuadCuller.cpp',
      'CCQuadCuller.h',
      'CCQuadSink.h',
      'CCRenderPass.cpp',
      'CCRenderPass.h',
      'CCRenderPassDrawQuad.cpp',
      'CCRenderPassDrawQuad.h',
      'CCRenderPassSink.h',
      'CCRenderSurface.cpp',
      'CCRenderSurface.h',
      'CCRenderSurfaceFilters.cpp',
      'CCRenderSurfaceFilters.h',
      'CCRenderer.h',
      'CCRendererGL.cpp',
      'CCRendererGL.h',
      'CCRenderingStats.h',
      'CCResourceProvider.cpp',
      'CCResourceProvider.h',
      'CCScheduler.cpp',
      'CCScheduler.h',
      'CCSchedulerStateMachine.cpp',
      'CCSchedulerStateMachine.h',
      'CCScopedTexture.cpp',
      'CCScopedTexture.h',
      'CCScopedThreadProxy.h',
      'CCScrollbarAnimationController.cpp',
      'CCScrollbarAnimationController.h',
      'CCScrollbarAnimationControllerLinearFade.cpp',
      'CCScrollbarAnimationControllerLinearFade.h',
      'CCScrollbarLayerImpl.cpp',
      'CCScrollbarLayerImpl.h',
      'CCScrollbarGeometryFixedThumb.cpp',
      'CCScrollbarGeometryFixedThumb.h',
      'CCScrollbarGeometryStub.cpp',
      'CCScrollbarGeometryStub.h',
      'CCSettings.cpp',
      'CCSettings.h',
      'CCSharedQuadState.cpp',
      'CCSharedQuadState.h',
      'CCSingleThreadProxy.cpp',
      'CCSingleThreadProxy.h',
      'CCSolidColorDrawQuad.cpp',
      'CCSolidColorDrawQuad.h',
      'CCSolidColorLayerImpl.cpp',
      'CCSolidColorLayerImpl.h',
      'CCStreamVideoDrawQuad.cpp',
      'CCStreamVideoDrawQuad.h',
      'CCTexture.cpp',
      'CCTexture.h',
      'CCTextureDrawQuad.cpp',
      'CCTextureDrawQuad.h',
      'CCTextureLayerImpl.cpp',
      'CCTextureLayerImpl.h',
      'CCTextureUpdateController.cpp',
      'CCTextureUpdateController.h',
      'CCTextureUpdateQueue.cpp',
      'CCTextureUpdateQueue.h',
      'CCThread.h',
      'CCThreadProxy.cpp',
      'CCThreadProxy.h',
      'CCThreadTask.h',
      'CCTileDrawQuad.cpp',
      'CCTileDrawQuad.h',
      'CCTiledLayerImpl.cpp',
      'CCTiledLayerImpl.h',
      'CCTimeSource.h',
      'CCTimer.cpp',
      'CCTimer.h',
      'CCTimingFunction.cpp',
      'CCTimingFunction.h',
      'CCVideoLayerImpl.cpp',
      'CCVideoLayerImpl.h',
      'CCYUVVideoDrawQuad.cpp',
      'CCYUVVideoDrawQuad.h',
      'CanvasLayerTextureUpdater.cpp',
      'CanvasLayerTextureUpdater.h',
      'ContentLayerChromium.cpp',
      'ContentLayerChromium.h',
      'ContentLayerChromiumClient.h',
      'FrameBufferSkPictureCanvasLayerTextureUpdater.cpp',
      'FrameBufferSkPictureCanvasLayerTextureUpdater.h',
      'GeometryBinding.cpp',
      'GeometryBinding.h',
      'HeadsUpDisplayLayerChromium.cpp',
      'HeadsUpDisplayLayerChromium.h',
      'IOSurfaceLayerChromium.cpp',
      'IOSurfaceLayerChromium.h',
      'ImageLayerChromium.cpp',
      'ImageLayerChromium.h',
      'LayerChromium.cpp',
      'LayerChromium.h',
      'LayerPainterChromium.h',
      'LayerTextureSubImage.cpp',
      'LayerTextureSubImage.h',
      'LayerTextureUpdater.h',
      'PlatformColor.h',
      'ProgramBinding.cpp',
      'ProgramBinding.h',
      'RateLimiter.cpp',
      'RateLimiter.h',
      'RenderSurfaceChromium.cpp',
      'RenderSurfaceChromium.h',
      'ScrollbarLayerChromium.cpp',
      'ScrollbarLayerChromium.h',
      'ShaderChromium.cpp',
      'ShaderChromium.h',
      'SkPictureCanvasLayerTextureUpdater.cpp',
      'SkPictureCanvasLayerTextureUpdater.h',
      'SolidColorLayerChromium.cpp',
      'SolidColorLayerChromium.h',
      'TextureCopier.cpp',
      'TextureCopier.h',
      'TextureLayerChromium.cpp',
      'TextureLayerChromium.h',
      'TextureLayerChromiumClient.h',
      'TextureUploader.h',
      'ThrottledTextureUploader.cpp',
      'ThrottledTextureUploader.h',
      'UnthrottledTextureUploader.h',
      'TiledLayerChromium.cpp',
      'TiledLayerChromium.h',
      'TreeSynchronizer.cpp',
      'TreeSynchronizer.h',
      'VideoLayerChromium.cpp',
      'VideoLayerChromium.h',
    ],
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../..',
      },{
        'webkit_src_dir': '../third_party/WebKit',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'cc',
      'type': 'static_library',
      'conditions': [
        ['use_libcc_for_compositor==1', {
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '<(DEPTH)/skia/skia.gyp:skia',
            '<(DEPTH)/ui/gl/gl.gyp:gl',
            '<(DEPTH)/ui/ui.gyp:ui',
            '<(webkit_src_dir)/Source/WTF/WTF.gyp/WTF.gyp:wtf',
            '<(webkit_src_dir)/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore_platform_geometry',
            '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit_wtf_support',
          ],
          'defines': [
            'WTF_USE_ACCELERATED_COMPOSITING=1',
          ],
          'include_dirs': [
            'stubs',
            '<(webkit_src_dir)/Source/Platform/chromium',
          ],
          'sources': [
            '<@(cc_source_files)',
            'stubs/Extensions3D.h',
            'stubs/Extensions3DChromium.h',
            'stubs/FloatPoint.h',
            'stubs/FloatPoint3D.h',
            'stubs/FloatQuad.h',
            'stubs/FloatRect.h',
            'stubs/FloatSize.h',
            'stubs/GraphicsContext3D.h',
            'stubs/GraphicsTypes3D.h',
            'stubs/IntPoint.h',
            'stubs/IntRect.h',
            'stubs/IntSize.h',
            'stubs/NotImplemented.h',
            'stubs/Region.h',
            'stubs/SkiaUtils.h',
            'stubs/TilingData.h',
            'stubs/TraceEvent.h',
            'stubs/UnitBezier.h',
          ],
        }],
      ],
    },
  ],
}
