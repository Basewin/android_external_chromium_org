// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_RENDERER_H_
#define MEDIA_BASE_AUDIO_RENDERER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class DemuxerStream;

class MEDIA_EXPORT AudioRenderer {
 public:
  // First parameter is the current time that has been rendered.
  // Second parameter is the maximum time value that the clock cannot exceed.
  typedef base::Callback<void(base::TimeDelta, base::TimeDelta)> TimeCB;

  AudioRenderer();
  virtual ~AudioRenderer();

  // Initialize an AudioRenderer with |stream|, executing |init_cb| upon
  // completion.
  //
  // |statistics_cb| is executed periodically with audio rendering stats.
  //
  // |time_cb| is executed whenever time has advanced by way of audio rendering.
  //
  // |buffering_state_cb| is executed when audio rendering has either run out of
  // data or has enough data to continue playback.
  //
  // |ended_cb| is executed when audio rendering has reached the end of stream.
  //
  // |error_cb| is executed if an error was encountered.
  virtual void Initialize(DemuxerStream* stream,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const TimeCB& time_cb,
                          const BufferingStateCB& buffering_state_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb) = 0;

  // Signal audio playback to start at the current rate. It is expected that
  // |time_cb| will eventually start being run with time updates.
  //
  // TODO(scherkus): Fold into TimeSource API.
  virtual void StartRendering() = 0;

  // Signal audio playback to stop until further notice. It is expected that
  // |time_cb| will no longer be run.
  //
  // TODO(scherkus): Fold into TimeSource API.
  virtual void StopRendering() = 0;

  // Sets the media time to start rendering from. Only valid to call while not
  // currently rendering.
  //
  // TODO(scherkus): Fold into TimeSource API.
  virtual void SetMediaTime(base::TimeDelta time) = 0;

  // Discard any audio data, executing |callback| when completed.
  //
  // Clients should expect |buffering_state_cb| to be called with
  // BUFFERING_HAVE_NOTHING while flushing is in progress.
  virtual void Flush(const base::Closure& callback) = 0;

  // Starts playback by reading from |stream| and decoding and rendering audio.
  //
  // Only valid to call after a successful Initialize() or Flush().
  virtual void StartPlaying() = 0;

  // Stop all operations in preparation for being deleted, executing |callback|
  // when complete.
  virtual void Stop(const base::Closure& callback) = 0;

  // Updates the current playback rate.
  //
  // TODO(scherkus): Fold into TimeSource API.
  virtual void SetPlaybackRate(float playback_rate) = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRenderer);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_RENDERER_H_
