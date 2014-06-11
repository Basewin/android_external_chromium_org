// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/audio_buffer.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decrypting_audio_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::AtMost;
using ::testing::IsNull;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const int kSampleRate = 44100;

// Make sure the kFakeAudioFrameSize is a valid frame size for all audio decoder
// configs used in this test.
static const int kFakeAudioFrameSize = 48;
static const uint8 kFakeKeyId[] = { 0x4b, 0x65, 0x79, 0x20, 0x49, 0x44 };
static const uint8 kFakeIv[DecryptConfig::kDecryptionKeySize] = { 0 };

// Create a fake non-empty encrypted buffer.
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(buffer_size));
  buffer->set_decrypt_config(scoped_ptr<DecryptConfig>(new DecryptConfig(
      std::string(reinterpret_cast<const char*>(kFakeKeyId),
                  arraysize(kFakeKeyId)),
      std::string(reinterpret_cast<const char*>(kFakeIv), arraysize(kFakeIv)),
      std::vector<SubsampleEntry>())));
  return buffer;
}

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

ACTION_P(ReturnBuffer, buffer) {
  return buffer;
}

ACTION_P(RunCallbackIfNotNull, param) {
  if (!arg0.is_null())
    arg0.Run(param);
}

MATCHER(IsEndOfStream, "end of stream") {
  return (arg->end_of_stream());
}

}  // namespace

class DecryptingAudioDecoderTest : public testing::Test {
 public:
  DecryptingAudioDecoderTest()
      : decoder_(new DecryptingAudioDecoder(
            message_loop_.message_loop_proxy(),
            base::Bind(
                &DecryptingAudioDecoderTest::RequestDecryptorNotification,
                base::Unretained(this)))),
        decryptor_(new StrictMock<MockDecryptor>()),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decoded_frame_(NULL),
        end_of_stream_frame_(AudioBuffer::CreateEOSBuffer()),
        decoded_frame_list_() {}

  virtual ~DecryptingAudioDecoderTest() {
    EXPECT_CALL(*this, RequestDecryptorNotification(_))
        .Times(testing::AnyNumber());
    Stop();
  }

  void InitializeAndExpectStatus(const AudioDecoderConfig& config,
                                 PipelineStatus status) {
    // Initialize data now that the config is known. Since the code uses
    // invalid values (that CreateEmptyBuffer() doesn't support), tweak them
    // just for CreateEmptyBuffer().
    int channels = ChannelLayoutToChannelCount(config.channel_layout());
    if (channels < 0)
      channels = 0;
    decoded_frame_ = AudioBuffer::CreateEmptyBuffer(config.channel_layout(),
                                                    channels,
                                                    kSampleRate,
                                                    kFakeAudioFrameSize,
                                                    kNoTimestamp());
    decoded_frame_list_.push_back(decoded_frame_);

    decoder_->Initialize(config, NewExpectedStatusCB(status),
                         base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                                    base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void Initialize() {
    EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
        .Times(AtMost(1))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*this, RequestDecryptorNotification(_))
        .WillOnce(RunCallbackIfNotNull(decryptor_.get()));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kAudio, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    config_.Initialize(kCodecVorbis, kSampleFormatPlanarF32,
                       CHANNEL_LAYOUT_STEREO, kSampleRate, NULL, 0, true, true,
                       base::TimeDelta(), 0);
    InitializeAndExpectStatus(config_, PIPELINE_OK);
  }

  void Reinitialize() {
    ReinitializeConfigChange(config_);
  }

  void ReinitializeConfigChange(const AudioDecoderConfig& new_config) {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio));
    EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kAudio, _))
              .WillOnce(SaveArg<1>(&key_added_cb_));
    decoder_->Initialize(new_config, NewExpectedStatusCB(PIPELINE_OK),
                         base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                                    base::Unretained(this)));
  }

  void DecodeAndExpectFrameReadyWith(
      scoped_refptr<DecoderBuffer> input,
      AudioDecoder::Status status,
      const Decryptor::AudioBuffers& audio_frames) {
    for (Decryptor::AudioBuffers::const_iterator it = audio_frames.begin();
         it != audio_frames.end(); ++it) {
      if ((*it)->end_of_stream()) {
        EXPECT_CALL(*this, FrameReady(IsEndOfStream()));
      } else {
        EXPECT_CALL(*this, FrameReady(*it));
      }
    }
    EXPECT_CALL(*this, DecodeDone(status));

    decoder_->Decode(input,
                     base::Bind(&DecryptingAudioDecoderTest::DecodeDone,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Sets up expectations and actions to put DecryptingAudioDecoder in an
  // active normal decoding state.
  void EnterNormalDecodingState() {
    Decryptor::AudioBuffers end_of_stream_frames_(1, end_of_stream_frame_);

    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
        .WillOnce(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNeedMoreData,
                                       Decryptor::AudioBuffers()));

    DecodeAndExpectFrameReadyWith(encrypted_buffer_, AudioDecoder::kOk,
                                Decryptor::AudioBuffers(1, decoded_frame_));
  }

  // Sets up expectations and actions to put DecryptingAudioDecoder in an end
  // of stream state. This function must be called after
  // EnterNormalDecodingState() to work.
  void EnterEndOfStreamState() {
    DecodeAndExpectFrameReadyWith(
        DecoderBuffer::CreateEOSBuffer(), AudioDecoder::kOk,
        Decryptor::AudioBuffers(1, end_of_stream_frame_));
  }

  // Make the audio decode callback pending by saving and not firing it.
  void EnterPendingDecodeState() {
    EXPECT_TRUE(pending_audio_decode_cb_.is_null());
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(encrypted_buffer_, _))
        .WillOnce(SaveArg<1>(&pending_audio_decode_cb_));

    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingAudioDecoderTest::DecodeDone,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
    // Make sure the Decode() on the decoder triggers a DecryptAndDecode() on
    // the decryptor.
    EXPECT_FALSE(pending_audio_decode_cb_.is_null());
  }

  void EnterWaitingForKeyState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(encrypted_buffer_, _))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNoKey,
                                       Decryptor::AudioBuffers()));
    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingAudioDecoderTest::DecodeDone,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void AbortPendingAudioDecodeCB() {
    if (!pending_audio_decode_cb_.is_null()) {
      base::ResetAndReturn(&pending_audio_decode_cb_).Run(
          Decryptor::kSuccess, Decryptor::AudioBuffers());
    }
  }

  void AbortAllPendingCBs() {
    if (!pending_init_cb_.is_null()) {
      ASSERT_TRUE(pending_audio_decode_cb_.is_null());
      base::ResetAndReturn(&pending_init_cb_).Run(false);
      return;
    }

    AbortPendingAudioDecodeCB();
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kAudio))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingAudioDecoderTest::AbortPendingAudioDecodeCB));

    decoder_->Reset(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  void Stop() {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingAudioDecoderTest::AbortAllPendingCBs));

    decoder_->Stop();
    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD1(RequestDecryptorNotification, void(const DecryptorReadyCB&));

  MOCK_METHOD1(FrameReady, void(const scoped_refptr<AudioBuffer>&));
  MOCK_METHOD1(DecodeDone, void(AudioDecoder::Status));

  base::MessageLoop message_loop_;
  scoped_ptr<DecryptingAudioDecoder> decoder_;
  scoped_ptr<StrictMock<MockDecryptor> > decryptor_;
  AudioDecoderConfig config_;

  Decryptor::DecoderInitCB pending_init_cb_;
  Decryptor::NewKeyCB key_added_cb_;
  Decryptor::AudioDecodeCB pending_audio_decode_cb_;

  // Constant buffer/frames, to be used/returned by |decoder_| and |decryptor_|.
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<AudioBuffer> decoded_frame_;
  scoped_refptr<AudioBuffer> end_of_stream_frame_;
  Decryptor::AudioBuffers decoded_frame_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptingAudioDecoderTest);
};

TEST_F(DecryptingAudioDecoderTest, Initialize_Normal) {
  Initialize();
}

// Ensure that DecryptingAudioDecoder only accepts encrypted audio.
TEST_F(DecryptingAudioDecoderTest, Initialize_UnencryptedAudioConfig) {
  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, kSampleRate, NULL, 0, false);

  InitializeAndExpectStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

// Ensure decoder handles invalid audio configs without crashing.
TEST_F(DecryptingAudioDecoderTest, Initialize_InvalidAudioConfig) {
  AudioDecoderConfig config(kUnknownAudioCodec, kUnknownSampleFormat,
                            CHANNEL_LAYOUT_STEREO, 0, NULL, 0, true);

  InitializeAndExpectStatus(config, PIPELINE_ERROR_DECODE);
}

// Ensure decoder handles unsupported audio configs without crashing.
TEST_F(DecryptingAudioDecoderTest, Initialize_UnsupportedAudioConfig) {
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(RunCallback<1>(false));
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillOnce(RunCallbackIfNotNull(decryptor_.get()));

  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, kSampleRate, NULL, 0, true);
  InitializeAndExpectStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(DecryptingAudioDecoderTest, Initialize_NullDecryptor) {
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillRepeatedly(RunCallbackIfNotNull(static_cast<Decryptor*>(NULL)));

  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, kSampleRate, NULL, 0, true);
  InitializeAndExpectStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

// Test normal decrypt and decode case.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_Normal) {
  Initialize();
  EnterNormalDecodingState();
}

// Test the case where the decryptor returns error when doing decrypt and
// decode.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_DecodeError) {
  Initialize();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kError,
                                     Decryptor::AudioBuffers()));

  DecodeAndExpectFrameReadyWith(
      encrypted_buffer_, AudioDecoder::kDecodeError, Decryptor::AudioBuffers());
}

// Test the case where the decryptor returns kNeedMoreData to ask for more
// buffers before it can produce a frame.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_NeedMoreData) {
  Initialize();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillOnce(RunCallback<1>(Decryptor::kNeedMoreData,
                               Decryptor::AudioBuffers()))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));

  // We expect it to eventually return kOk, with any number of returns of
  // kNotEnoughData beforehand.
  bool frame_delivered = false;
  EXPECT_CALL(*this, FrameReady(decoded_frame_))
      .WillOnce(Assign(&frame_delivered, true));

  while (!frame_delivered) {
    ASSERT_NO_FATAL_FAILURE(DecodeAndExpectFrameReadyWith(
        encrypted_buffer_, AudioDecoder::kOk, Decryptor::AudioBuffers()));
  }
}

// Test the case where the decryptor returns multiple decoded frames.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_MultipleFrames) {
  Initialize();

  scoped_refptr<AudioBuffer> frame_a = AudioBuffer::CreateEmptyBuffer(
      config_.channel_layout(),
      ChannelLayoutToChannelCount(config_.channel_layout()),
      kSampleRate,
      kFakeAudioFrameSize,
      kNoTimestamp());
  scoped_refptr<AudioBuffer> frame_b = AudioBuffer::CreateEmptyBuffer(
      config_.channel_layout(),
      ChannelLayoutToChannelCount(config_.channel_layout()),
      kSampleRate,
      kFakeAudioFrameSize,
      kNoTimestamp());
  decoded_frame_list_.push_back(frame_a);
  decoded_frame_list_.push_back(frame_b);

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillOnce(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));

  Decryptor::AudioBuffers buffers;
  buffers.push_back(decoded_frame_);
  buffers.push_back(frame_a);
  buffers.push_back(frame_b);
  DecodeAndExpectFrameReadyWith(encrypted_buffer_, AudioDecoder::kOk, buffers);
}

// Test the case where the decryptor receives end-of-stream buffer.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_EndOfStream) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
}

// Test reinitializing decode with a new config
TEST_F(DecryptingAudioDecoderTest, Reinitialize_ConfigChange) {
  Initialize();

  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .Times(AtMost(1))
      .WillOnce(RunCallback<1>(true));

  // The new config is different from the initial config in bits-per-channel,
  // channel layout and samples_per_second.
  AudioDecoderConfig new_config(kCodecVorbis, kSampleFormatPlanarS16,
                                CHANNEL_LAYOUT_5_1, 88200, NULL, 0, true);
  EXPECT_NE(new_config.bits_per_channel(), config_.bits_per_channel());
  EXPECT_NE(new_config.channel_layout(), config_.channel_layout());
  EXPECT_NE(new_config.samples_per_second(), config_.samples_per_second());

  ReinitializeConfigChange(new_config);
  message_loop_.RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kWaitingForKey state.
TEST_F(DecryptingAudioDecoderTest, KeyAdded_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(*this, FrameReady(decoded_frame_));
  EXPECT_CALL(*this, DecodeDone(AudioDecoder::kOk));
  key_added_cb_.Run();
  message_loop_.RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kPendingDecode state.
TEST_F(DecryptingAudioDecoderTest, KeyAdded_DruingPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(*this, FrameReady(decoded_frame_));
  EXPECT_CALL(*this, DecodeDone(AudioDecoder::kOk));
  // The audio decode callback is returned after the correct decryption key is
  // added.
  key_added_cb_.Run();
  base::ResetAndReturn(&pending_audio_decode_cb_).Run(
      Decryptor::kNoKey, Decryptor::AudioBuffers());
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringIdleAfterInitialization) {
  Initialize();
  Reset();
}

// Test resetting when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
}

// Test resetting when the decoder is in kPendingDecode state.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, DecodeDone(AudioDecoder::kAborted));

  Reset();
}

// Test resetting when the decoder is in kWaitingForKey state.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, DecodeDone(AudioDecoder::kAborted));

  Reset();
}

// Test resetting when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingAudioDecoderTest, Reset_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting after the decoder has been reset.
TEST_F(DecryptingAudioDecoderTest, Reset_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Reset();
}

}  // namespace media
