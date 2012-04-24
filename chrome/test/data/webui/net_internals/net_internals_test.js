// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The way these tests work is as follows:
 * C++ in net_internals_ui_browsertest.cc does any necessary setup, and then
 * calls the entry point for a test with RunJavascriptTest.  The called
 * function can then use the assert/expect functions defined in test_api.js.
 * All callbacks from the browser are wrapped in such a way that they can
 * also use the assert/expect functions.
 *
 * A test ends when testDone is called.  This can be done by the test itself,
 * but will also be done by the test framework when an assert/expect test fails
 * or an exception is thrown.
 */

// Include the C++ browser test class when generating *.cc files.
GEN('#include ' +
    '"chrome/browser/ui/webui/net_internals/net_internals_ui_browsertest.h"');

var NetInternalsTest = (function() {
  /**
   * A shorter poll interval is used for tests, since a few tests wait for
   * polled values to change.
   * @type {number}
   * @const
   */
  var TESTING_POLL_INTERVAL_MS = 50;

  /**
   * Private pointer to the currently active test framework.  Needed so static
   * functions can access some of the inner workings of the test framework.
   * @type {NetInternalsTest}
   */
  var activeTest_ = null;

  function NetInternalsTest() {
    activeTest_ = this;
  }

  NetInternalsTest.prototype = {
    __proto__: testing.Test.prototype,

    /**
     * Define the C++ fixture class and include it.
     * @type {?string}
     * @override
     */
    typedefCppFixture: 'NetInternalsTest',

    /** @inheritDoc */
    browsePreload: 'chrome://net-internals/',

    /** @inheritDoc */
    isAsync: true,

    setUp: function() {
      // Wrap g_browser.receive around a test function so that assert and expect
      // functions can be called from observers.
      g_browser.receive =
          this.continueTest(WhenTestDone.EXPECT,
                            BrowserBridge.prototype.receive.bind(g_browser));

      g_browser.setPollInterval(TESTING_POLL_INTERVAL_MS);

      var runTest = this.deferRunTest(WhenTestDone.EXPECT);

      // If we've already received the constants, start the tests.
      if (Constants) {
        // Stat test asynchronously, to avoid running a nested test function.
        window.setTimeout(runTest, 0);
        return;
      }

      // Otherwise, wait until we do.
      console.log('Received constants late.');

      /**
       * Observer that starts the tests once we've received the constants.
       */
      function ConstantsObserver() {
        this.testStarted_ = false;
      }

      ConstantsObserver.prototype.onReceivedConstants = function() {
        if (!this.testStarted_) {
          this.testStarted_ = true;
          // Stat test asynchronously, to avoid running a nested test function,
          // and so we don't call any constants observers used by individual
          // tests.
          window.setTimeout(runTest, 0);
        }
      };

      g_browser.addConstantsObserver(new ConstantsObserver());
    }
  };

  /**
   * A callback function for use by asynchronous Tasks that need a return value
   * from the NetInternalsTest::MessageHandler.  Must be null when no such
   * Task is running.  Set by NetInternalsTest.setCallback.  Automatically
   * cleared once called.  Must only be called through
   * NetInternalsTest::MessageHandler::RunCallback, from the browser process.
   */
  NetInternalsTest.callback = null;

  /**
   * Sets NetInternalsTest.callback.  Any arguments will be passed to the
   * callback function.
   * @param {function} callbackFunction Callback function to be called from the
   *     browser.
   */
  NetInternalsTest.setCallback = function(callbackFunction) {
    // Make sure no Task has already set the callback function.
    assertEquals(null, NetInternalsTest.callback);

    // Wrap |callbackFunction| in a function that clears
    // |NetInternalsTest.callback| before calling |callbackFunction|.
    var callbackFunctionWrapper = function() {
      NetInternalsTest.callback = null;
      callbackFunction.apply(null, Array.prototype.slice.call(arguments));
    };

    // Wrap |callbackFunctionWrapper| with test framework code.
    NetInternalsTest.callback =
        activeTest_.continueTest(WhenTestDone.EXPECT, callbackFunctionWrapper);
  };

  /**
   * Returns the first styled table body that's a descendent of |ancestorId|.
   * If the specified node is itself a table body node, just returns that node.
   * Returns null if no such node is found.
   * @param {string} ancestorId HTML element id containing a styled table.
   */
  NetInternalsTest.getStyledTableDescendent = function(ancestorId) {
    if ($(ancestorId).nodeName == 'TBODY')
      return $(ancestorId);
    // The tbody element of the first styled table in |parentId|.
    return document.querySelector('#' + ancestorId + ' .styled-table tbody');
  };

  /**
   * Finds the first styled table body that's a descendent of |ancestorId|,
   * including the |ancestorId| element itself, and returns the number of rows
   * it has. Returns -1 if there's no such table.
   * @param {string} ancestorId HTML element id containing a styled table.
   * @return {number} Number of rows the style table's body has.
   */
  NetInternalsTest.getStyledTableNumRows = function(ancestorId) {
    // The tbody element of the first styled table in |parentId|.
    var tbody = NetInternalsTest.getStyledTableDescendent(ancestorId);
    if (!tbody)
      return -1;
    return tbody.children.length;
  };

  /**
   * Finds the first styled table body that's a descendent of |ancestorId|,
   * including the |ancestorId| element itself, and checks if it has exactly
   * |expectedRows| rows.  As only table bodies are considered, the header row
   * will not be included in the count.
   * @param {string} ancestorId HTML element id containing a styled table.
   * @param {number} expectedRows Expected number of rows in the table.
   */
  NetInternalsTest.checkStyledTableRows = function(ancestorId, expectedRows) {
    expectEquals(expectedRows,
                 NetInternalsTest.getStyledTableNumRows(ancestorId),
                 'Incorrect number of rows in ' + ancestorId);
  };

  /**
   * Finds the first styled table body that's a descendent of |ancestorId|,
   * including the |ancestorId| element itself, and returns the text of the
   * specified cell.  If the cell does not exist, throws an exception.
   * @param {string} ancestorId HTML element id containing a styled table.
   * @param {number} row Row of the value to retrieve.
   * @param {number} column Column of the value to retrieve.
   */
  NetInternalsTest.getStyledTableText = function(ancestorId, row, column) {
    var tbody = NetInternalsTest.getStyledTableDescendent(ancestorId);
    return tbody.children[row].children[column].innerText;
  };

  /**
   * Returns the TabEntry with the given id.  Asserts if the tab can't be found.
   * @param {string}: tabId Id of the TabEntry to get.
   * @return {TabEntry} The specified TabEntry.
   */
  NetInternalsTest.getTab = function(tabId) {
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
    var tab = categoryTabSwitcher.findTabById(tabId);
    assertNotEquals(tab, undefined, tabId + ' does not exist.');
    return tab;
  };

  /**
   * Returns true if the node is visible.
   * @param {Node}: node Node to check the visibility state of.
   * @return {bool} Whether or not the node is visible.
   */
  NetInternalsTest.nodeIsVisible = function(node) {
    return node.style.display != 'none';
  };

  /**
   * Returns true if the specified tab's handle is visible, false otherwise.
   * Asserts if the handle can't be found.
   * @param {string}: tabId Id of the tab to check.
   * @return {bool} Whether or not the tab's handle is visible.
   */
  NetInternalsTest.tabHandleIsVisible = function(tabId) {
    var tabHandleNode = NetInternalsTest.getTab(tabId).getTabHandleNode();
    return NetInternalsTest.nodeIsVisible(tabHandleNode);
  };

  /**
   * Returns the tab id of a tab, given its associated URL hash value.  Asserts
   *     if |hash| has no associated tab.
   * @param {string}: hash Hash associated with the tab to return the id of.
   * @return {string} String identifier of the tab with the given hash.
   */
  NetInternalsTest.getTabId = function(hash) {
    /**
     * Map of tab handle names to location hashes.  Since the text fixture
     * must be runnable independent of net-internals, for generating the
     * test's cc files, must be careful to only create this map while a test
     * is running.
     * @type {object.<string, string>}
     */
    var hashToTabHandleIdMap = {
      capture: CaptureView.TAB_HANDLE_ID,
      export: ExportView.TAB_HANDLE_ID,
      import: ImportView.TAB_HANDLE_ID,
      proxy: ProxyView.TAB_HANDLE_ID,
      events: EventsView.TAB_HANDLE_ID,
      timeline: TimelineView.TAB_HANDLE_ID,
      dns: DnsView.TAB_HANDLE_ID,
      sockets: SocketsView.TAB_HANDLE_ID,
      spdy: SpdyView.TAB_HANDLE_ID,
      httpPipeline: HttpPipelineView.TAB_HANDLE_ID,
      httpCache: HttpCacheView.TAB_HANDLE_ID,
      serviceProviders: ServiceProvidersView.TAB_HANDLE_ID,
      tests: TestView.TAB_HANDLE_ID,
      hsts: HSTSView.TAB_HANDLE_ID,
      logs: LogsView.TAB_HANDLE_ID,
      prerender: PrerenderView.TAB_HANDLE_ID,
      chromeos: CrosView.TAB_HANDLE_ID
    };

    assertEquals(typeof hashToTabHandleIdMap[hash], 'string',
                 'Invalid tab anchor: ' + hash);
    var tabId = hashToTabHandleIdMap[hash];
    assertEquals('object', typeof NetInternalsTest.getTab(tabId),
                 'Invalid tab: ' + tabId);
    return tabId;
  };

  /**
   * Switches to the specified tab.
   * @param {string}: hash Hash associated with the tab to switch to.
   */
  NetInternalsTest.switchToView = function(hash) {
    var tabId = NetInternalsTest.getTabId(hash);

    // Make sure the tab handle is visible, as we only simulate normal usage.
    expectTrue(NetInternalsTest.tabHandleIsVisible(tabId),
               tabId + ' does not have a visible tab handle.');
    var tabHandleNode = NetInternalsTest.getTab(tabId).getTabHandleNode();

    // Simulate a left click.
    var mouseEvent = document.createEvent('MouseEvents');
    mouseEvent.initMouseEvent('click', true, true, window,
                              1, 0, 0, 0, 0,
                              false, false, false, false, 0, null);
    $(tabId).dispatchEvent(mouseEvent);

    // Make sure the hash changed.
    assertEquals('#' + hash, document.location.hash);

    // Run the onhashchange function, so we can test the resulting state.
    // Otherwise, won't trigger until after we return.
    window.onhashchange();

    // Make sure only the specified tab is visible.
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
    var tabIds = categoryTabSwitcher.getAllTabIds();
    for (var i = 0; i < tabIds.length; ++i) {
      expectEquals(tabIds[i] == tabId,
                   NetInternalsTest.getTab(tabIds[i]).contentView.isVisible(),
                   tabIds[i] + ': Unexpected visibility state.');
    }
  };

  /**
   * Checks the visibility of all tab handles against expected values.
   * @param {object.<string, bool>}: tabVisibilityState Object with a an entry
   *     for each tab's hash, and a bool indicating if it should be visible or
   *     not.
   * @param {bool+}: tourTabs True if tabs expected to be visible should should
   *     each be navigated to as well.
   */
  NetInternalsTest.checkTabHandleVisibility = function(tabVisibilityState,
                                                       tourTabs) {
    // Check visibility state of all tabs.
    var tabCount = 0;
    for (var hash in tabVisibilityState) {
      var tabId = NetInternalsTest.getTabId(hash);
      assertEquals('object', typeof NetInternalsTest.getTab(tabId),
                   'Invalid tab: ' + tabId);
      expectEquals(tabVisibilityState[hash],
                   NetInternalsTest.tabHandleIsVisible(tabId),
                   tabId + ' visibility state is unexpected.');
      if (tourTabs && tabVisibilityState[hash])
        NetInternalsTest.switchToView(hash);
      tabCount++;
    }

    // Check that every tab was listed.
    var categoryTabSwitcher = MainView.getInstance().categoryTabSwitcher();
    var tabIds = categoryTabSwitcher.getAllTabIds();
    expectEquals(tabCount, tabIds.length);
  };

  /**
   * This class allows multiple Tasks to be queued up to be run sequentially.
   * A Task can wait for asynchronous callbacks from the browser before
   * completing, at which point the next queued Task will be run.
   * @param {bool}: endTestWhenDone True if testDone should be called when the
   *     final task completes.
   * @param {NetInternalsTest}: test Test fixture passed to Tasks.
   * @constructor
   */
  NetInternalsTest.TaskQueue = function(endTestWhenDone) {
    // Test fixture for passing to tasks.
    this.tasks_ = [];
    this.isRunning_ = false;
    this.endTestWhenDone_ = endTestWhenDone;
  };

  NetInternalsTest.TaskQueue.prototype = {
    /**
     * Adds a Task to the end of the queue.  Each Task may only be added once
     * to a single queue.  Also passes the text fixture to the Task.
     * @param {Task}: task The Task to add.
     */
    addTask: function(task) {
      this.tasks_.push(task);
      task.setTaskQueue_(this);
    },

    /**
     * Adds a Task to the end of the queue.  The task will call the provided
     * function, and then complete.
     * @param {function}: taskFunction The function the task will call.
     */
    addFunctionTask: function(taskFunction) {
      this.addTask(new NetInternalsTest.CallFunctionTask(taskFunction));
    },

    /**
     * Starts running the Tasks in the queue, passing its arguments, if any,
     * to the first Task's start function.  May only be called once.
     */
    run: function() {
      assertFalse(this.isRunning_);
      this.isRunning_ = true;
      this.runNextTask_(Array.prototype.slice.call(arguments));
    },

    /**
     * If there are any Tasks in |tasks_|, removes the first one and runs it.
     * Otherwise, sets |isRunning_| to false.  If |endTestWhenDone_| is true,
     * calls testDone.
     * @param {array} argArray arguments to be passed on to next Task's start
     *     method.  May be a 0-length array.
     */
    runNextTask_: function(argArray) {
      assertTrue(this.isRunning_);
      // The last Task may have used |NetInternalsTest.callback|.  Make sure
      // it's now null.
      expectEquals(null, NetInternalsTest.callback);

      if (this.tasks_.length > 0) {
        var nextTask = this.tasks_.shift();
        nextTask.start.apply(nextTask, argArray);
      } else {
        this.isRunning_ = false;
        if (this.endTestWhenDone_)
          testDone();
      }
    }
  };

  /**
   * A Task that can be added to a TaskQueue.  A Task is started with a call to
   * the start function, and must call its own onTaskDone when complete.
   * @constructor
   */
  NetInternalsTest.Task = function() {
    this.taskQueue_ = null;
    this.isDone_ = false;
    this.completeAsync_ = false;
  };

  NetInternalsTest.Task.prototype = {
    /**
     * Starts running the Task.  Only called once per Task, must be overridden.
     * Any arguments passed to the last Task's onTaskDone, or to run (If the
     * first task) will be passed along.
     */
    start: function() {
      assertNotReached('Start function not overridden.');
    },

    /**
     * Updates value of |completeAsync_|.  If set to true, the next Task will
     * start asynchronously.  Useful if the Task completes on an event that
     * the next Task may care about.
     */
    setCompleteAsync: function(value) {
      this.completeAsync_ = value;
    },

    /**
     * @return {bool} True if this task has completed by calling onTaskDone.
     */
    isDone: function() {
      return this.isDone_;
    },

    /**
     * Sets the TaskQueue used by the task in the onTaskDone function.  May only
     * be called by the TaskQueue.
     * @param {TaskQueue}: taskQueue The TaskQueue |this| has been added to.
     */
    setTaskQueue_: function(taskQueue) {
      assertEquals(null, this.taskQueue_);
      this.taskQueue_ = taskQueue;
    },

    /**
     * Must be called when a task is complete, and can only be called once for a
     * task.  Runs the next task, if any, passing along all arguments.
     */
    onTaskDone: function() {
      assertFalse(this.isDone_);
      this.isDone_ = true;

      // Function to run the next task in the queue.
      var runNextTask = this.taskQueue_.runNextTask_.bind(
                            this.taskQueue_,
                            Array.prototype.slice.call(arguments));

      // If we need to start the next task asynchronously, we need to wrap
      // it with the test framework code.
      if (this.completeAsync_) {
        window.setTimeout(activeTest_.continueTest(WhenTestDone.EXPECT,
                                                   runNextTask),
                          0);
        return;
      }

      // Otherwise, just run the next task directly.
      runNextTask();
    },
  };

  /**
   * A Task that can be added to a TaskQueue.  A Task is started with a call to
   * the start function, and must call its own onTaskDone when complete.
   * @extends {NetInternalsTest.Task}
   * @constructor
   */
  NetInternalsTest.CallFunctionTask = function(taskFunction) {
    NetInternalsTest.Task.call(this);
    assertEquals('function', typeof taskFunction);
    this.taskFunction_ = taskFunction;
  };

  NetInternalsTest.CallFunctionTask.prototype = {
    __proto__: NetInternalsTest.Task.prototype,

    /**
     * Runs the function and then completes.  Passes all arguments, if any,
     * along to the function.
     */
    start: function() {
      this.taskFunction_.apply(null, Array.prototype.slice.call(arguments));
      this.onTaskDone();
    }
  };

  /**
   * A Task that converts the given path into a URL served by the TestServer.
   * The resulting URL will be passed to the next task.  Will also start the
   * TestServer, if needed.
   * @param {string} path Path to convert to a URL.
   * @extends {NetInternalsTest.Task}
   * @constructor
   */
  NetInternalsTest.GetTestServerURLTask = function(path) {
    NetInternalsTest.Task.call(this);
    assertEquals('string', typeof path);
    this.path_ = path;
  };

  NetInternalsTest.GetTestServerURLTask.prototype = {
    __proto__: NetInternalsTest.Task.prototype,

    /**
     * Sets |NetInternals.callback|, and sends the path to the browser process.
     */
    start: function() {
      NetInternalsTest.setCallback(this.onURLReceived_.bind(this));
      chrome.send('getTestServerURL', [this.path_]);
    },

    /**
     * Completes the Task, passing the url on to the next Task.
     * @param {string} url TestServer URL of the input path.
     */
    onURLReceived_: function(url) {
      assertEquals('string', typeof url);
      this.onTaskDone(url);
    }
  };

  /**
   * A Task that creates an incognito window and only completes once it has
   * navigated to about:blank.  The waiting is required to avoid reentrancy
   * issues, since the function to create the incognito browser also waits
   * for the navigation to complete.  May not be called if there's already an
   * incognito browser in existence.
   * @extends {NetInternalsTest.Task}
   * @constructor
   */
  NetInternalsTest.CreateIncognitoBrowserTask = function() {
    NetInternalsTest.Task.call(this);
  };

  NetInternalsTest.CreateIncognitoBrowserTask.prototype = {
    __proto__: NetInternalsTest.Task.prototype,

    /**
     * Tells the browser process to create an incognito browser, and sets
     * up a callback to be called on completion.
     */
    start: function() {
      // Reuse the BrowserBridge's callback mechanism, since it's already
      // wrapped in our test harness.
      assertEquals('undefined',
                   typeof g_browser.onIncognitoBrowserCreatedForTest);
      g_browser.onIncognitoBrowserCreatedForTest =
          this.onIncognitoBrowserCreatedForTest.bind(this);

      chrome.send('createIncognitoBrowser');
    },

    /**
     * Deletes the callback function, and completes the task.
     */
    onIncognitoBrowserCreatedForTest: function() {
      delete g_browser.onIncognitoBrowserCreatedForTest;
      this.onTaskDone();
    }
  };

  /**
   * Returns a task that closes an incognito window created with the task
   * above.  May only be called if there's an incognito window created by
   * the above function that has yet to be closed.  Returns immediately.
   * @return {Task} Task that closes incognito browser window.
   */
  NetInternalsTest.getCloseIncognitoBrowserTask = function() {
    return new NetInternalsTest.CallFunctionTask(
        function() {
          chrome.send('closeIncognitoBrowser');
        });
  };

  /**
   * Returns true if a node does not have a 'display' property of 'none'.
   * @param {node}: node The node to check.
   */
  NetInternalsTest.isDisplayed = function(node) {
    var style = getComputedStyle(node);
    return style.getPropertyValue('display') != 'none';
  };

  /**
   * Creates a new NetLog source.  Note that the id may conflict with events
   * received from the browser.
   * @param {int}: type The source type.
   * @param {int}: id The source id.
   * @constructor
   */
  NetInternalsTest.Source = function(type, id) {
    assertNotEquals(getKeyWithValue(LogSourceType, type), '?');
    assertGE(id, 0);
    this.type = type;
    this.id = id;
  };

  /**
   * Creates a new NetLog event.
   * @param {Source}: source The source associated with the event.
   * @param {int}: type The event id.
   * @param {int}: time When the event occurred.
   * @param {int}: phase The event phase.
   * @param {object}: params The event parameters.  May be null.
   * @constructor
   */
  NetInternalsTest.Event = function(source, type, time, phase, params) {
    assertNotEquals(getKeyWithValue(LogEventType, type), '?');
    assertNotEquals(getKeyWithValue(LogEventPhase, phase), '?');

    this.source = source;
    this.phase = phase;
    this.type = type;
    this.time = '' + time;
    this.phase = phase;
    if (params)
      this.params = params;
  };

  /**
   * Creates a new NetLog begin event.  Parameters are the same as Event,
   * except there's no |phase| argument.
   * @see Event
   */
  NetInternalsTest.createBeginEvent = function(source, type, time, params) {
    return new NetInternalsTest.Event(source, type, time,
                                      LogEventPhase.PHASE_BEGIN, params);
  };

  /**
   * Creates a new NetLog end event.  Parameters are the same as Event,
   * except there's no |phase| argument.
   * @see Event
   */
  NetInternalsTest.createEndEvent = function(source, type, time, params) {
    return new NetInternalsTest.Event(source, type, time,
                                      LogEventPhase.PHASE_END, params);
  };

  /**
   * Creates a new NetLog end event matching the given begin event.
   * @param {Event}: beginEvent The begin event.  Returned event will have the
   *                 same source and type.
   * @param {int}: time When the event occurred.
   * @param {object}: params The event parameters.  May be null.
   * @see Event
   */
  NetInternalsTest.createMatchingEndEvent = function(beginEvent, time, params) {
    return NetInternalsTest.createEndEvent(
               beginEvent.source, beginEvent.type, time, params);
  };

  /**
   * Checks that only the given status view node is visible.
   * @param {string}: nodeId ID of the node that should be visible.
   */
  NetInternalsTest.expectStatusViewNodeVisible = function(nodeId) {
    var allIds = [
      CaptureStatusView.MAIN_BOX_ID,
      LoadedStatusView.MAIN_BOX_ID,
      HaltedStatusView.MAIN_BOX_ID
    ];

    for (var i = 0; i < allIds.length; ++i) {
      var curId = allIds[i];
      expectEquals(nodeId == curId, NetInternalsTest.nodeIsVisible($(curId)));
    }
  };

  return NetInternalsTest;
})();
