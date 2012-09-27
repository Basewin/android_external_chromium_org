// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  var getURL = chrome.extension.getURL;
  var URL_TEST = "http://127.0.0.1:PORT/test";
  chrome.tabs.create({"url": "about:blank"}, function(tab) {
    var tabId = tab.id;
    chrome.test.getConfig(function(config) {
      var fixPort = function(url) {
        return url.replace(/PORT/g, config.testServer.port);
      };
      URL_TEST = fixPort(URL_TEST);

      chrome.test.runTests([
        // Navigates to a different site, but then modifies the history using
        // history.pushState().
        function crossProcessHistory() {
          expect([
            { label: "a-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/e.html') }},
            { label: "a-onCommitted",
              event: "onCommitted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: [],
                         transitionType: "link",
                         url: getURL('crossProcess/e.html') }},
            { label: "a-onDOMContentLoaded",
              event: "onDOMContentLoaded",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/e.html') }},
            { label: "a-onCompleted",
              event: "onCompleted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/e.html') }},
            { label: "a-onHistoryStateUpdated",
              event: "onHistoryStateUpdated",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                       transitionQualifiers: [],
                         transitionType: "link",
                         url: getURL('crossProcess/empty.html') }},
            { label: "b-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         processId: 1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_TEST + "2" }},
            { label: "b-onErrorOccurred",
              event: "onErrorOccurred",
              details: { error: "net::ERR_ABORTED",
                         frameId: 0,
                         processId: 1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_TEST + "2" }}],
            [ navigationOrder("a-"),
              [ "a-onCompleted", "b-onBeforeNavigate",
                "a-onHistoryStateUpdated"] ]);

          chrome.tabs.update(
              tabId,
              { url: getURL('crossProcess/e.html?' + config.testServer.port) });
        },

        // A page with an iframe that changes its history state using
        // history.pushState before the iframe is committed.
        function crossProcessHistoryIFrame() {
          expect([
            { label: "a-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/h.html') }},
            { label: "a-onCommitted",
              event: "onCommitted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: [],
                         transitionType: "link",
                         url: getURL('crossProcess/h.html') }},
            { label: "a-onDOMContentLoaded",
              event: "onDOMContentLoaded",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/h.html') }},
            { label: "a-onCompleted",
              event: "onCompleted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/h.html') }},
            { label: "a-onHistoryStateUpdated",
              event: "onHistoryStateUpdated",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: [],
                         transitionType: "link",
                         url: getURL('crossProcess/empty.html') }},
            { label: "b-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 1,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_TEST + "5" }},
            { label: "b-onCommitted",
              event: "onCommitted",
              details: { frameId: 1,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: [],
                         transitionType: "auto_subframe",
                         url: URL_TEST + "5" }},
            { label: "b-onDOMContentLoaded",
              event: "onDOMContentLoaded",
              details: { frameId: 1,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_TEST + "5" }},
            { label: "b-onCompleted",
              event: "onCompleted",
              details: { frameId: 1,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_TEST + "5" }}],
            [ navigationOrder("a-"), navigationOrder("b-"),
              [ "a-onCompleted", "b-onBeforeNavigate",
                "a-onHistoryStateUpdated"] ]);

          chrome.tabs.update(
              tabId,
              { url: getURL('crossProcess/h.html?' + config.testServer.port) });
        },

        // Navigates to a different site, but then modifies the history using
        // history.replaceState().
        function crossProcessHistoryReplace() {
          expect([
            { label: "a-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/i.html') }},
            { label: "a-onCommitted",
              event: "onCommitted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         transitionQualifiers: [],
                         transitionType: "link",
                         url: getURL('crossProcess/i.html') }},
            { label: "a-onDOMContentLoaded",
              event: "onDOMContentLoaded",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/i.html') }},
            { label: "a-onCompleted",
              event: "onCompleted",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                         url: getURL('crossProcess/i.html') }},
            { label: "a-onHistoryStateUpdated",
              event: "onHistoryStateUpdated",
              details: { frameId: 0,
                         processId: 0,
                         tabId: 0,
                         timeStamp: 0,
                       transitionQualifiers: [],
                         transitionType: "link",
                         url: getURL('crossProcess/empty.html') }},
            { label: "b-onBeforeNavigate",
              event: "onBeforeNavigate",
              details: { frameId: 0,
                         processId: 1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_TEST + "6" }},
            { label: "b-onErrorOccurred",
              event: "onErrorOccurred",
              details: { error: "net::ERR_ABORTED",
                         frameId: 0,
                         processId: 1,
                         tabId: 0,
                         timeStamp: 0,
                         url: URL_TEST + "6" }}],
            [ navigationOrder("a-"),
              [ "a-onCompleted", "b-onBeforeNavigate",
                "a-onHistoryStateUpdated"] ]);

          chrome.tabs.update(
              tabId,
              { url: getURL('crossProcess/i.html?' + config.testServer.port) });
        },

      ]);
    });
  });
};
