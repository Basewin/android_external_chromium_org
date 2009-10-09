// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script contains unprivileged javascript APIs related to chrome
// extensions.  It is loaded by any extension-related context, such as content
// scripts or toolstrips.
// See user_script_slave.cc for script that is loaded by content scripts only.
// TODO(mpcomplete): we also load this in regular web pages, but don't need
// to.

var chrome = chrome || {};
(function () {
  native function OpenChannelToExtension(sourceId, targetId, name);
  native function CloseChannel(portId);
  native function PortAddRef(portId);
  native function PortRelease(portId);
  native function PostMessage(portId, msg);
  native function GetChromeHidden();

  var chromeHidden = GetChromeHidden();

  // Map of port IDs to port object.
  var ports = {};

  // Port object.  Represents a connection to another script context through
  // which messages can be passed.
  chrome.Port = function(portId, opt_name) {
    this.portId_ = portId;
    this.name = opt_name;
    this.onDisconnect = new chrome.Event();
    this.onMessage = new chrome.Event();
  };

  chromeHidden.Port = {};

  // Hidden port creation function.  We don't want to expose an API that lets
  // people add arbitrary port IDs to the port list.
  chromeHidden.Port.createPort = function(portId, opt_name) {
    if (ports[portId]) {
      throw new Error("Port '" + portId + "' already exists.");
    }
    var port = new chrome.Port(portId, opt_name);
    ports[portId] = port;
    chromeHidden.onUnload.addListener(function() {
      PortRelease(portId);
    });
    PortAddRef(portId);
    return port;
  }

  // Called by native code when a channel has been opened to this context.
  chromeHidden.Port.dispatchOnConnect = function(portId, channelName, tab,
                                                 sourceExtensionId,
                                                 targetExtensionId) {
    // Only create a new Port if someone is actually listening for a connection.
    // In addition to being an optimization, this also fixes a bug where if 2
    // channels were opened to and from the same process, closing one would
    // close both.
    if (targetExtensionId != chromeHidden.extensionId)
      return;  // not for us

    // Determine whether this is coming from another extension, and use the
    // right event.
    var connectEvent = (sourceExtensionId == chromeHidden.extensionId ?
        chrome.extension.onConnect : chrome.extension.onConnectExternal);
    if (connectEvent.hasListeners()) {
      var port = chromeHidden.Port.createPort(portId, channelName);
      if (tab) {
        tab = JSON.parse(tab);
      }
      port.sender = {tab: tab, id: sourceExtensionId};
      // TODO(EXTENSIONS_DEPRECATED): port.tab is obsolete.
      port.tab = port.sender.tab;
      connectEvent.dispatch(port);
    }
  };

  // Called by native code when a channel has been closed.
  chromeHidden.Port.dispatchOnDisconnect = function(portId) {
    var port = ports[portId];
    if (port) {
      port.onDisconnect.dispatch(port);
      delete ports[portId];
    }
  };

  // Called by native code when a message has been sent to the given port.
  chromeHidden.Port.dispatchOnMessage = function(msg, portId) {
    var port = ports[portId];
    if (port) {
      if (msg) {
        msg = JSON.parse(msg);
      }
      port.onMessage.dispatch(msg, port);
    }
  };

  // Sends a message asynchronously to the context on the other end of this
  // port.
  chrome.Port.prototype.postMessage = function(msg) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (msg === undefined)
      msg = null;
    PostMessage(this.portId_, JSON.stringify(msg));
  };

  // Disconnects the port from the other end.
  chrome.Port.prototype.disconnect = function() {
    delete ports[this.portId_];
    CloseChannel(this.portId_);
  }

  // This function is called on context initialization for both content scripts
  // and extension contexts.
  chrome.initExtension = function(extensionId) {
    delete chrome.initExtension;
    chromeHidden.extensionId = extensionId;

    chrome.extension = chrome.extension || {};

    // TODO(EXTENSIONS_DEPRECATED): chrome.self is obsolete.
    // http://code.google.com/p/chromium/issues/detail?id=16356
    chrome.self = chrome.extension;

    // Events for when a message channel is opened to our extension.
    chrome.extension.onConnect = new chrome.Event();
    chrome.extension.onConnectExternal = new chrome.Event();

    // Opens a message channel to the given target extension, or the current one
    // if unspecified.  Returns a Port for message passing.
    chrome.extension.connect = function(targetId_opt, connectInfo_opt) {
      var name = "";
      var targetId = extensionId;
      var nextArg = 0;
      if (typeof(arguments[nextArg]) == "string")
        targetId = arguments[nextArg++];
      if (typeof(arguments[nextArg]) == "object")
        name = arguments[nextArg++].name || name;

      var portId = OpenChannelToExtension(extensionId, targetId, name);
      if (portId >= 0)
        return chromeHidden.Port.createPort(portId, name);
      throw new Error("Error connecting to extension '" + targetId + "'");
    };

    // Returns a resource URL that can be used to fetch a resource from this
    // extension.
    chrome.extension.getURL = function(path) {
      return "chrome-extension://" + extensionId + "/" + path;
    };
  }
})();
