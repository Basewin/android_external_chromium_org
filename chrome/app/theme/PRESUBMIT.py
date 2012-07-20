# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium theme resources.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl/git cl, and see
http://www.chromium.org/developers/web-development-style-guide for the rules
we're checking against here.
"""


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  resources = input_api.os_path.join(input_api.PresubmitLocalPath(),
      '../../../ui/resources')

  # List of paths with their associated scale factor. This is used to verify
  # that the images modified in one are the correct scale of the other.
  path_scales = [
    [(1, 'default_100_percent/'), (2, 'default_200_percent/')],
    [(1, 'touch_100_percent/'), (2, 'touch_200_percent/')],
  ]

  import sys
  old_path = sys.path

  try:
    sys.path = [resources] + old_path
    from resource_check import resource_scale_factors

    for paths in path_scales:
      results.extend(resource_scale_factors.ResourceScaleFactors(
          input_api, output_api, paths).RunChecks())
  finally:
    sys.path = old_path

  return results
