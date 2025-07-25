// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)

#include "chrome/browser/nostr/key_storage_linux.h"

#include <cstdlib>

#include "base/environment.h"
#include "base/logging.h"
#include "chrome/browser/nostr/file_fallback_storage.h"
#include "chrome/browser/nostr/secret_service_client.h"
#include "chrome/browser/profiles/profile.h"

namespace nostr {

KeyStorageLinux::KeyStorageLinux(Profile* profile) 
    : profile_(profile),
      use_secret_service_(false) {
  DCHECK(profile_);
  InitializeStorage();
}

KeyStorageLinux::~KeyStorageLinux() = default;

void KeyStorageLinux::InitializeStorage() {
  // Detect desktop environment
  desktop_environment_ = DetectDesktopEnvironment();
  LOG(INFO) << "Detected desktop environment: " << static_cast<int>(desktop_environment_);
  
  // Try to initialize Secret Service first
  secret_service_ = std::make_unique<SecretServiceClient>();
  if (secret_service_->Initialize()) {
    use_secret_service_ = true;
    LOG(INFO) << "Using Secret Service for key storage";
    return;
  }
  
  LOG(WARNING) << "Secret Service not available, using file fallback storage";
  
  // Fall back to file-based storage
  file_fallback_ = std::make_unique<FileFallbackStorage>(profile_);
  if (!file_fallback_->Initialize()) {
    LOG(ERROR) << "Failed to initialize file fallback storage";
    // Continue anyway, operations will fail gracefully
  }
}

KeyStorage* KeyStorageLinux::GetActiveStorage() {
  if (use_secret_service_ && secret_service_) {
    return secret_service_.get();
  }
  return file_fallback_.get();
}

bool KeyStorageLinux::IsSecretServiceAvailable() const {
  return use_secret_service_ && secret_service_ && secret_service_->IsAvailable();
}

KeyStorageLinux::DesktopEnvironment KeyStorageLinux::DetectDesktopEnvironment() const {
  auto env = base::Environment::Create();
  
  std::string desktop;
  if (env->GetVar("XDG_CURRENT_DESKTOP", &desktop)) {
    if (desktop.find("GNOME") != std::string::npos) {
      return DesktopEnvironment::kGnome;
    } else if (desktop.find("KDE") != std::string::npos) {
      return DesktopEnvironment::kKde;
    } else if (desktop.find("XFCE") != std::string::npos) {
      return DesktopEnvironment::kXfce;
    }
  }
  
  // Check for specific session variables
  std::string session;
  if (env->GetVar("DESKTOP_SESSION", &session)) {
    if (session.find("gnome") != std::string::npos) {
      return DesktopEnvironment::kGnome;
    } else if (session.find("kde") != std::string::npos) {
      return DesktopEnvironment::kKde;
    } else if (session.find("xfce") != std::string::npos) {
      return DesktopEnvironment::kXfce;
    }
  }
  
  // Check for running processes as last resort
  if (std::system("pgrep gnome-shell > /dev/null 2>&1") == 0) {
    return DesktopEnvironment::kGnome;
  } else if (std::system("pgrep plasmashell > /dev/null 2>&1") == 0) {
    return DesktopEnvironment::kKde;
  }
  
  return DesktopEnvironment::kUnknown;
}

bool KeyStorageLinux::StoreKey(const KeyIdentifier& id,
                              const EncryptedKey& key) {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return false;
  }
  
  return storage->StoreKey(id, key);
}

std::optional<EncryptedKey> KeyStorageLinux::RetrieveKey(
    const KeyIdentifier& id) {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return std::nullopt;
  }
  
  return storage->RetrieveKey(id);
}

bool KeyStorageLinux::DeleteKey(const KeyIdentifier& id) {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return false;
  }
  
  return storage->DeleteKey(id);
}

std::vector<KeyIdentifier> KeyStorageLinux::ListKeys() {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return {};
  }
  
  return storage->ListKeys();
}

bool KeyStorageLinux::UpdateKeyMetadata(const KeyIdentifier& id) {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return false;
  }
  
  return storage->UpdateKeyMetadata(id);
}

bool KeyStorageLinux::HasKey(const std::string& key_id) {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return false;
  }
  
  return storage->HasKey(key_id);
}

std::optional<KeyIdentifier> KeyStorageLinux::GetDefaultKey() {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return std::nullopt;
  }
  
  return storage->GetDefaultKey();
}

bool KeyStorageLinux::SetDefaultKey(const std::string& key_id) {
  auto* storage = GetActiveStorage();
  if (!storage) {
    LOG(ERROR) << "No storage backend available";
    return false;
  }
  
  return storage->SetDefaultKey(key_id);
}

}  // namespace nostr

#endif  // BUILDFLAG(IS_LINUX)