// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
#define MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_

#include <stdint.h>
#include <stdlib.h>  // For |free()|.

#include "base/basictypes.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// This class is used to represent data in transit. It is thread-unsafe.
// Note: This class is POD.
class MOJO_SYSTEM_IMPL_EXPORT MessageInTransit {
 public:
  typedef uint16_t Type;
  // Messages that are forwarded to |MessagePipeEndpoint|s.
  static const Type kTypeMessagePipeEndpoint = 0;
  // Messages that are forwarded to |MessagePipe|s.
  static const Type kTypeMessagePipe = 1;
  // Messages that are consumed by the channel.
  static const Type kTypeChannel = 2;

  typedef uint16_t Subtype;
  // Subtypes for type |kTypeMessagePipeEndpoint|:
  static const Subtype kSubtypeMessagePipeEndpointData = 0;
  // Subtypes for type |kTypeMessagePipe|:
  static const Subtype kSubtypeMessagePipePeerClosed = 0;

  typedef uint32_t EndpointId;
  // Never a valid endpoint ID.
  static const EndpointId kInvalidEndpointId = 0;

  // Messages (the header and data) must always be aligned to a multiple of this
  // quantity (which must be a power of 2).
  static const size_t kMessageAlignment = 8;

  // Creates a |MessageInTransit| of the given |type| and |subtype|, with the
  // data given by |bytes|/|num_bytes|.
  static MessageInTransit* Create(Type type,
                                  Subtype subtype,
                                  const void* bytes,
                                  uint32_t num_bytes,
                                  uint32_t num_handles);

  // Destroys a |MessageInTransit| created using |Create()|.
  inline void Destroy() {
    // No need to call the destructor, since we're POD.
    free(this);
  }

  // Gets the size of the data (in number of bytes).
  uint32_t data_size() const {
    return data_size_;
  }

  // Gets the data (of size |data_size()| bytes).
  const void* data() const {
    return reinterpret_cast<const char*>(this) + sizeof(*this);
  }

  // TODO(vtl): Add a |num_bytes()| and use that (instead of |data_size()|)
  // where appropriate. Similarly for |bytes()| (instead of |data()|)?

  uint32_t num_handles() const {
    return num_handles_;
  }

  size_t size_with_header_and_padding() const {
    return RoundUpMessageAlignment(sizeof(*this) + data_size_);
  }

  Type type() const { return type_; }
  Subtype subtype() const { return subtype_; }
  EndpointId source_id() const { return source_id_; }
  EndpointId destination_id() const { return destination_id_; }

  void set_source_id(EndpointId source_id) { source_id_ = source_id; }
  void set_destination_id(EndpointId destination_id) {
    destination_id_ = destination_id;
  }

  // TODO(vtl): Add whatever's necessary to transport handles.

  // Rounds |n| up to a multiple of |kMessageAlignment|.
  static inline size_t RoundUpMessageAlignment(size_t n) {
    return (n + kMessageAlignment - 1) & ~(kMessageAlignment - 1);
  }

 private:
  // TODO(vtl): Set |num_bytes_| and |num_handles_| properly.
  explicit MessageInTransit(uint32_t data_size,
                            Type type,
                            Subtype subtype,
                            uint32_t num_bytes,
                            uint32_t num_handles)
      : data_size_(data_size),
        type_(type),
        subtype_(subtype),
        source_id_(kInvalidEndpointId),
        destination_id_(kInvalidEndpointId),
        num_bytes_(num_bytes),
        num_handles_(num_handles),
        reserved0_(0),
        reserved1_(0) {}

  // "Header" for the data. Must be a multiple of |kMessageAlignment| bytes in
  // size.
  // Total size of data following the "header".
  uint32_t data_size_;
  Type type_;
  Subtype subtype_;
  EndpointId source_id_;
  EndpointId destination_id_;
  // Size of actual message data.
  uint32_t num_bytes_;
  // Number of handles "attached".
  uint32_t num_handles_;
  // To be used soon.
  uint32_t reserved0_;
  uint32_t reserved1_;

  // Intentionally unimplemented (and private): Use |Destroy()| instead (which
  // simply frees the memory).
  ~MessageInTransit();

  DISALLOW_COPY_AND_ASSIGN(MessageInTransit);
};

// The size of |MessageInTransit| must be appropriate to maintain alignment of
// the following data.
COMPILE_ASSERT(sizeof(MessageInTransit) % MessageInTransit::kMessageAlignment ==
                   0, MessageInTransit_has_wrong_size);

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
