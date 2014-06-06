# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_lib',
      'type': '<(component)',
      'dependencies': [
        '../ui/aura/aura.gyp:aura',
        '../ui/app_list/app_list.gyp:app_list',
        '../ui/views/views.gyp:views',
        '../ui/accessibility/accessibility.gyp:ax_gen',
        '../skia/skia.gyp:skia',
      ],
      'defines': [
        'ATHENA_IMPLEMENTATION',
      ],
      'sources': [
        # All .cc, .h under athena, except unittests
        'activity/activity.cc',
        'activity/activity_factory.cc',
        'activity/activity_manager_impl.cc',
        'activity/activity_view_manager_impl.cc',
        'activity/public/activity.h',
        'activity/public/activity_factory.h',
        'activity/public/activity_manager.h',
        'activity/public/activity_view_manager.h',
        'activity/public/activity_view_model.h',
        'athena_export.h',
        'home/app_list_view_delegate.cc',
        'home/app_list_view_delegate.h',
        'home/home_card_impl.cc',
        'home/public/home_card.h',
	'input/public/input_manager.h',
	'input/public/accelerator_manager.h',
	'input/input_manager_impl.cc',
	'input/accelerator_manager_impl.cc',
	'input/accelerator_manager_impl.h',
        'screen/background_controller.cc',
        'screen/background_controller.h',
        'screen/public/screen_manager.h',
        'screen/screen_manager_impl.cc',
        'wm/public/window_manager.h',
        'wm/window_manager_impl.cc',
        'wm/window_overview_mode.cc',
        'wm/window_overview_mode.h',
      ],
    },
    {
      'target_name': 'athena_content_lib',
      'type': '<(component)',
      'dependencies': [
        'athena_lib',
        '../content/content.gyp:content_browser',
      ],
      'defines': [
        'ATHENA_IMPLEMENTATION',
      ],
      'sources': [
        'content/public/content_activity_factory.h',
        'content/content_activity_factory.cc',
        'content/web_activity.h',
        'content/web_activity.cc',
      ],
    },
    {
      'target_name': 'athena_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../ui/accessibility/accessibility.gyp:ax_gen',
        '../ui/aura/aura.gyp:aura_test_support',
        '../ui/base/ui_base.gyp:ui_base_test_support',
        '../ui/compositor/compositor.gyp:compositor_test_support',
        '../ui/views/views.gyp:views',
        '../ui/wm/wm.gyp:wm',
        '../url/url.gyp:url_lib',
        'athena_lib',
      ],
      'sources': [
        'main/athena_launcher.cc',
        'main/athena_launcher.h',
        'main/placeholder.cc',
        'main/placeholder.h',
        'test/athena_test_base.cc',
        'test/athena_test_base.h',
        'test/athena_test_helper.cc',
        'test/athena_test_helper.h',
        'test/sample_activity.cc',
        'test/sample_activity.h',
        'test/sample_activity_factory.cc',
        'test/sample_activity_factory.h',
      ],
    },
    {
      'target_name': 'athena_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'athena_lib',
        'athena_test_support',
      ],
      'sources': [
        'test/athena_unittests.cc',
        'input/accelerator_manager_unittest.cc',
        'wm/window_manager_unittest.cc',
      ],
    }
  ],
}

