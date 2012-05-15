# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      # Define an "os_include" variable that points at the OS-specific generated
      # headers.  These were generated by running the configure script offline.
      ['os_posix == 1 and OS != "mac"', {
        'os_include': 'linux'
      }],
      ['OS=="mac"', {'os_include': 'mac'}],
      ['OS=="win"', {'os_include': 'win32'}],
    ],
    'use_system_libxml%': 0,
  },
  'targets': [
    {
      'target_name': 'libxml',
      'conditions': [
        ['os_posix == 1 and OS != "mac" and use_system_libxml', {
          'type': 'static_library',
          'sources': [
            'chromium/libxml_utils.h',
            'chromium/libxml_utils.cc',
          ],
          'cflags': [
            '<!@(pkg-config --cflags libxml-2.0)',
          ],
          'defines': [
            'USE_SYSTEM_LIBXML',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags libxml-2.0)',
            ],
            'defines': [
              'USE_SYSTEM_LIBXML',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other libxml-2.0)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l libxml-2.0)',
            ],
          },
        }, { # else: os_posix != 1 or OS == "mac" or ! use_system_libxml
          'type': 'static_library',
          'sources': [
            'chromium/libxml_utils.h',
            'chromium/libxml_utils.cc',
            'linux/config.h',
            'linux/include/libxml/xmlversion.h',
            'mac/config.h',
            'mac/include/libxml/xmlversion.h',
            'src/include/libxml/c14n.h',
            'src/include/libxml/catalog.h',
            'src/include/libxml/chvalid.h',
            'src/include/libxml/debugXML.h',
            'src/include/libxml/dict.h',
            'src/include/libxml/DOCBparser.h',
            'src/include/libxml/encoding.h',
            'src/include/libxml/entities.h',
            'src/include/libxml/globals.h',
            'src/include/libxml/hash.h',
            'src/include/libxml/HTMLparser.h',
            'src/include/libxml/HTMLtree.h',
            'src/include/libxml/list.h',
            'src/include/libxml/nanoftp.h',
            'src/include/libxml/nanohttp.h',
            'src/include/libxml/parser.h',
            'src/include/libxml/parserInternals.h',
            'src/include/libxml/pattern.h',
            'src/include/libxml/relaxng.h',
            'src/include/libxml/SAX.h',
            'src/include/libxml/SAX2.h',
            'src/include/libxml/schemasInternals.h',
            'src/include/libxml/schematron.h',
            'src/include/libxml/threads.h',
            'src/include/libxml/tree.h',
            'src/include/libxml/uri.h',
            'src/include/libxml/valid.h',
            'src/include/libxml/xinclude.h',
            'src/include/libxml/xlink.h',
            'src/include/libxml/xmlautomata.h',
            'src/include/libxml/xmlerror.h',
            'src/include/libxml/xmlexports.h',
            'src/include/libxml/xmlIO.h',
            'src/include/libxml/xmlmemory.h',
            'src/include/libxml/xmlmodule.h',
            'src/include/libxml/xmlreader.h',
            'src/include/libxml/xmlregexp.h',
            'src/include/libxml/xmlsave.h',
            'src/include/libxml/xmlschemas.h',
            'src/include/libxml/xmlschemastypes.h',
            'src/include/libxml/xmlstring.h',
            'src/include/libxml/xmlunicode.h',
            'src/include/libxml/xmlwriter.h',
            'src/include/libxml/xpath.h',
            'src/include/libxml/xpathInternals.h',
            'src/include/libxml/xpointer.h',
            'src/include/win32config.h',
            'src/include/wsockcompat.h',
            'src/acconfig.h',
            'src/c14n.c',
            'src/catalog.c',
            'src/chvalid.c',
            'src/debugXML.c',
            'src/dict.c',
            'src/DOCBparser.c',
            'src/elfgcchack.h',
            'src/encoding.c',
            'src/entities.c',
            'src/error.c',
            'src/globals.c',
            'src/hash.c',
            'src/HTMLparser.c',
            'src/HTMLtree.c',
            'src/legacy.c',
            'src/libxml.h',
            'src/list.c',
            'src/nanoftp.c',
            'src/nanohttp.c',
            'src/parser.c',
            'src/parserInternals.c',
            'src/pattern.c',
            'src/relaxng.c',
            'src/SAX.c',
            'src/SAX2.c',
            'src/schematron.c',
            'src/threads.c',
            'src/tree.c',
            #'src/trio.c',
            #'src/trio.h',
            #'src/triodef.h',
            #'src/trionan.c',
            #'src/trionan.h',
            #'src/triop.h',
            #'src/triostr.c',
            #'src/triostr.h',
            'src/uri.c',
            'src/valid.c',
            'src/xinclude.c',
            'src/xlink.c',
            'src/xmlIO.c',
            'src/xmlmemory.c',
            'src/xmlmodule.c',
            'src/xmlreader.c',
            'src/xmlregexp.c',
            'src/xmlsave.c',
            'src/xmlschemas.c',
            'src/xmlschemastypes.c',
            'src/xmlstring.c',
            'src/xmlunicode.c',
            'src/xmlwriter.c',
            'src/xpath.c',
            'src/xpointer.c',
            'win32/config.h',
            'win32/include/libxml/xmlversion.h',
          ],
          'defines': [
            # Define LIBXML_STATIC as nothing to match how libxml.h
            # (an internal header) defines LIBXML_STATIC, otherwise
            # we get the macro redefined warning from GCC.  (-DFOO
            # defines the macro FOO as 1.)
            'LIBXML_STATIC=',
          ],
          'include_dirs': [
            '<(os_include)',
            '<(os_include)/include',
            'src/include',
          ],
          'dependencies': [
            '../icu/icu.gyp:icuuc',
            '../zlib/zlib.gyp:zlib',
          ],
          'export_dependent_settings': [
            '../icu/icu.gyp:icuuc',
          ],
          'direct_dependent_settings': {
            'defines': [
              'LIBXML_STATIC',
            ],
            'include_dirs': [
              '<(os_include)/include',
              'src/include',
            ],
          },
          'conditions': [
            ['OS=="linux"', {
              'link_settings': {
                'libraries': [
                  # We need dl for dlopen() and friends.
                  '-ldl',
                ],
              },
            }],
            ['OS=="mac" or OS=="android"', {'defines': ['_REENTRANT']}],
            ['OS=="win"', {
              'product_name': 'libxml2',
              # Disable unimportant 'unused variable' warning, and
              # signed/unsigned comparison warning. The signed/unsigned (4101)
              # is fixed upstream and can be removed eventually.
              'msvs_disabled_warnings': [ 4018, 4101 ],
            }, {  # else: OS!="win"
              'product_name': 'xml2',
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # libxml passes `const unsigned char*` through `const char*`.
                  '-Wno-pointer-sign',
                  # pattern.c and uri.c both have an intentional
                  # `for (...);` / `while(...);` loop. I submitted a patch to
                  # move the `'` to its own line, but until that's landed
                  # suppress the warning:
                  '-Wno-empty-body',
                ],
              },
              'cflags': [
                '-Wno-pointer-sign',
                '-Wno-empty-body',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
