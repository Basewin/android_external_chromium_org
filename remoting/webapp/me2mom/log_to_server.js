// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Module for sending log entries to the server.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.LogToServer = function() {
  /** @type Array.<string> */
  this.pendingEntries = [];
  /** @type {remoting.StatsAccumulator} */
  this.statsAccumulator = new remoting.StatsAccumulator();
  /** @type string */
  this.sessionId = '';
  /** @type number */
  this.sessionIdGenerationTime = 0;
};

// Local storage key.
/** @private */
remoting.LogToServer.KEY_ENABLED_ = 'remoting.LogToServer.enabled';

// Constants used for generating a session ID.
/** @private */
remoting.LogToServer.SESSION_ID_ALPHABET_ =
    'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890';
/** @private */
remoting.LogToServer.SESSION_ID_LEN_ = 20;

// The maximum age of a session ID, in milliseconds.
remoting.LogToServer.MAX_SESSION_ID_AGE = 24 * 60 * 60 * 1000;

// The time over which to accumulate connection statistics before logging them
// to the server, in milliseconds.
remoting.LogToServer.CONNECTION_STATS_ACCUMULATE_TIME = 60 * 1000;

/**
 * Enables or disables logging.
 *
 * @param {boolean} enabled whether logging is enabled
 */
remoting.LogToServer.prototype.setEnabled = function(enabled) {
  window.localStorage.setItem(remoting.LogToServer.KEY_ENABLED_,
     enabled ? 'true' : 'false');
}

/**
 * Logs a client session state change.
 *
 * @param {remoting.ClientSession.State} state
 * @param {remoting.ClientSession.ConnectionError} connectionError
 */
remoting.LogToServer.prototype.logClientSessionStateChange =
    function(state, connectionError) {
  this.maybeExpireSessionId();
  // Maybe set the session ID.
  if ((state == remoting.ClientSession.State.CONNECTING) ||
      (state == remoting.ClientSession.State.INITIALIZING) ||
      (state == remoting.ClientSession.State.CONNECTED)) {
    if (this.sessionId == '') {
      this.setSessionId();
    }
  }
  // Log the session state change.
  var entry = remoting.ServerLogEntry.makeClientSessionStateChange(
      state, connectionError);
  entry.addHostFields();
  entry.addChromeVersionField();
  entry.addWebappVersionField();
  entry.addSessionIdField(this.sessionId);
  this.log(entry);
  // Don't accumulate connection statistics across state changes.
  this.logAccumulatedStatistics();
  this.statsAccumulator.empty();
  // Maybe clear the session ID.
  if ((state == remoting.ClientSession.State.CLOSED) ||
      (state == remoting.ClientSession.State.CONNECTION_FAILED)) {
    this.clearSessionId();
  }
};

/**
 * Logs connection statistics.
 * @param {Object.<string, number>} stats the connection statistics
 */
remoting.LogToServer.prototype.logStatistics = function(stats) {
  this.maybeExpireSessionId();
  // Store the statistics.
  this.statsAccumulator.add(stats);
  // Send statistics to the server if they've been accumulating for at least
  // 60 seconds.
  if (this.statsAccumulator.getTimeSinceFirstValue() >=
      remoting.LogToServer.CONNECTION_STATS_ACCUMULATE_TIME) {
    this.logAccumulatedStatistics();
  }
};

/**
 * Moves connection statistics from the accumulator to the log server.
 *
 * If all the statistics are zero, then the accumulator is still emptied,
 * but the statistics are not sent to the log server.
 *
 * @private
 */
remoting.LogToServer.prototype.logAccumulatedStatistics = function() {
  var entry = remoting.ServerLogEntry.makeStats(this.statsAccumulator);
  if (entry) {
    entry.addHostFields();
    entry.addChromeVersionField();
    entry.addWebappVersionField();
    entry.addSessionIdField(this.sessionId);
    this.log(entry);
  }
  this.statsAccumulator.empty();
};

/**
 * Sends a log entry to the server.
 *
 * @private
 * @param {remoting.ServerLogEntry} entry
 */
remoting.LogToServer.prototype.log = function(entry) {
  if (!this.isEnabled()) {
    return;
  }
  // Send the stanza to the debug log.
  remoting.debug.log('Enqueueing log entry:');
  entry.toDebugLog(1);
  // Store a stanza for the entry.
  this.pendingEntries.push(entry.toStanza());
  // Stop if there's no connection to the server.
  if (!remoting.wcs) {
    return;
  }
  // Send all pending entries to the server.
  remoting.debug.log('Sending ' + this.pendingEntries.length + ' log ' +
      ((this.pendingEntries.length == 1) ? 'entry' : 'entries') +
      '  to the server.');
  var stanza = '<cli:iq to="remoting@bot.talk.google.com" type="set" ' +
      'xmlns:cli="jabber:client"><gr:log xmlns:gr="google:remoting">';
  while (this.pendingEntries.length > 0) {
    stanza += /** @type string */ this.pendingEntries.shift();
  }
  stanza += '</gr:log></cli:iq>';
  remoting.wcs.sendIq(stanza);
};

/**
 * Whether logging is enabled.
 *
 * @private
 * @return {boolean} whether logging is enabled
 */
remoting.LogToServer.prototype.isEnabled = function() {
  var value = window.localStorage.getItem(remoting.LogToServer.KEY_ENABLED_);
  return (value == 'true');
};

/**
 * Sets the session ID to a random string.
 *
 * @private
 */
remoting.LogToServer.prototype.setSessionId = function() {
  this.sessionId = remoting.LogToServer.generateSessionId();
  this.sessionIdGenerationTime = new Date().getTime();
};

/**
 * Clears the session ID.
 *
 * @private
 */
remoting.LogToServer.prototype.clearSessionId = function() {
  this.sessionId = '';
  this.sessionIdGenerationTime = 0;
};

/**
 * Sets a new session ID, if the current session ID has reached its maximum age.
 *
 * This method also logs the old and new session IDs to the server, in separate
 * log entries.
 *
 * @private
 */
remoting.LogToServer.prototype.maybeExpireSessionId = function() {
  if ((this.sessionId != '') &&
      (new Date().getTime() - this.sessionIdGenerationTime >=
      remoting.LogToServer.MAX_SESSION_ID_AGE)) {
    // Log the old session ID.
    var entry = remoting.ServerLogEntry.makeSessionIdOld(this.sessionId);
    this.log(entry);
    // Generate a new session ID.
    this.setSessionId();
    // Log the new session ID.
    entry = remoting.ServerLogEntry.makeSessionIdNew(this.sessionId);
    this.log(entry);
  }
};

/**
 * Generates a string that can be used as a session ID.
 *
 * @private
 * @return {string} a session ID
 */
remoting.LogToServer.generateSessionId = function() {
  var idArray = [];
  for (var i = 0; i < remoting.LogToServer.SESSION_ID_LEN_; i++) {
    var index =
        Math.random() * remoting.LogToServer.SESSION_ID_ALPHABET_.length;
    idArray.push(
        remoting.LogToServer.SESSION_ID_ALPHABET_.slice(index, index + 1));
  }
  return idArray.join('');
};
