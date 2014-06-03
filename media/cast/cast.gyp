# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'include_tests%': 1,
    'chromium_code': 1,
  },
  'conditions': [
    ['include_tests==1', {
      'includes': [ 'cast_testing.gypi' ]
    }],
  ],
  'targets': [
    {
      'target_name': 'cast_base',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_logging_proto',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'export_dependent_settings': [
        'cast_logging_proto',
      ],
      'sources': [
        'cast_config.cc',
        'cast_config.h',
        'cast_defines.h',
        'cast_environment.cc',
        'cast_environment.h',
        'base/clock_drift_smoother.cc',
        'base/clock_drift_smoother.h',
        'logging/encoding_event_subscriber.cc',
        'logging/encoding_event_subscriber.h',
        'logging/log_deserializer.cc',
        'logging/log_deserializer.h',
        'logging/log_serializer.cc',
        'logging/log_serializer.h',
        'logging/logging_defines.cc',
        'logging/logging_defines.h',
        'logging/logging_impl.cc',
        'logging/logging_impl.h',
        'logging/logging_raw.cc',
        'logging/logging_raw.h',
        'logging/raw_event_subscriber.h',
        'logging/raw_event_subscriber_bundle.cc',
        'logging/raw_event_subscriber_bundle.h',
        'logging/receiver_time_offset_estimator.h',
        'logging/receiver_time_offset_estimator_impl.cc',
        'logging/receiver_time_offset_estimator_impl.h',
        'logging/simple_event_subscriber.cc',
        'logging/simple_event_subscriber.h',
        'logging/stats_event_subscriber.cc',
        'logging/stats_event_subscriber.h',
        'rtp_timestamp_helper.cc',
        'rtp_timestamp_helper.h',
        'transport/cast_transport_config.cc',
        'transport/cast_transport_config.h',
        'transport/cast_transport_defines.h',
        'transport/cast_transport_sender.h',
      ], # source
    },
    {
      'target_name': 'cast_logging_proto',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'logging/proto/proto_utils.cc',
        'logging/proto/raw_events.proto',
      ],
      'variables': {
        'proto_in_dir': 'logging/proto',
        'proto_out_dir': 'media/cast/logging/proto',
      },
      'includes': ['../../build/protoc.gypi'],
    },
    {
      'target_name': 'cast_receiver',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_rtcp',
        'cast_transport',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        '<(DEPTH)/third_party/opus/opus.gyp:opus',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'sources': [
        'cast_receiver.h',
        'framer/cast_message_builder.cc',
        'framer/cast_message_builder.h',
        'framer/frame_buffer.cc',
        'framer/frame_buffer.h',
        'framer/frame_id_map.cc',
        'framer/frame_id_map.h',
        'framer/framer.cc',
        'framer/framer.h',
        'receiver/audio_decoder.cc',
        'receiver/audio_decoder.h',
        'receiver/cast_receiver_impl.cc',
        'receiver/cast_receiver_impl.h',
        'receiver/frame_receiver.cc',
        'receiver/frame_receiver.h',
        'receiver/video_decoder.cc',
        'receiver/video_decoder.h',
        'rtp_receiver/receiver_stats.cc',
        'rtp_receiver/receiver_stats.h',
        'rtp_receiver/rtp_receiver_defines.cc',
        'rtp_receiver/rtp_receiver_defines.h',
        'rtp_receiver/rtp_parser/rtp_parser.cc',
        'rtp_receiver/rtp_parser/rtp_parser.h',
      ], # source
    },
    {
      'target_name': 'cast_rtcp',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_transport',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'rtcp/rtcp_defines.cc',
        'rtcp/rtcp_defines.h',
        'rtcp/rtcp.h',
        'rtcp/rtcp.cc',
        'rtcp/rtcp_receiver.cc',
        'rtcp/rtcp_receiver.h',
        'rtcp/rtcp_sender.cc',
        'rtcp/rtcp_sender.h',
        'rtcp/rtcp_utility.cc',
        'rtcp/rtcp_utility.h',
        'rtcp/receiver_rtcp_event_subscriber.cc',
        'rtcp/receiver_rtcp_event_subscriber.cc',
      ], # source
    },
    {
      'target_name': 'cast_sender',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_rtcp',
        'cast_transport',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        '<(DEPTH)/third_party/opus/opus.gyp:opus',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
      ], # dependencies
      'sources': [
        'audio_sender/audio_encoder.h',
        'audio_sender/audio_encoder.cc',
        'audio_sender/audio_sender.h',
        'audio_sender/audio_sender.cc',
        'cast_sender.h',
        'cast_sender_impl.cc',
        'cast_sender_impl.h',
        'congestion_control/congestion_control.h',
        'congestion_control/congestion_control.cc',
        'video_sender/codecs/vp8/vp8_encoder.cc',
        'video_sender/codecs/vp8/vp8_encoder.h',
        'video_sender/external_video_encoder.h',
        'video_sender/external_video_encoder.cc',
        'video_sender/fake_software_video_encoder.h',
        'video_sender/fake_software_video_encoder.cc',
        'video_sender/software_video_encoder.h',
        'video_sender/video_encoder.h',
        'video_sender/video_encoder_impl.h',
        'video_sender/video_encoder_impl.cc',
        'video_sender/video_sender.h',
        'video_sender/video_sender.cc',
      ], # source
    },
    {
      'target_name': 'cast_transport',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'transport/cast_transport_sender_impl.cc',
        'transport/cast_transport_sender_impl.h',
        'transport/pacing/paced_sender.cc',
        'transport/pacing/paced_sender.h',
        'transport/rtcp/rtcp_builder.cc',
        'transport/rtcp/rtcp_builder.h',
        'transport/rtp_sender/packet_storage/packet_storage.cc',
        'transport/rtp_sender/packet_storage/packet_storage.h',
        'transport/rtp_sender/rtp_packetizer/rtp_packetizer.cc',
        'transport/rtp_sender/rtp_packetizer/rtp_packetizer.h',
        'transport/rtp_sender/rtp_sender.cc',
        'transport/rtp_sender/rtp_sender.h',
        'transport/transport/udp_transport.cc',
        'transport/transport/udp_transport.h',
        'transport/transport_audio_sender.cc',
        'transport/transport_audio_sender.h',
        'transport/transport_video_sender.cc',
        'transport/transport_video_sender.h',
        'transport/utility/transport_encryption_handler.cc',
        'transport/utility/transport_encryption_handler.h',
      ], # source
    },
  ],
}
