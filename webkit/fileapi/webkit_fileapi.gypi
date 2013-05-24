# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'webkit_fileapi_sources': [
      '../fileapi/async_file_util.h',
      '../fileapi/async_file_util_adapter.cc',
      '../fileapi/async_file_util_adapter.h',
      '../fileapi/directory_entry.h',
      '../fileapi/file_permission_policy.cc',
      '../fileapi/file_permission_policy.h',
      '../fileapi/file_stream_writer.h',
      '../fileapi/file_system_context.cc',
      '../fileapi/file_system_context.h',
      '../fileapi/file_system_file_stream_reader.cc',
      '../fileapi/file_system_file_stream_reader.h',
      '../fileapi/file_system_operation.h',
      '../fileapi/file_system_operation_context.cc',
      '../fileapi/file_system_operation_context.h',
      '../fileapi/file_system_options.cc',
      '../fileapi/file_system_options.h',
      '../fileapi/file_system_types.h',
      '../fileapi/file_system_url.cc',
      '../fileapi/file_system_url.h',
      '../fileapi/file_system_util.cc',
      '../fileapi/file_system_util.h',
      '../fileapi/file_writer_delegate.cc',
      '../fileapi/file_writer_delegate.h',
      '../fileapi/local_file_stream_writer.cc',
      '../fileapi/local_file_stream_writer.h',
      '../fileapi/remote_file_system_proxy.h',
      '../fileapi/remove_operation_delegate.cc',
      '../fileapi/remove_operation_delegate.h',
      '../fileapi/syncable/file_change.cc',
      '../fileapi/syncable/file_change.h',
      '../fileapi/syncable/local_file_change_tracker.cc',
      '../fileapi/syncable/local_file_change_tracker.h',
      '../fileapi/syncable/local_file_sync_context.cc',
      '../fileapi/syncable/local_file_sync_context.h',
      '../fileapi/syncable/local_file_sync_status.cc',
      '../fileapi/syncable/local_file_sync_status.h',
      '../fileapi/syncable/local_origin_change_observer.h',
      '../fileapi/syncable/sync_callbacks.h',
      '../fileapi/syncable/sync_file_metadata.cc',
      '../fileapi/syncable/sync_file_metadata.h',
      '../fileapi/syncable/sync_file_status.h',
      '../fileapi/syncable/sync_file_type.h',
      '../fileapi/syncable/sync_action.h',
      '../fileapi/syncable/sync_direction.h',
      '../fileapi/syncable/sync_status_code.cc',
      '../fileapi/syncable/sync_status_code.h',
      '../fileapi/syncable/syncable_file_operation_runner.cc',
      '../fileapi/syncable/syncable_file_operation_runner.h',
      '../fileapi/syncable/syncable_file_system_operation.cc',
      '../fileapi/syncable/syncable_file_system_operation.h',
      '../fileapi/syncable/syncable_file_system_util.cc',
      '../fileapi/syncable/syncable_file_system_util.h',
      '../fileapi/test_mount_point_provider.cc',
      '../fileapi/test_mount_point_provider.h',
      '../fileapi/upload_file_system_file_element_reader.cc',
      '../fileapi/upload_file_system_file_element_reader.h',
      '../fileapi/webfilewriter_base.cc',
      '../fileapi/webfilewriter_base.h',
    ],
    'webkit_fileapi_chromeos_sources': [
      '../chromeos/fileapi/async_file_stream.h',
      '../chromeos/fileapi/cros_mount_point_provider.cc',
      '../chromeos/fileapi/cros_mount_point_provider.h',
      '../chromeos/fileapi/file_access_permissions.cc',
      '../chromeos/fileapi/file_access_permissions.h',
      '../chromeos/fileapi/file_util_async.h',
      '../chromeos/fileapi/remote_file_system_operation.cc',
      '../chromeos/fileapi/remote_file_system_operation.h',
      '../chromeos/fileapi/remote_file_stream_writer.cc',
      '../chromeos/fileapi/remote_file_stream_writer.h',
    ],
  },
  'targets': [
    {
      'target_name': 'dump_file_system',
      'type': 'executable',
      'sources': [
        '../fileapi/dump_file_system.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../support/webkit_support.gyp:webkit_storage',
      ],
    },
  ],
}
