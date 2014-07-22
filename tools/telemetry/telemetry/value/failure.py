# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import traceback

from telemetry import value as value_module

class FailureValue(value_module.Value):

  def __init__(self, page, exc_info):
    """A value representing a failure when running the page.

    Args:
      page: The page where this failure occurs.
      exc_info: The exception info (sys.exc_info()) corresponding to
          this failure.
    """
    exc_type = exc_info[0].__name__
    super(FailureValue, self).__init__(page, exc_type, '', True)
    self._exc_info = exc_info

  @classmethod
  def FromMessage(cls, page, message):
    """Creates a failure value for a given string message.

    Args:
      page: The page where this failure occurs.
      message: A string message describing the failure.
    """
    try:
      raise Exception(message)
    except Exception:
      return FailureValue(page, sys.exc_info())

  def __repr__(self):
    if self.page:
      page_name = self.page.url
    else:
      page_name = None
    return 'FailureValue(%s, %s)' % (
        page_name, GetStringFromExcInfo(self._exc_info))

  @property
  def exc_info(self):
    return self._exc_info

  def GetBuildbotDataType(self, output_context):
    return None

  def GetBuildbotValue(self):
    return None

  def GetBuildbotMeasurementAndTraceNameForPerPageResult(self):
    return None

  def GetRepresentativeNumber(self):
    return None

  def GetRepresentativeString(self):
    return None

  @classmethod
  def GetJSONTypeName(cls):
    return 'failure'

  def AsDict(self):
    d = super(FailureValue, self).AsDict()
    d['value'] = GetStringFromExcInfo(self.exc_info)
    return d

  @classmethod
  def MergeLikeValuesFromSamePage(cls, values):
    assert False, 'Should not be called.'

  @classmethod
  def MergeLikeValuesFromDifferentPages(cls, values,
                                        group_by_name_suffix=False):
    assert False, 'Should not be called.'

def GetStringFromExcInfo(exc_info):
  return ''.join(traceback.format_exception(*exc_info))
