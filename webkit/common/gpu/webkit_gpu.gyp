# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    # TODO(stuartmorgan): All dependencies from code built on iOS to
    # webkit/ should be removed, at which point this condition can be
    # removed.
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'webkit_gpu',
          'type': '<(component)',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/cc/cc.gyp:cc',
            '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '<(DEPTH)/gpu/command_buffer/command_buffer.gyp:gles2_utils',
            '<(DEPTH)/gpu/gpu.gyp:command_buffer_service',
            '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
            '<(DEPTH)/gpu/gpu.gyp:gles2_implementation',
            '<(DEPTH)/skia/skia.gyp:skia',
            '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink_minimal',
            '<(DEPTH)/ui/gl/gl.gyp:gl',
            '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
            '<(DEPTH)/ui/ui.gyp:ui',
          ],
          'sources': [
            # This list contains all .h and .cc in gpu except for test code.
            'context_provider_in_process.cc',
            'context_provider_in_process.h',
            'gl_bindings_skia_cmd_buffer.cc',
            'gl_bindings_skia_cmd_buffer.h',
            'grcontext_for_webgraphicscontext3d.cc',
            'grcontext_for_webgraphicscontext3d.h',
            'managed_memory_policy_convert.cc',
            'managed_memory_policy_convert.h',
            'test_context_provider_factory.cc',
            'test_context_provider_factory.h',
            'webgraphicscontext3d_in_process_command_buffer_impl.cc',
            'webgraphicscontext3d_in_process_command_buffer_impl.h',
            'webgraphicscontext3d_provider_impl.cc',
            'webgraphicscontext3d_provider_impl.h',
          ],
          'defines': [
            'WEBKIT_GPU_IMPLEMENTATION',
          ],
          'conditions': [
            ['use_angle_translator==1', {
              'dependencies': [
                '<(DEPTH)/third_party/angle_dx11/src/build_angle.gyp:translator',
              ],
            }, {
              'dependencies': [
                '<(DEPTH)/third_party/angle_dx11/src/build_angle.gyp:translator_glsl',
              ],
            }],
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        },
      ],
    }],
  ],
}
