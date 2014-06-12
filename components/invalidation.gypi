# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'invalidation',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../jingle/jingle.gyp:notifier',
        '../sync/sync.gyp:sync',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        'keyed_service_core',
        'pref_registry',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'invalidation/invalidation_logger.cc',
        'invalidation/invalidation_logger.h',
        'invalidation/invalidation_logger_observer.h',
        'invalidation/invalidation_prefs.cc',
        'invalidation/invalidation_prefs.h',
        'invalidation/invalidation_service.h',
        'invalidation/invalidation_service_util.cc',
        'invalidation/invalidation_service_util.h',
        'invalidation/invalidation_switches.cc',
        'invalidation/invalidation_switches.h',
        'invalidation/invalidator_registrar.cc',
        'invalidation/invalidator_registrar.h',
        'invalidation/invalidator_storage.cc',
        'invalidation/invalidator_storage.h',
        'invalidation/profile_invalidation_provider.cc',
        'invalidation/profile_invalidation_provider.h',
        'invalidation/ticl_settings_provider.cc',
        'invalidation/ticl_settings_provider.h',
      ],
      'conditions': [
          ['OS != "android"', {
            'sources': [
              'invalidation/gcm_network_channel.cc',
              'invalidation/gcm_network_channel.h',
              'invalidation/gcm_network_channel_delegate.h',
              'invalidation/invalidation_notifier.cc',
              'invalidation/invalidation_notifier.h',
              'invalidation/non_blocking_invalidator.cc',
              'invalidation/non_blocking_invalidator.h',
              'invalidation/notifier_reason_util.cc',
              'invalidation/notifier_reason_util.h',
              'invalidation/p2p_invalidator.cc',
              'invalidation/p2p_invalidator.h',
              'invalidation/push_client_channel.cc',
              'invalidation/push_client_channel.h',
              'invalidation/state_writer.h',
              'invalidation/sync_invalidation_listener.cc',
              'invalidation/sync_invalidation_listener.h',
              'invalidation/sync_system_resources.cc',
              'invalidation/sync_system_resources.h',
            ],
          }],
      ],
    },

    {
      'target_name': 'invalidation_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../jingle/jingle.gyp:notifier',
        '../jingle/jingle.gyp:notifier_test_util',
        '../net/net.gyp:net',
        '../sync/sync.gyp:sync',
        '../sync/sync.gyp:test_support_sync_notifier',
        '../testing/gmock.gyp:gmock',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        'keyed_service_core',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'invalidation/fake_invalidation_handler.cc',
        'invalidation/fake_invalidation_handler.h',
        'invalidation/fake_invalidation_state_tracker.cc',
        'invalidation/fake_invalidation_state_tracker.h',
        'invalidation/fake_invalidator.cc',
        'invalidation/fake_invalidator.h',
        'invalidation/invalidator_test_template.cc',
        'invalidation/invalidator_test_template.h',
      ],
      'conditions': [
          ['OS != "android"', {
            'sources': [
              'invalidation/p2p_invalidation_service.cc',
              'invalidation/p2p_invalidation_service.h',
            ],
          }],
      ],
    },
  ],
}
