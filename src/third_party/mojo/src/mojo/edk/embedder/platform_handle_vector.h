// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOJO_SRC_MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_VECTOR_H_
#define THIRD_PARTY_MOJO_SRC_MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_VECTOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_handle.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_handle_utils.h"
#include "third_party/mojo/src/mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace embedder {

using PlatformHandleVector = std::vector<PlatformHandle>;

// A deleter (for use with |scoped_ptr|) which closes all handles and then
// |delete|s the |PlatformHandleVector|.
struct MOJO_SYSTEM_IMPL_EXPORT PlatformHandleVectorDeleter {
  void operator()(PlatformHandleVector* platform_handles) const {
    CloseAllPlatformHandles(platform_handles);
    delete platform_handles;
  }
};

using ScopedPlatformHandleVectorPtr =
    scoped_ptr<PlatformHandleVector, PlatformHandleVectorDeleter>;

}  // namespace embedder
}  // namespace mojo

#endif  // THIRD_PARTY_MOJO_SRC_MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_VECTOR_H_
