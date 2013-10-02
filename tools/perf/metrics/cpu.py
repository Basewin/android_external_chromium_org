# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric

class CpuMetric(Metric):
  """Calulates CPU load over a span of time."""

  def __init__(self, browser):
    super(CpuMetric, self).__init__()
    self._results = None
    self._browser = browser
    self._start_cpu = None

  def DidStartBrowser(self, browser):
    # Save the browser object so that cpu_stats can be accessed later.
    self._browser = browser

  def Start(self, page, tab):
    self._start_cpu = self._browser.cpu_stats

  def Stop(self, page, tab):
    assert self._start_cpu, 'Must call Start() first'
    self._results = _SubtractCpuStats(self._browser.cpu_stats, self._start_cpu)

  # Optional argument trace_name is not in base class Metric.
  # pylint: disable=W0221
  def AddResults(self, tab, results, trace_name='cpu_utilization'):
    assert self._results, 'Must call Stop() first'
    # Add a result for each process type.
    for process_type in self._results:
      trace_name = '%s_%s' % (trace_name, process_type.lower())
      cpu_percent = 100 * self._results[process_type]
      results.Add(trace_name, '%', cpu_percent, chart_name='cpu_utilization',
                  data_type='unimportant')


def _SubtractCpuStats(cpu_stats, start_cpu_stats):
  """Computes average cpu usage over a time period for different process types.

  Each of the two cpu_stats arguments is a dict with the following format:
      {'Browser': {'CpuProcessTime': ..., 'TotalTime': ...},
       'Renderer': {'CpuProcessTime': ..., 'TotalTime': ...}
       'Gpu': {'CpuProcessTime': ..., 'TotalTime': ...}}

  The 'CpuProcessTime' fields represent the number of seconds of CPU time
  spent in each process, and total time is the number of real seconds
  that have passed (this may be a Unix timestamp).

  Returns:
    A dict of process type names (Browser, Renderer, etc.) to ratios of cpu
    time used to total time elapsed.
  """
  cpu_usage = {}
  for process_type in cpu_stats:
    assert process_type in start_cpu_stats, 'Mismatching process types'
    # Skip any process_types that are empty.
    if (not cpu_stats[process_type]) or (not start_cpu_stats[process_type]):
      continue
    cpu_process_time = (cpu_stats[process_type]['CpuProcessTime'] -
                        start_cpu_stats[process_type]['CpuProcessTime'])
    total_time = (cpu_stats[process_type]['TotalTime'] -
                  start_cpu_stats[process_type]['TotalTime'])
    assert total_time > 0
    cpu_usage[process_type] = float(cpu_process_time) / total_time
  return cpu_usage

