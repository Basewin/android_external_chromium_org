# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Holds the constants for pretty printing histograms.xml."""

import os
import sys

# Import the metrics/common module for pretty print xml.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import pretty_print_xml

# Desired order for tag attributes; attributes listed here will appear first,
# and in the same order as in these lists.
# { tag_name: [attribute_name, ...] }
ATTRIBUTE_ORDER = {
  'enum': ['name', 'type'],
  'histogram': ['name', 'enum', 'units'],
  'int': ['value', 'label'],
  'fieldtrial': ['name', 'separator', 'ordering'],
  'group': ['name', 'label'],
  'affected-histogram': ['name'],
  'with-group': ['name'],
}

# Tag names for top-level nodes whose children we don't want to indent.
TAGS_THAT_DONT_INDENT = [
  'histogram-configuration',
  'histograms',
  'fieldtrials',
  'enums'
]

# Extra vertical spacing rules for special tag names.
# {tag_name: (newlines_after_open, newlines_before_close, newlines_after_close)}
TAGS_THAT_HAVE_EXTRA_NEWLINE = {
  'histogram-configuration': (2, 1, 1),
  'histograms': (2, 1, 1),
  'fieldtrials': (2, 1, 1),
  'enums': (2, 1, 1),
  'histogram': (1, 1, 1),
  'enum': (1, 1, 1),
  'fieldtrial': (1, 1, 1),
}

# Tags that we allow to be squished into a single line for brevity.
TAGS_THAT_ALLOW_SINGLE_LINE = [
  'summary',
  'int',
]


