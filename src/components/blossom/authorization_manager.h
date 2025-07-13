// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOSSOM_AUTHORIZATION_MANAGER_H_
#define COMPONENTS_BLOSSOM_AUTHORIZATION_MANAGER_H_

#include <string>

#include "base/functional/callback.h"

namespace blossom {

// Manages authorization for Blossom operations based on Nostr events
class AuthorizationManager {
 public:
  virtual ~AuthorizationManager() = default;

  // Check if a request is authorized for the given verb
  // Verbs: "upload", "delete", "list"
  virtual void CheckAuthorization(
      const std::string& auth_header,
      const std::string& verb,
      const std::string& hash,
      base::OnceCallback<void(bool authorized, 
                            const std::string& reason)> callback) = 0;
};

}  // namespace blossom

#endif  // COMPONENTS_BLOSSOM_AUTHORIZATION_MANAGER_H_