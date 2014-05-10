// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_metrics_service_observer.h"

#include "chrome/browser/metrics/metrics_service.h"
#include "net/base/network_change_notifier.h"

ChromeBrowserMetricsServiceObserver::ChromeBrowserMetricsServiceObserver() {
  MetricsServiceHelper::AddMetricsServiceObserver(this);
}

ChromeBrowserMetricsServiceObserver::~ChromeBrowserMetricsServiceObserver() {
  MetricsServiceHelper::RemoveMetricsServiceObserver(this);
}

void ChromeBrowserMetricsServiceObserver::OnDidCreateMetricsLog() {
  net::NetworkChangeNotifier::LogOperatorCodeHistogram(
      net::NetworkChangeNotifier::GetConnectionType());
}
