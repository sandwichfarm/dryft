// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_PASSPHRASE_MANAGER_H_
#define CHROME_BROWSER_NOSTR_NOSTR_PASSPHRASE_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace nostr {

// Manages passphrase prompting and caching for Nostr key operations
class NostrPassphraseManager : public KeyedService {
 public:
  // Callback for passphrase prompts
  using PassphraseCallback = base::OnceCallback<void(bool success, const std::string& passphrase)>;
  
  explicit NostrPassphraseManager(Profile* profile);
  ~NostrPassphraseManager() override;
  
  // Request a passphrase from the user
  // If cached and not expired, returns immediately
  // Otherwise prompts the user via UI
  void RequestPassphrase(const std::string& prompt_message,
                        PassphraseCallback callback);
  
  // Synchronous passphrase request (blocks until response)
  // Returns empty string on failure
  std::string RequestPassphraseSync(const std::string& prompt_message);
  
  // Clear cached passphrase immediately
  void ClearCachedPassphrase();
  
  // Check if a passphrase is currently cached
  bool HasCachedPassphrase() const;
  
  // Set the cache timeout duration (default: 5 minutes)
  void SetCacheTimeout(base::TimeDelta timeout);
  
  // Get current cache timeout
  base::TimeDelta GetCacheTimeout() const { return cache_timeout_; }
  
  // KeyedService implementation
  void Shutdown() override;
  
 private:
  // Show passphrase prompt dialog
  void ShowPassphraseDialog(const std::string& prompt_message,
                           PassphraseCallback callback);
  
  // Handle dialog response
  void OnPassphraseDialogResponse(PassphraseCallback callback,
                                 bool accepted,
                                 const std::string& passphrase);
  
  // Cache management
  void CachePassphrase(const std::string& passphrase);
  void OnCacheTimeout();
  
  // Profile for context
  Profile* profile_;
  
  // Cached passphrase (cleared on timeout)
  std::string cached_passphrase_;
  
  // Cache timeout duration
  base::TimeDelta cache_timeout_;
  
  // Timer for cache expiration
  base::OneShotTimer cache_timer_;
  
  // Weak pointer factory
  base::WeakPtrFactory<NostrPassphraseManager> weak_factory_{this};
  
  DISALLOW_COPY_AND_ASSIGN(NostrPassphraseManager);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_PASSPHRASE_MANAGER_H_