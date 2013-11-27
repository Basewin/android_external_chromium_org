// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_ARGUMENTS_H_
#define GIN_ARGUMENTS_H_

#include "base/basictypes.h"
#include "gin/converter.h"

namespace gin {

// Arguments is a wrapper around v8::FunctionCallbackInfo that integrates
// with Converter to make it easier to marshall arguments and return values
// between V8 and C++.
class Arguments {
 public:
  Arguments();
  explicit Arguments(const v8::FunctionCallbackInfo<v8::Value>& info);
  ~Arguments();

  template<typename T>
  // TODO(aa): Rename GetHolder().
  bool Holder(T* out) {
    return ConvertFromV8(isolate_, info_->Holder(), out);
  }

  template<typename T>
  bool GetData(T* out) {
    return ConvertFromV8(isolate_, info_->Data(), out);
  }

  template<typename T>
  bool GetNext(T* out) {
    if (next_ >= info_->Length()) {
      insufficient_arguments_ = true;
      return false;
    }
    v8::Handle<v8::Value> val = (*info_)[next_++];
    return ConvertFromV8(isolate_, val, out);
  }

  template<typename T>
  bool GetRemaining(std::vector<T>* out) {
    if (next_ >= info_->Length()) {
      insufficient_arguments_ = true;
      return false;
    }
    int remaining = info_->Length() - next_;
    out->resize(remaining);
    for (int i = 0; i < remaining; ++i) {
      v8::Handle<v8::Value> val = (*info_)[next_++];
      if (!ConvertFromV8(isolate_, val, &out->at(i)))
        return false;
    }
    return true;
  }

  template<typename T>
  void Return(T val) {
    info_->GetReturnValue().Set(ConvertToV8(isolate_, val));
  }

  v8::Handle<v8::Value> PeekNext();

  void ThrowError();
  void ThrowTypeError(const std::string& message);

  v8::Isolate* isolate() const { return isolate_; }

 private:
  v8::Isolate* isolate_;
  const v8::FunctionCallbackInfo<v8::Value>* info_;
  int next_;
  bool insufficient_arguments_;
};

template<>
bool Arguments::GetNext<Arguments>(Arguments* out);

}  // namespace gin

#endif  // GIN_ARGUMENTS_H_
