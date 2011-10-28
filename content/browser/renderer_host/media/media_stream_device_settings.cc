// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_device_settings.h"

#include "base/stl_util.h"
#include "base/task.h"
#include "content/browser/renderer_host/media/media_stream_settings_requester.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/browser_thread.h"

namespace media_stream {

typedef std::map<MediaStreamType, StreamDeviceInfoArray> DeviceMap;

// Device request contains all data needed to keep track of requests between the
// different calls.
struct MediaStreamDeviceSettings::SettingsRequest {
  SettingsRequest(int render_pid, int render_vid, const std::string& origin,
                  StreamOptions request_options)
    : render_process_id(render_pid),
      render_view_id(render_vid),
      security_origin(origin),
      options(request_options) {}

  // The render process id generating this request, needed for UI.
  int render_process_id;
  // The render view id generating this request, needed for UI.
  int render_view_id;
  // Security origin generated by WebKit.
  std::string security_origin;
  // Request options.
  StreamOptions options;
  // Map containing available devices for the requested capture types.
  DeviceMap devices;
};

MediaStreamDeviceSettings::MediaStreamDeviceSettings(
    SettingsRequester* requester)
    : requester_(requester),
      // TODO(macourteau) Change to false when UI exists.
      use_fake_ui_(true) {
  DCHECK(requester_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

MediaStreamDeviceSettings::~MediaStreamDeviceSettings() {
  STLDeleteValues(&requests_);
}

void MediaStreamDeviceSettings::RequestCaptureDeviceUsage(
    const std::string& label, int render_process_id, int render_view_id,
    const StreamOptions& request_options, const std::string& security_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (requests_.find(label) != requests_.end()) {
    // Request with this id already exists.
    requester_->SettingsError(label);
    return;
  }

  // Create a new request.
  requests_.insert(std::make_pair(label, new SettingsRequest(render_process_id,
                                                             render_view_id,
                                                             security_origin,
                                                             request_options)));
  // Ask for available devices.
  if (request_options.audio) {
    requester_->GetDevices(label, kAudioCapture);
  }
  if (request_options.video_option != StreamOptions::kNoCamera) {
    requester_->GetDevices(label, kVideoCapture);
  }
}

void MediaStreamDeviceSettings::AvailableDevices(
    const std::string& label, MediaStreamType stream_type,
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SettingsRequests::iterator request_it = requests_.find(label);
  if (request_it == requests_.end()) {
    // Request with this id doesn't exist.
    requester_->SettingsError(label);
    return;
  }

  // Add the answer for the request.
  SettingsRequest* request = request_it->second;
  request->devices[stream_type] = devices;

  // Check if we're done.
  size_t num_media_requests = 0;
  if (request->options.audio) {
    num_media_requests++;
  }
  if (request->options.video_option != StreamOptions::kNoCamera) {
    num_media_requests++;
  }

  if (request->devices.size() == num_media_requests) {
    // We have all answers needed.
    if (!use_fake_ui_) {
      // TODO(macourteau)
      // This is the place to:
      // - Choose what devices to use from some kind of settings, user dialog or
      //   default device.
      // - Request user permission / show we're using devices.
      DCHECK(false);
    } else {
      // Used to fake UI, which is needed for server based testing.
      // Choose first non-opened device for each media type.
      StreamDeviceInfoArray devices_to_use;
      for (DeviceMap::iterator it = request->devices.begin();
           it != request->devices.end(); ++it) {
        for (StreamDeviceInfoArray::iterator device_it = it->second.begin();
             device_it != it->second.end(); ++device_it) {
          if (!device_it->in_use) {
            devices_to_use.push_back(*device_it);
            break;
          }
        }
      }
      // Post result and delete request.
      requester_->DevicesAccepted(label, devices_to_use);
      requests_.erase(request_it);
      delete request;
    }
  }
}

void MediaStreamDeviceSettings::UseFakeUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  use_fake_ui_ = true;
}

}  // namespace media_stream
