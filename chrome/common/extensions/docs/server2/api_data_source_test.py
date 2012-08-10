#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import unittest

from file_system import FileNotFoundError
from file_system_cache import FileSystemCache
from local_file_system import LocalFileSystem
from api_data_source import APIDataSource

class FakeSamplesDataSource:
  def Create(self, request):
    return {}

class APIDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join('test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def testSimple(self):
    cache_builder = FileSystemCache.Builder(LocalFileSystem(self._base_path))
    data_source_factory = APIDataSource.Factory(cache_builder,
                                                '.',
                                                FakeSamplesDataSource())
    data_source = data_source_factory.Create({})

    # Take the dict out of the list.
    expected = json.loads(self._ReadLocalFile('expected_test_file.json'))
    expected['permissions'] = None
    self.assertEqual(expected, data_source['test_file'])
    self.assertEqual(expected, data_source['testFile'])
    self.assertEqual(expected, data_source['testFile.html'])
    self.assertRaises(FileNotFoundError, data_source.get, 'junk')

if __name__ == '__main__':
  unittest.main()
