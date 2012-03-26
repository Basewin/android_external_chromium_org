// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CERTIFICATE_PATTERN_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CERTIFICATE_PATTERN_H_
#pragma once

#include <list>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"

namespace base {
class DictionaryValue;
}

namespace net {
struct CertPrincipal;
class X509Certificate;
}

namespace chromeos {

// Class to represent the DER fields of an issuer or a subject in a
// certificate and compare them.
class IssuerSubjectPattern {
 public:
  IssuerSubjectPattern();
  IssuerSubjectPattern(const std::string& common_name,
                       const std::string& locality,
                       const std::string& organization,
                       const std::string& organizational_unit);
  ~IssuerSubjectPattern();

  // Returns true only if any fields set in this pattern match exactly with
  // similar fields in the principal.  If organization_ or organizational_unit_
  // are set, then at least one of the organizations or units in the principal
  // must match.
  bool Matches(const net::CertPrincipal& principal) const;

  // Returns true if all fields in the pattern are empty.
  bool Empty() const;

  // Clears out all values in this pattern (so Empty returns true).
  void Clear();

  void set_common_name(const std::string& name) { common_name_ = name; }
  void set_locality(const std::string& locality) { locality_ = locality; }
  void set_organization(const std::string& organization) {
    organization_ = organization;
  }
  void set_organizational_unit(const std::string& unit) {
    organizational_unit_ = unit;
  }

  const std::string& common_name() const {
    return common_name_;
  }
  const std::string& locality() const {
    return locality_;
  }
  const std::string& organization() const {
    return organization_;
  }
  const std::string& organizational_unit() const {
    return organizational_unit_;
  }

  // Creates a new dictionary with the issuer subject pattern as its contents.
  // Caller assumes ownership.
  base::DictionaryValue* CreateAsDictionary() const;

  bool CopyFromDictionary(const base::DictionaryValue& dictionary);

 private:
  std::string common_name_;
  std::string locality_;
  std::string organization_;
  std::string organizational_unit_;
};

// A class to contain a certificate pattern and find existing matches to the
// pattern in the certificate database.
class CertificatePattern {
 public:
  CertificatePattern();
  ~CertificatePattern();

  // Returns true if this pattern has nothing set (and so would match
  // all certs).  Ignores enrollment_uri_;
  bool Empty() const;

  // Clears out all the values in this pattern (so Empty returns true).
  void Clear();

  // Fetches the matching certificate that has the latest valid start date.
  // Returns a NULL refptr if there is no such match.
  scoped_refptr<net::X509Certificate> GetMatch() const;

  void set_issuer_ca_ref_list(const std::vector<std::string>& ref_list) {
    issuer_ca_ref_list_ = ref_list;
  }
  void set_issuer(const IssuerSubjectPattern& issuer) { issuer_ = issuer; }
  void set_subject(const IssuerSubjectPattern& subject) { subject_ = subject; }
  void set_enrollment_uri_list(const std::vector<std::string>& uri_list) {
    enrollment_uri_list_ = uri_list;
  }

  const IssuerSubjectPattern& issuer() const {
    return issuer_;
  }
  const IssuerSubjectPattern& subject() const {
    return subject_;
  }
  const std::vector<std::string>& issuer_ca_ref_list() const {
    return issuer_ca_ref_list_;
  }
  const std::vector<std::string>& enrollment_uri_list() const {
    return enrollment_uri_list_;
  }

  // Creates a new dictionary containing the data in the certificate pattern.
  base::DictionaryValue* CreateAsDictionary() const;

  // Replaces the contents of this CertificatePattern object with
  // the values in the dictionary.  Returns false if the dictionary is
  // malformed.
  bool CopyFromDictionary(const base::DictionaryValue& dictionary);

 private:
  std::vector<std::string> issuer_ca_ref_list_;
  IssuerSubjectPattern issuer_;
  IssuerSubjectPattern subject_;
  std::vector<std::string> enrollment_uri_list_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CERTIFICATE_PATTERN_H_
