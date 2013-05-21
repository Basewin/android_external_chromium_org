// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_property_ui_data.h"

#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class NetworkPropertyUIDataTest : public testing::Test {
 protected:
  NetworkPropertyUIDataTest() {}
  virtual ~NetworkPropertyUIDataTest() {}

  void CheckProperty(const NetworkPropertyUIData& property,
                     const base::Value* expected_default_value,
                     bool expected_managed,
                     bool expected_editable) {
    if (expected_default_value) {
      EXPECT_TRUE(base::Value::Equals(expected_default_value,
                                      property.default_value()));
    } else {
      EXPECT_FALSE(property.default_value());
    }
    EXPECT_EQ(expected_managed, property.IsManaged());
    EXPECT_EQ(expected_editable, property.IsEditable());
  }
};

TEST_F(NetworkPropertyUIDataTest, PropertyInit) {
  NetworkPropertyUIData empty_prop;
  CheckProperty(empty_prop, NULL, false, true);

  NetworkPropertyUIData null_prop(onc::ONC_SOURCE_NONE);
  CheckProperty(null_prop, NULL, false, true);
}

TEST_F(NetworkPropertyUIDataTest, ParseOncProperty) {
  base::DictionaryValue onc;

  base::StringValue val_a("a");
  base::StringValue val_b("b");
  base::StringValue val_a_a("a_a");
  base::StringValue val_a_b("a_b");

  onc.Set("a", val_a.DeepCopy());
  onc.Set("b", val_b.DeepCopy());
  onc.Set("a.a", val_a_a.DeepCopy());
  onc.Set("a.b", val_a_b.DeepCopy());
  base::ListValue recommended;
  recommended.AppendString("b");
  recommended.AppendString("c");
  recommended.AppendString("a.a");
  onc.Set("Recommended", recommended.DeepCopy());
  onc.Set("a.Recommended", recommended.DeepCopy());

  NetworkPropertyUIData prop;

  prop.ParseOncProperty(onc::ONC_SOURCE_NONE, &onc, "a");
  CheckProperty(prop, NULL, false, true);

  onc::ONCSource source = onc::ONC_SOURCE_USER_IMPORT;

  prop.ParseOncProperty(source, &onc, "a");
  CheckProperty(prop, NULL, false, true);

  prop.ParseOncProperty(source, &onc, "a.b");
  CheckProperty(prop, NULL, false, true);

  prop.ParseOncProperty(source, &onc, "c");
  CheckProperty(prop, NULL, false, true);

  source = onc::ONC_SOURCE_USER_POLICY;

  prop.ParseOncProperty(source, &onc, "a");
  CheckProperty(prop, NULL, true, false);

  prop.ParseOncProperty(source, &onc, "b");
  CheckProperty(prop, &val_b, false, true);

  prop.ParseOncProperty(source, &onc, "c");
  CheckProperty(prop, NULL, false, true);

  prop.ParseOncProperty(source, &onc, "d");
  CheckProperty(prop, NULL, true, false);

  prop.ParseOncProperty(source, &onc, "a.a");
  CheckProperty(prop, NULL, true, false);

  prop.ParseOncProperty(source, &onc, "a.b");
  CheckProperty(prop, &val_a_b, false, true);

  prop.ParseOncProperty(source, &onc, "a.c");
  CheckProperty(prop, NULL, false, true);

  prop.ParseOncProperty(source, &onc, "a.d");
  CheckProperty(prop, NULL, true, false);

  prop.ParseOncProperty(source, NULL, "a.e");
  CheckProperty(prop, NULL, true, false);
}

}  // namespace chromeos
