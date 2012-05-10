#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Auto-generates the WebGL conformance test list header file.

Parses the WebGL conformance test *.txt file, which contains a list of URLs
for individual conformance tests (each on a new line). It recursively parses
*.txt files. For each test URL, the matching gtest call is created and
sent to the C++ header file.
"""

import getopt
import os
import re
import sys

COPYRIGHT = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"""
WARNING = """\
// DO NOT EDIT! This file is auto-generated by
//   generate_webgl_conformance_test_list.py
// It is included by webgl_conformance_tests.cc

"""
HEADER_GUARD = """\
#ifndef CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_
#define CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_

"""
HEADER_GUARD_END = """
#endif  // CHROME_TEST_GPU_WEBGL_CONFORMANCE_TEST_LIST_AUTOGEN_H_

"""

# Assume this script is run from the src/chrome/test/gpu directory.
INPUT_DIR = "../../../third_party/webgl_conformance"
INPUT_FILE = "00_test_list.txt"
OUTPUT_FILE = "webgl_conformance_test_list_autogen.h"

def main(argv):
  """Main function for the WebGL conformance test list generator.
  """
  if not os.path.exists(os.path.join(INPUT_DIR, INPUT_FILE)):
    print >> sys.stderr, "ERROR: WebGL conformance tests do not exist."
    print >> sys.stderr, "Run the script from the directory containing it."
    return 1

  output = open(OUTPUT_FILE, "w")
  output.write(COPYRIGHT)
  output.write(WARNING)
  output.write(HEADER_GUARD)

  test_prefix = {}

  unparsed_files = [INPUT_FILE]
  while unparsed_files:
    filename = unparsed_files.pop(0)
    try:
      input = open(os.path.join(INPUT_DIR, filename))
    except IOError:
      print >> sys.stderr, "WARNING: %s does not exist (skipped)." % filename
      continue

    for url in input:
      url = re.sub("//.*", "", url)
      url = re.sub("#.*", "", url)
      url = url.strip()
      # Some filename has options before them, for example,
      # --min-version 1.0.2 testname.html
      pos = url.rfind(" ")
      if pos != -1:
        url = url[pos+1:]

      if not url:
        continue

      # Cannot use os.path.join() because Windows with use "\\" but this path
      # is sent through javascript.
      if os.path.dirname(filename):
        url = "%s/%s" % (os.path.dirname(filename), url)

      # Queue all text files for parsing, because test list URLs are nested
      # through .txt files.
      if re.match(".+\.txt\s*$", url):
        unparsed_files.append(url)

      # Convert the filename to a valid test name and output the gtest code.
      else:
        name = os.path.splitext(url)[0]
        name = re.sub("\W+", "_", name)
        if os.path.exists(os.path.join(INPUT_DIR, url)):
          output.write('CONFORMANCE_TEST(%s,\n  "%s");\n' % (name, url))
        else:
          print >> sys.stderr, "WARNING: %s does not exist (skipped)." % url
    input.close()

  output.write(HEADER_GUARD_END)
  output.close()
  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
