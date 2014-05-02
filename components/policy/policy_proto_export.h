// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_POLICY_PROTO_EXPORT_H_
#define COMPONENTS_POLICY_POLICY_PROTO_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(POLICY_PROTO_COMPILATION)
#define POLICY_PROTO_EXPORT __declspec(dllexport)
#else
#define POLICY_PROTO_EXPORT __declspec(dllimport)
#endif  // defined(POLICY_PROTO_COMPILATION)

#else  // defined(WIN32)

#if defined(POLICY_PROTO_COMPILATION)
#define POLICY_PROTO_EXPORT __attribute__((visibility("default")))
#else
#define POLICY_PROTO_EXPORT
#endif  // defined(POLICY_PROTO_COMPILATION)

#endif  // defined(WIN32)

#else  // defined(COMPONENT_BUILD)

#define POLICY_PROTO_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // COMPONENTS_POLICY_POLICY_PROTO_EXPORT_H_
