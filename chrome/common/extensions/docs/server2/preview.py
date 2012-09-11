#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This helps you preview the apps and extensions docs.
#
#    ./preview.py --help
#
# There are two modes: server- and render- mode. The default is server, in which
# a webserver is started on a port (default 8000). Navigating to paths on
# http://localhost:8000, for example
#
#     http://localhost:8000/extensions/tabs.html
#
# will render the documentation for the extension tabs API.
#
# On the other hand, render mode statically renders docs to stdout. Use this
# to save the output (more convenient than needing to save the page in a
# browser), handy when uploading the docs somewhere (e.g. for a review),
# and for profiling the server. For example,
#
#    ./preview.py -r extensions/tabs.html
#
# will output the documentation for the tabs API on stdout and exit immediately.
#
# Note: absolute paths into static content (e.g. /static/css/site.css) will be
# relative paths (e.g. static/css/site.css) for convenient sandboxing.

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import optparse
import os
import shutil
from StringIO import StringIO
import sys
import urlparse

import build_server
# Copy all the files necessary to run the server. These are cleaned up when the
# server quits.
build_server.main()

from fake_fetchers import ConfigureFakeFetchers

class _Response(object):
  def __init__(self):
    self.status = 200
    self.out = StringIO()
    self.headers = {}

  def set_status(self, status):
    self.status = status

class _Request(object):
  def __init__(self, path):
    self.headers = {}
    self.path = path
    self.url = 'http://localhost' + path

def _Render(path, local_path):
  response = _Response()
  Handler(_Request(urlparse.urlparse(path).path),
          response,
          local_path=local_path).get()
  content = response.out.getvalue()
  if isinstance(content, unicode):
    content = content.encode('utf-8', 'replace')
  return (content, response.status)

def _GetLocalPath():
  return os.path.join(sys.argv[0].rsplit('/', 1)[0], os.pardir, os.pardir)

class RequestHandler(BaseHTTPRequestHandler):
  """A HTTPRequestHandler that outputs the docs page generated by Handler.
  """
  def do_GET(self):
    (content, status) = _Render(self.path, RequestHandler.local_path)
    self.send_response(status)
    self.end_headers()
    self.wfile.write(content)

if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Runs a server to preview the extension documentation.',
      usage='usage: %prog [option]...')
  parser.add_option('-p', '--port', default="8000",
      help='port to run the server on')
  parser.add_option('-d', '--directory', default=_GetLocalPath(),
      help='extensions directory to serve from - '
           'should be chrome/common/extensions within a Chromium checkout')
  parser.add_option('-r', '--render', default="",
      help='statically render a page and print to stdout rather than starting '
           'the server, e.g. apps/storage.html. The path may optionally end '
           'with #n where n is the number of times to render the page before '
           'printing it, e.g. apps/storage.html#50, to use for profiling.')

  (opts, argv) = parser.parse_args()

  if (not os.path.isdir(opts.directory) or
      not os.path.isdir(os.path.join(opts.directory, 'docs')) or
      not os.path.isdir(os.path.join(opts.directory, 'api'))):
    print('Specified directory does not exist or does not contain extension '
          'docs.')
    exit()

  ConfigureFakeFetchers(os.path.join(opts.directory, 'docs', 'server2'))
  from handler import Handler

  if opts.render:
    if opts.render.find('#') >= 0:
      (path, iterations) = opts.render.rsplit('#', 1)
      extra_iterations = int(iterations) - 1
    else:
      path = opts.render
      extra_iterations = 0

    (content, status) = _Render(path, opts.directory)
    if status != 200:
      print('Error status: %s' % status)
      exit(1)

    for _ in range(extra_iterations):
      _Render(path, opts.directory)

    # Static paths will show up as /stable/static/foo but this only makes sense
    # from a webserver.
    print(content.replace('/stable/static', 'static'))
    exit()

  print('Starting previewserver on port %s' % opts.port)
  print('Reading from %s' % opts.directory)
  print('')
  print('The extension documentation can be found at:')
  print('')
  print('  http://localhost:%s' % opts.port)
  print('')

  RequestHandler.local_path = opts.directory
  server = HTTPServer(('', int(opts.port)), RequestHandler)
  try:
    server.serve_forever()
  finally:
    server.socket.close()
