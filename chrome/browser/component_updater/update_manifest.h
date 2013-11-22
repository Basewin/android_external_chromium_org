// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_UPDATE_MANIFEST_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_UPDATE_MANIFEST_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "url/gurl.h"

namespace component_updater {

// Parses responses for the update protocol version 3.
// (http://code.google.com/p/omaha/wiki/ServerProtocol)
//
// An update manifest looks like this:
//
// <?xml version="1.0" encoding="UTF-8"?>
//  <response protocol="3.0" server="prod">
//    <daystart elapsed_seconds="56508"/>
//    <app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" status="ok">
//      <updatecheck status="noupdate"/>
//      <ping status="ok"/>
//    </app>
//    <app appid="{D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}" status="ok">
//      <updatecheck status="ok">
//        <urls>
//          <url codebase="http://host/edgedl/chrome/install/782.112/"
//          <url codebasediff="http://fallback/chrome/diff/782.112/"/>
//        </urls>
//        <manifest version="13.0.782.112" prodversionmin="2.0.143.0">
//          <packages>
//            <package name="component.crx"
//                     namediff="diff_1.2.3.4.crx"
//                     fp="1.123"
//                     hash_sha256="9830b4245c4..." size="23963192"
//                     hashdiff_sha256="cfb6caf3d0..." sizediff="101"/>
//          </packages>
//        </manifest>
//      </updatecheck>
//      <ping status="ok"/>
//    </app>
//  </response>
//
// The <daystart> tag contains a "elapsed_seconds" attribute which refers to
// the server's notion of how many seconds it has been since midnight.
//
// The "appid" attribute of the <app> tag refers to the unique id of the
// extension. The "codebase" attribute of the <updatecheck> tag is the url to
// fetch the updated crx file, and the "prodversionmin" attribute refers to
// the minimum version of the chrome browser that the update applies to.
//
// The diff data members correspond to the differential update package, if
// a differential update is specified in the response.
class UpdateManifest {
 public:
  // The result of parsing one <app> tag in an xml update check manifest.
  struct Result {
    struct Manifest {
      struct Package {
        Package();
        ~Package();

        std::string fingerprint;

        // Attributes for the full update.
        std::string name;
        std::string hash_sha256;
        int size;

        // Attributes for the differential update.
        std::string namediff;
        std::string hashdiff_sha256;
        int sizediff;
      };

      Manifest();
      ~Manifest();

      std::string version;
      std::string browser_min_version;
      std::vector<Package> packages;
    };

    Result();
    ~Result();

    std::string extension_id;

    // The list of fallback urls, for full and diff updates respectively.
    std::vector<GURL> crx_urls;
    std::vector<GURL> crx_diffurls;

    Manifest manifest;
  };

  static const int kNoDaystart = -1;
  struct Results {
    Results();
    ~Results();

    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_seconds;
    std::vector<Result> list;
  };

  UpdateManifest();
  ~UpdateManifest();

  // Parses an update manifest xml string into Result data. Returns a bool
  // indicating success or failure. On success, the results are available by
  // calling results(). The details for any failures are available by calling
  // errors().
  bool Parse(const std::string& manifest_xml);

  const Results& results() const { return results_; }
  const std::string& errors() const { return errors_; }

 private:
  Results results_;
  std::string errors_;

  // Adds parse error details to |errors_| string.
  void ParseError(const char* details, ...);

  DISALLOW_COPY_AND_ASSIGN(UpdateManifest);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_UPDATE_MANIFEST_H_

