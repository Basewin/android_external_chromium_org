#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import os
import random

NOT_AVAILABLE_STRING = 'not available'


class MediaTestMatrix:
  """This class reads video test matrix file and stores them.

  Video test matrix contains different video titles as column and different
  video codec (such as "webm" as columns. The matrix entry contains video
  specification such as "320x240 50 fps VP8 / no audio 1 MBit/s PSNR 36.70"
  or "not available". It also provides methods to generate information based
  on the client's needs. For example, "video" tag or "audio" tag is returned
  based on what kinds of media it is. This matrix is for managing different
  kinds of media files in media testing.

  TODO(imasaki@chromium.org): remove the code duplication in Generate methods.
  """
  # Video extensions such as "webm".
  _exts = []
  # Video specification dictionary of list of video descriptions
  # (the key is video title).
  _specs = {}
  # Video titles such as "bear".
  _video_titles = []
  # Tag dictionary (the key is extension).
  _tags = {}

  def ReadData(self, csv_file):
    """Reads CSV file and stores in list specs and extensions.

    CSV file should have the following format:
    "ext","tag","title1","title2",....
    "0.webm", "video","description1","descrption2", "not available"
    (when it is not available).

    Args:
      csv_file : CSV file name contains video matrix information described
                 above.

    Raises:
      csv.Error if the number of columns is not the same as the number of
        tiles.
    """
    # Clean up all data.
    self._exts = []
    self._specs = {}
    self._video_titles = []
    self._tags = {}
    file = open(csv_file, 'rb')
    reader = csv.reader(file)
    for row_counter, row in enumerate(reader):
      if row_counter == 0:
        # First row is comment. So, skip it.
        pass
      elif row_counter == 1:
        # Second row is for header (video titles).
        for column_counter, title in enumerate(row[1:]):
          # Skip the first column since it is for tag ('video' or 'audio').
          if column_counter > 0:
            self._video_titles.append(title)
          self._specs[title] = []
      else:
        # Error checking is done here based on the number of titles
        if len(self._video_titles) != len(row) - 2:
          print "Row %d should have %d columns but has %d columns" % (
              row_counter, len(self._video_titles), len(row))
          raise csv.Error
        # First column should contain extension.
        self._exts.append(row[0])
        # Second column should contain tag (audio or video).
        self._tags[row[0]] = row[1]
        for i in range(len(row) - 2):
          self._specs[self._video_titles[i]].append(row[i + 2])
    file.close()

  def _GenerateMediaInfo(self, sub_type, name, media_test_matrix_home_url):
    """Generate media information from matrix generated by CSV file.

    Args:
      sub_type: index of the extensions (rows in the CSV file).
      name: the name of the video file (column name in the CSV file).
      media_test_matrix_home_url: url of the matrix home that used
        to generate link.

    Returns:
      a tuple of (info, url, link, tag, is_video, nickname).
        info: description of the video (an entry in the matrix).
        url: URL of the video or audio.
        link: href tag for video or audio.
        tag: a tag string can be used to display video or audio.
        is_video: True if the file is video; False otherwise.
        nickname: nickname of the video for presentation.
    """
    if name is 'none':
      return
    cur_video = name
    is_video = self._tags[self._exts[sub_type]] == 'video'
    file = os.path.join(cur_video, cur_video + self._exts[sub_type])
    # Specs were generated from CSV file in readFile().
    info = self._specs[cur_video][sub_type]
    url = media_test_matrix_home_url + file
    link = '<b><a href="%s">%s</a>&nbsp;&nbsp;%s</b>' % (url, file, info)
    type = ['audio', 'video'][is_video]
    tag_attr = 'id="v" controls autoplay playbackRate=1 loop valign=top'
    tag = '<%s %s src="%s">' % (type, tag_attr, file)
    nickname = cur_video + self._exts[sub_type]
    return (info, url, link, tag, is_video, nickname)

  def GenerateRandomMediaInfo(self, number_of_tries=10,
                              video_matrix_home_url=''):
    """Generate random video info that can be used for playing this media.

    Args:
      video_only: True if generate random video only.
      video_matrix_home_url: url of the matrix home that is used
        to generate link.

    Returns:
      a list of a tuples (info, url, link, tag, is_video, nickname).
        info: description of the video (an entry in the matrix).
        url: URL of the video/audio.
        link: href tag for video or audio.
        tag: a tag string can be used to display video or audio.
        is_video: True if the file is video; False otherwise.
        nickname: nickname of the video for presentation.
    """
    # Try number_of_tries times to find available video/audio.
    for i in range(number_of_tries):
      sub_type = random.randint(0, len(self._exts))
      name = self._video_titles[random.randint(0, len(self._video_titles)-1)]
      (info, url, link, tag, is_video, nickname) = (
          self._GenerateMediaInfo(sub_type, name, video_matrix_home_url))
      if NOT_AVAILABLE_STRING not in info:
        return (info, url, link, tag, is_video, nickname)
    # Gives up after that (very small chance).
    return None

  def GenerateAllMediaInfos(self, video_only, media_matrix_home_url=''):
    """Generate all video infos that can be used for playing this media.

    Args:
      video_only: True if generate random video only (not audio).
      media_matrix_home_url: url of the matrix home that is used
        to generate link.
    Returns:
      a list of a tuples (info, url, link, tag, is_video, nickname).
        info: description of the video (an entry in the matrix).
        url: URL of the video/audio.
        link: href tag for video or audio.
        tag: a tag string can be used to display video or audio.
        is_video: True if the file is video; False otherwise.
        nickname: nickname of the video for presentation (such as bear.webm).
    """
    media_infos = []

    for i in range(0, len(self._video_titles)-1):
      name = self._video_titles[i]
      for sub_type in range(len(self._exts)):
        (info, url, link, tag, is_video, nickname) = (
            self._GenerateMediaInfo(sub_type, name, media_matrix_home_url))
        if ((NOT_AVAILABLE_STRING not in info) and
            ((not video_only) or (video_only and is_video))):
          media_infos.append([info, url, link, tag, is_video, nickname])
    return media_infos

  def GenerateAllMediaInfosInCompactForm(self, video_only,
                                         media_matrix_home_url=''):
    """Generate all media information in compact form.

    Compact form contains only url, nickname and tag.

    Args:
      video_only: True if generate random video only.
      media_matrix_home_url: url of the matrix home that is used
        to generate link.

    Returns:
      a list of a tuples (url, nickname, tag).
        url: URL of the video/audio.
        nickname: nickname of the video/audio for presentation
                  (such as bear.webm).
        tag: HTML5 tag for the video/audio.
    """
    media_infos = []
    for i in range(0, len(self._video_titles)-1):
      name = self._video_titles[i]
      for sub_type in range(len(self._exts)):
        (info, url, link, tag, is_video, nickname) = (
            self._GenerateMediaInfo(sub_type, name, media_matrix_home_url))
        tag = ['audio', 'video'][is_video]
        if ((not NOT_AVAILABLE_STRING in info) and
            ((not video_only) or (video_only and is_video))):
          media_infos.append([tag, url, nickname])
    return media_infos

  @staticmethod
  def LookForMediaInfoInCompactFormByNickName(compact_list, target):
    """Look for video by its nickname in the compact_list.

    Args:
      compact_list: a list generated by GenerateAllMediaInfosInCompactForm.
      target: a target nickname string to look for.

    Returns:
      A tuple (url, nickname, tag) where nickname is a target string.
        url: URL of the video/audio.
        nickname: nickname of the video/audio for presentation
                  (such as bear.webm).
        tag: HTML5 tag for the video/audio.
    """
    for tag, url, nickname in compact_list:
      if target == nickname:
        return (tag, url, nickname)
