// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_passphrase_manager.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/browser_thread.h"

namespace nostr {

namespace {
// Default cache timeout is 5 minutes
constexpr base::TimeDelta kDefaultCacheTimeout = base::Minutes(5);
}  // namespace

NostrPassphraseManager::NostrPassphraseManager(Profile* profile)
    : profile_(profile),
      cache_timeout_(kDefaultCacheTimeout) {
  DCHECK(profile_);
}

NostrPassphraseManager::~NostrPassphraseManager() {
  ClearCachedPassphrase();
}

void NostrPassphraseManager::RequestPassphrase(const std::string& prompt_message,
                                              PassphraseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  
  // If we have a cached passphrase, use it
  if (HasCachedPassphrase()) {
    std::move(callback).Run(true, cached_passphrase_);
    return;
  }
  
  // Otherwise, show the passphrase dialog
  ShowPassphraseDialog(prompt_message, std::move(callback));
}

void NostrPassphraseManager::ClearCachedPassphrase() {
  // Securely clear the passphrase from memory
  if (!cached_passphrase_.empty()) {
    // Overwrite with zeros before clearing
    std::fill(cached_passphrase_.begin(), cached_passphrase_.end(), '\0');
    cached_passphrase_.clear();
  }
  
  cache_timer_.Stop();
}

bool NostrPassphraseManager::HasCachedPassphrase() const {
  return !cached_passphrase_.empty();
}

void NostrPassphraseManager::SetCacheTimeout(base::TimeDelta timeout) {
  cache_timeout_ = timeout;
  
  // If we have a cached passphrase, restart the timer with new timeout
  if (HasCachedPassphrase()) {
    cache_timer_.Stop();
    cache_timer_.Start(FROM_HERE, cache_timeout_,
                      base::BindOnce(&NostrPassphraseManager::OnCacheTimeout,
                                   weak_factory_.GetWeakPtr()));
  }
}

void NostrPassphraseManager::Shutdown() {
  ClearCachedPassphrase();
  weak_factory_.InvalidateWeakPtrs();
}

std::string NostrPassphraseManager::RequestPassphraseSync(const std::string& prompt_message) {
  // If we have a cached passphrase, use it
  if (HasCachedPassphrase()) {
    return cached_passphrase_;
  }
  
  // For now, return a placeholder until UI is implemented
  // This is still better than hardcoded passphrase as it will force implementation
  LOG(ERROR) << "Synchronous passphrase prompt not yet implemented: " << prompt_message;
  
  // Return empty string to indicate failure
  return "";
}

void NostrPassphraseManager::ShowPassphraseDialog(const std::string& prompt_message,
                                                 PassphraseCallback callback) {
  // TODO: Implement actual UI dialog
  // For now, this is a placeholder that fails
  LOG(ERROR) << "Passphrase dialog not yet implemented: " << prompt_message;
  std::move(callback).Run(false, "");
}

void NostrPassphraseManager::OnPassphraseDialogResponse(PassphraseCallback callback,
                                                       bool accepted,
                                                       const std::string& passphrase) {
  if (!accepted || passphrase.empty()) {
    std::move(callback).Run(false, "");
    return;
  }
  
  // Cache the passphrase and start the timer
  CachePassphrase(passphrase);
  
  // Return the passphrase to the caller
  std::move(callback).Run(true, passphrase);
}

void NostrPassphraseManager::CachePassphrase(const std::string& passphrase) {
  cached_passphrase_ = passphrase;
  
  // Start or restart the cache timer
  cache_timer_.Stop();
  cache_timer_.Start(FROM_HERE, cache_timeout_,
                    base::BindOnce(&NostrPassphraseManager::OnCacheTimeout,
                                 weak_factory_.GetWeakPtr()));
}

void NostrPassphraseManager::OnCacheTimeout() {
  LOG(INFO) << "Passphrase cache timeout - clearing cached passphrase";
  ClearCachedPassphrase();
}

}  // namespace nostr