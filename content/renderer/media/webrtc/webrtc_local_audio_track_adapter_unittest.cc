// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace content {

namespace {

class MockWebRtcAudioSink : public webrtc::AudioTrackSinkInterface {
 public:
  MockWebRtcAudioSink() {}
  ~MockWebRtcAudioSink() {}
  MOCK_METHOD5(OnData, void(const void* audio_data,
                            int bits_per_sample,
                            int sample_rate,
                            int number_of_channels,
                            int number_of_frames));
};

}  // namespace

class WebRtcLocalAudioTrackAdapterTest : public ::testing::Test {
 public:
  WebRtcLocalAudioTrackAdapterTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 480),
        adapter_(WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL)),
        capturer_(WebRtcAudioCapturer::CreateCapturer(
            -1, StreamDeviceInfo(), blink::WebMediaConstraints(), NULL)),
        track_(new WebRtcLocalAudioTrack(adapter_, capturer_, NULL)) {}

 protected:
  virtual void SetUp() OVERRIDE {
    static_cast<WebRtcLocalAudioSourceProvider*>(
        track_->audio_source_provider())->SetSinkParamsForTesting(params_);
    track_->OnSetFormat(params_);
    EXPECT_TRUE(track_->GetAudioAdapter()->enabled());
  }

  media::AudioParameters params_;
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_;
  scoped_refptr<WebRtcAudioCapturer> capturer_;
  scoped_ptr<WebRtcLocalAudioTrack> track_;
};

// Adds and Removes a WebRtcAudioSink to a local audio track.
TEST_F(WebRtcLocalAudioTrackAdapterTest, AddAndRemoveSink) {
  // Add a sink to the webrtc track.
  scoped_ptr<MockWebRtcAudioSink> sink(new MockWebRtcAudioSink());
  webrtc::AudioTrackInterface* webrtc_track =
      static_cast<webrtc::AudioTrackInterface*>(adapter_.get());
  webrtc_track->AddSink(sink.get());

  // Send a packet via |track_| and it data should reach the sink of the
  // |adapter_|.
  scoped_ptr<int16[]> data(
      new int16[params_.frames_per_buffer() * params_.channels()]);
  EXPECT_CALL(*sink,
              OnData(_, 16, params_.sample_rate(), params_.channels(),
                     params_.frames_per_buffer()));
  track_->Capture(data.get(), base::TimeDelta(), 255, false, false);

  // Remove the sink from the webrtc track.
  webrtc_track->RemoveSink(sink.get());
  sink.reset();

  // Verify that no more callback gets into the sink.
  track_->Capture(data.get(), base::TimeDelta(), 255, false, false);
}

}  // namespace content
