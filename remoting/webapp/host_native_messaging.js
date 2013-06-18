// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class to communicate with the Host components via Native Messaging.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.HostNativeMessaging = function() {
  /**
   * @type {number}
   * @private
   */
  this.nextId_ = 0;

  /**
   * @type {Object.<number, remoting.HostNativeMessaging.PendingReply>}
   * @private
   */
  this.pendingReplies_ = {};

  /** @type {?chrome.extension.Port} @private */
  this.port_ = null;

  /** @type {string} @private */
  this.version_ = ''
};

/**
 * Type used for entries of |pendingReplies_| list.
 *
 * @param {string} type Type of the originating request.
 * @param {?function(...):void} callback The callback, if any, to be triggered
 *     on response. The actual parameters depend on the original request type.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @constructor
 */
remoting.HostNativeMessaging.PendingReply = function(type, callback, onError) {
  this.type = type;
  this.callback = callback;
  this.onError = onError;
};

/**
 * Sets up connection to the Native Messaging host process and exchanges
 * 'hello' messages. If Native Messaging is not available or the host
 * process is not installed, this returns false to the callback.
 *
 * @param {function(boolean): void} onDone Called with the result of
 *     initialization.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.initialize = function(onDone) {
  // TODO(lambroslambrou): Remove 'alwaysTrue ||' once the Chrome Remote Desktop
  // native messaging API is stable.
  var alwaysTrue = true;
  if (alwaysTrue || !chrome.runtime.connectNative) {
    console.log('Native Messaging API not available');
    onDone(false);
    return;
  }

  // NativeMessaging API exists on Chrome 26.xxx but fails to notify
  // onDisconnect in the case where the Host components are not installed. Need
  // to blacklist these versions of Chrome.
  var majorVersion = navigator.appVersion.match('Chrome/(\\d+)\.')[1];
  if (!majorVersion || majorVersion <= 26) {
    console.log('Native Messaging not supported on this version of Chrome');
    onDone(false);
    return;
  }

  try {
    this.port_ = chrome.runtime.connectNative(
        'com.google.chrome.remote_desktop');
    this.port_.onMessage.addListener(this.onIncomingMessage_.bind(this));
    this.port_.onDisconnect.addListener(this.onDisconnect_.bind(this));
    this.postMessage_({type: 'hello'}, onDone.bind(null, true),
                      onDone.bind(null, false));
  } catch (err) {
    console.log('Native Messaging initialization failed: ',
                /** @type {*} */ (err));
    onDone(false);
    return;
  }
};

/**
 * Verifies that |object| is of type |type|, logging an error if not.
 *
 * @param {string} name Name of the object, to be included in the error log.
 * @param {*} object Object to test.
 * @param {string} type Expected type of the object.
 * @return {boolean} Result of test.
 */
function checkType_(name, object, type) {
  if (typeof(object) !== type) {
    console.error('NativeMessaging: "', name, '" expected to be of type "',
                  type, '", got: ', object);
    return false;
  }
  return true;
}

/**
 * Returns |result| as an AsyncResult. If |result| is not valid, returns null
 * and logs an error.
 *
 * @param {*} result
 * @return {remoting.HostController.AsyncResult?} Converted result.
 */
function asAsyncResult_(result) {
  if (!checkType_('result', result, 'string')) {
    return null;
  }
  if (!remoting.HostController.AsyncResult.hasOwnProperty(result)) {
    console.error('NativeMessaging: unexpected result code: ', result);
    return null;
  }
  return remoting.HostController.AsyncResult[result];
}

/**
 * Returns |result| as a HostController.State. If |result| is not valid,
 * returns null and logs an error.
 *
 * @param {*} result
 * @return {remoting.HostController.State?} Converted result.
 */
function asHostState_(result) {
  if (!checkType_('result', result, 'string')) {
    return null;
  }
  if (!remoting.HostController.State.hasOwnProperty(result)) {
    console.error('NativeMessaging: unexpected result code: ', result);
    return null;
  }
  return remoting.HostController.State[result];
}

/**
 * Attaches a new ID to the supplied message, and posts it to the Native
 * Messaging port, adding |callback| to the list of pending replies.
 * |message| should have its 'type' field set, and any other fields set
 * depending on the message type.
 *
 * @param {{type: string}} message The message to post.
 * @param {?function(...):void} callback The callback, if any, to be triggered
 *     on response.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 * @private
 */
remoting.HostNativeMessaging.prototype.postMessage_ = function(message,
                                                               callback,
                                                               onError) {
  var id = this.nextId_++;
  message['id'] = id;
  this.pendingReplies_[id] = new remoting.HostNativeMessaging.PendingReply(
    message.type + 'Response', callback, onError);
  this.port_.postMessage(message);
};

/**
 * Handler for incoming Native Messages.
 *
 * @param {Object} message The received message.
 * @return {void} Nothing.
 * @private
 */
remoting.HostNativeMessaging.prototype.onIncomingMessage_ = function(message) {
  /** @type {number} */
  var id = message['id'];
  if (typeof(id) != 'number') {
    console.error('NativeMessaging: missing or non-numeric id');
    return;
  }
  var reply = this.pendingReplies_[id];
  if (!reply) {
    console.error('NativeMessaging: unexpected id: ', id);
    return;
  }
  delete this.pendingReplies_[id];

  var callback = reply.callback;
  var onError = reply.onError;

  /** @type {string} */
  var type = message['type'];
  if (!checkType_('type', type, 'string')) {
    onError(remoting.Error.UNEXPECTED);
    return;
  }
  if (type != reply.type) {
    console.error('NativeMessaging: expected reply type: ', reply.type,
                  ', got: ', type);
    onError(remoting.Error.UNEXPECTED);
    return;
  }

  switch (type) {
    case 'helloResponse':
      /** @type {string} */
      var version = message['version'];
      if (checkType_('version', version, 'string')) {
        this.version_ = version;
        callback();
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'getHostNameResponse':
      /** @type {*} */
      var hostname = message['hostname'];
      if (checkType_('hostname', hostname, 'string')) {
        callback(hostname);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'getPinHashResponse':
      /** @type {*} */
      var hash = message['hash'];
      if (checkType_('hash', hash, 'string')) {
        callback(hash);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'generateKeyPairResponse':
      /** @type {*} */
      var privateKey = message['privateKey'];
      /** @type {*} */
      var publicKey = message['publicKey'];
      if (checkType_('privateKey', privateKey, 'string') &&
          checkType_('publicKey', publicKey, 'string')) {
        callback(privateKey, publicKey);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'updateDaemonConfigResponse':
      var result = asAsyncResult_(message['result']);
      if (result != null) {
        callback(result);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'getDaemonConfigResponse':
      /** @type {*} */
      var config = message['config'];
      if (checkType_('config', config, 'object')) {
        callback(config);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'getUsageStatsConsentResponse':
      /** @type {*} */
      var supported = message['supported'];
      /** @type {*} */
      var allowed = message['allowed'];
      /** @type {*} */
      var setByPolicy = message['setByPolicy'];
      if (checkType_('supported', supported, 'boolean') &&
          checkType_('allowed', allowed, 'boolean') &&
          checkType_('setByPolicy', setByPolicy, 'boolean')) {
        callback(supported, allowed, setByPolicy);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'startDaemonResponse':
    case 'stopDaemonResponse':
      var result = asAsyncResult_(message['result']);
      if (result != null) {
        callback(result);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    case 'getDaemonStateResponse':
      var state = asHostState_(message['state']);
      if (state != null) {
        callback(state);
      } else {
        onError(remoting.Error.UNEXPECTED);
      }
      break;

    default:
      console.error('Unexpected native message: ', message);
      onError(remoting.Error.UNEXPECTED);
  }
};

/**
 * @return {void} Nothing.
 * @private
 */
remoting.HostNativeMessaging.prototype.onDisconnect_ = function() {
  console.error('Native Message port disconnected');

  // Notify the error-handlers of any requests that are still outstanding.
  for (var id in this.pendingReplies_) {
    this.pendingReplies_[/** @type {number} */(id)].onError(
        remoting.Error.UNEXPECTED);
  }
  this.pendingReplies_ = {};
}

/**
 * @param {function(string):void} callback Callback to be called with the
 *     local hostname.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getHostName = function(callback,
                                                              onError) {
  this.postMessage_({type: 'getHostName'}, callback, onError);
};

/**
 * Calculates PIN hash value to be stored in the config, passing the resulting
 * hash value base64-encoded to the callback.
 *
 * @param {string} hostId The host ID.
 * @param {string} pin The PIN.
 * @param {function(string):void} callback Callback.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getPinHash = function(hostId, pin,
                                                             callback,
                                                             onError) {
  this.postMessage_({
      type: 'getPinHash',
      hostId: hostId,
      pin: pin
  }, callback, onError);
};

/**
 * Generates new key pair to use for the host. The specified callback is called
 * when the key is generated. The key is returned in format understood by the
 * host (PublicKeyInfo structure encoded with ASN.1 DER, and then BASE64).
 *
 * @param {function(string, string):void} callback Callback.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.generateKeyPair = function(callback,
                                                                  onError) {
  this.postMessage_({type: 'generateKeyPair'}, callback, onError);
};

/**
 * Updates host config with the values specified in |config|. All
 * fields that are not specified in |config| remain
 * unchanged. Following parameters cannot be changed using this
 * function: host_id, xmpp_login. Error is returned if |config|
 * includes these parameters. Changes take effect before the callback
 * is called.
 *
 * @param {Object} config The new config parameters.
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback to be called when finished.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.updateDaemonConfig =
  function(config, callback, onError) {
  this.postMessage_({
      type: 'updateDaemonConfig',
      config: config
  }, callback, onError);
};

/**
 * Loads daemon config. The config is passed as a JSON formatted string to the
 * callback.
 *
 * @param {function(Object):void} callback Callback.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getDaemonConfig = function(callback,
                                                                  onError) {
  this.postMessage_({type: 'getDaemonConfig'}, callback, onError);
};

/**
 * Retrieves daemon version. The version is returned as a dotted decimal string
 * of the form major.minor.build.patch.
 * @return {string} The daemon version, or the empty string if not available.
 */
remoting.HostNativeMessaging.prototype.getDaemonVersion = function() {
  // Return the cached version from the 'hello' exchange.
  return this.version_;
};

/**
 * Get the user's consent to crash reporting. The consent flags are passed to
 * the callback as booleans: supported, allowed, set-by-policy.
 *
 * @param {function(boolean, boolean, boolean):void} callback Callback.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getUsageStatsConsent =
  function(callback, onError) {
    this.postMessage_({type: 'getUsageStatsConsent'}, callback, onError);
};

/**
 * Starts the daemon process with the specified configuration.
 *
 * @param {Object} config Host configuration.
 * @param {boolean} consent Consent to report crash dumps.
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.startDaemon = function(
  config, consent, callback, onError) {
  this.postMessage_({
      type: 'startDaemon',
      config: config,
      consent: consent
  }, callback, onError);
};

/**
 * Stops the daemon process.
 *
 * @param {function(remoting.HostController.AsyncResult):void} callback
 *     Callback.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.stopDaemon = function(callback,
                                                             onError) {
  this.postMessage_({type: 'stopDaemon'}, callback, onError);
};

/**
 * Gets the installed/running state of the Host process.
 *
 * @param {function(remoting.HostController.State):void} callback Callback.
 * @param {function(remoting.Error):void} onError The callback to be triggered
 *     on error.
 * @return {void} Nothing.
 */
remoting.HostNativeMessaging.prototype.getDaemonState = function(callback,
                                                                 onError) {
  this.postMessage_({type: 'getDaemonState'}, callback, onError);
}
