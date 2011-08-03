// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays the progress and results from the "connection tester".
 *
 *   - Has an input box to specify the URL.
 *   - Has a button to start running the tests.
 *   - Shows the set of experiments that have been run so far, and their
 *     result.
 */

var TestView = (function() {
  // IDs for special HTML elements in test_view.html
  const MAIN_BOX_ID = 'test-view-tab-content';
  const URL_INPUT_ID = 'test-view-url-input';
  const FORM_ID = 'test-view-connection-tests-form';
  const SUMMARY_DIV_ID = 'test-view-summary';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function TestView() {
    // Call superclass's constructor.
    superClass.call(this, MAIN_BOX_ID);

    this.urlInput_ = $(URL_INPUT_ID);
    this.summaryDiv_ = $(SUMMARY_DIV_ID);

    var form = $(FORM_ID);
    form.addEventListener('submit', this.onSubmitForm_.bind(this), false);

    // Register to test information as it's received.
    g_browser.addConnectionTestsObserver(this);
  }

  cr.addSingletonGetter(TestView);

  TestView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onSubmitForm_: function(event) {
      g_browser.sendStartConnectionTests(this.urlInput_.value);
      event.preventDefault();
    },

    /**
     * Callback for when the connection tests have begun.
     */
    onStartedConnectionTestSuite: function() {
      this.summaryDiv_.innerHTML = '';

      var p = addNode(this.summaryDiv_, 'p');
      addTextNode(p, 'Started connection test suite suite on ' +
                     (new Date()).toLocaleString());

      // Add a table that will hold the individual test results.
      var table = addNode(this.summaryDiv_, 'table');
      table.className = 'styledTable';
      var thead = addNode(table, 'thead');
      thead.innerHTML = '<tr><th>Result</th><th>Experiment</th>' +
                        '<th>Error</th><th>Time (ms)</th></tr>';

      this.tbody_ = addNode(table, 'tbody');
    },

    /**
     * Callback for when an individual test in the suite has begun.
     */
    onStartedConnectionTestExperiment: function(experiment) {
      var tr = addNode(this.tbody_, 'tr');

      var passFailCell = addNode(tr, 'td');

      var experimentCell = addNode(tr, 'td');

      var resultCell = addNode(tr, 'td');
      addTextNode(resultCell, '?');

      var dtCell = addNode(tr, 'td');
      addTextNode(dtCell, '?');

      // We will fill in result cells with actual values (to replace the
      // placeholder '?') once the test has completed. For now we just
      // save references to these cells.
      this.currentExperimentRow_ = {
        experimentCell: experimentCell,
        dtCell: dtCell,
        resultCell: resultCell,
        passFailCell: passFailCell,
        startTime: (new Date()).getTime()
      };

      addTextNode(experimentCell, 'Fetch ' + experiment.url);

      if (experiment.proxy_settings_experiment ||
          experiment.host_resolver_experiment) {
        var ul = addNode(experimentCell, 'ul');

        if (experiment.proxy_settings_experiment) {
          var li = addNode(ul, 'li');
          addTextNode(li, experiment.proxy_settings_experiment);
        }

        if (experiment.host_resolver_experiment) {
          var li = addNode(ul, 'li');
          addTextNode(li, experiment.host_resolver_experiment);
        }
      }
    },

    /**
     * Callback for when an individual test in the suite has finished.
     */
    onCompletedConnectionTestExperiment: function(experiment, result) {
      var r = this.currentExperimentRow_;

      var endTime = (new Date()).getTime();

      r.dtCell.innerHTML = '';
      addTextNode(r.dtCell, (endTime - r.startTime));

      r.resultCell.innerHTML = '';

      if (result == 0) {
        r.passFailCell.style.color = 'green';
        addTextNode(r.passFailCell, 'PASS');
      } else {
        addTextNode(r.resultCell,
                    getKeyWithValue(NetError, result) + ' (' + result + ')');
        r.passFailCell.style.color = 'red';
        addTextNode(r.passFailCell, 'FAIL');
      }

      this.currentExperimentRow_ = null;
    },

    /**
     * Callback for when the last test in the suite has finished.
     */
    onCompletedConnectionTestSuite: function() {
      var p = addNode(this.summaryDiv_, 'p');
      addTextNode(p, 'Completed connection test suite suite');
    }
  };

  return TestView;
})();
