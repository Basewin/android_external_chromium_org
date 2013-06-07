// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_experiment_util.h"

#include <vector>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time.h"

namespace installer {

namespace {

const char* const kDays[] =
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

const char* const kMonths[] =
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
      "Oct", "Nov", "Dec"};

}

string16 BuildExperimentDateString() {
  // The Google Update experiment_labels timestamp format is:
  // "DAY, DD0 MON YYYY HH0:MI0:SE0 TZ"
  //  DAY = 3 character day of week,
  //  DD0 = 2 digit day of month,
  //  MON = 3 character month of year,
  //  YYYY = 4 digit year,
  //  HH0 = 2 digit hour,
  //  MI0 = 2 digit minute,
  //  SE0 = 2 digit second,
  //  TZ = 3 character timezone
  base::Time::Exploded then = {};
  base::Time::Now().UTCExplode(&then);
  then.year += 1;
  DCHECK(then.HasValidValues());

  return UTF8ToUTF16(base::StringPrintf("%s, %02d %s %d %02d:%02d:%02d GMT",
                                        kDays[then.day_of_week],
                                        then.day_of_month,
                                        kMonths[then.month - 1],
                                        then.year,
                                        then.hour,
                                        then.minute,
                                        then.second));
}

}  // namespace installer

