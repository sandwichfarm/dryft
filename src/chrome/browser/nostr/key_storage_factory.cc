// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/key_storage_factory.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/nostr/key_storage_in_memory.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/nostr/key_storage_windows.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/nostr/key_storage_mac.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/nostr/key_storage_linux.h"
#include "chrome/browser/nostr/secret_service_client.h"
#endif

namespace nostr {

// static
std::unique_ptr<KeyStorage> KeyStorageFactory::CreateKeyStorage(Profile* profile) {
  return CreateKeyStorage(profile, GetDefaultBackend());
}

// static
std::unique_ptr<KeyStorage> KeyStorageFactory::CreateKeyStorage(
    Profile* profile,
    StorageBackend backend) {
  if (!IsBackendAvailable(backend)) {
    LOG(WARNING) << "Requested backend " << GetBackendName(backend) 
                 << " is not available, falling back to encrypted preferences";
    backend = StorageBackend::ENCRYPTED_PREFERENCES;
  }
  
  switch (backend) {
    case StorageBackend::PLATFORM_DEFAULT:
      return CreateKeyStorage(profile, GetDefaultBackend());
      
#if BUILDFLAG(IS_WIN)
    case StorageBackend::WINDOWS_CREDENTIAL_MANAGER:
      return CreateWindowsKeyStorage(profile);
#endif
      
#if BUILDFLAG(IS_MAC)
    case StorageBackend::MACOS_KEYCHAIN:
      return CreateMacOSKeyStorage(profile);
#endif
      
#if BUILDFLAG(IS_LINUX)
    case StorageBackend::LINUX_SECRET_SERVICE:
      return CreateLinuxKeyStorage(profile);
#endif
      
    case StorageBackend::ENCRYPTED_PREFERENCES:
      return CreateEncryptedPrefsKeyStorage(profile);
      
    case StorageBackend::IN_MEMORY:
      return CreateInMemoryKeyStorage();
      
    default:
      LOG(ERROR) << "Unsupported storage backend: " << static_cast<int>(backend);
      return CreateEncryptedPrefsKeyStorage(profile);
  }
}

// static
KeyStorageFactory::StorageBackend KeyStorageFactory::GetDefaultBackend() {
#if BUILDFLAG(IS_WIN)
  return StorageBackend::WINDOWS_CREDENTIAL_MANAGER;
#elif BUILDFLAG(IS_MAC)
  return StorageBackend::MACOS_KEYCHAIN;
#elif BUILDFLAG(IS_LINUX)
  return StorageBackend::LINUX_SECRET_SERVICE;
#else
  return StorageBackend::ENCRYPTED_PREFERENCES;
#endif
}

// static
bool KeyStorageFactory::IsBackendAvailable(StorageBackend backend) {
  switch (backend) {
    case StorageBackend::PLATFORM_DEFAULT:
      return true;
      
    case StorageBackend::WINDOWS_CREDENTIAL_MANAGER:
#if BUILDFLAG(IS_WIN)
      return true;
#else
      return false;
#endif
      
    case StorageBackend::MACOS_KEYCHAIN:
#if BUILDFLAG(IS_MAC)
      return true;
#else
      return false;
#endif
      
    case StorageBackend::LINUX_SECRET_SERVICE:
#if BUILDFLAG(IS_LINUX)
      return CheckLinuxSecretServiceAvailable();
#else
      return false;
#endif
      
    case StorageBackend::ENCRYPTED_PREFERENCES:
    case StorageBackend::IN_MEMORY:
      return true;
      
    default:
      return false;
  }
}

// static
std::string KeyStorageFactory::GetBackendName(StorageBackend backend) {
  switch (backend) {
    case StorageBackend::PLATFORM_DEFAULT:
      return "Platform Default";
    case StorageBackend::WINDOWS_CREDENTIAL_MANAGER:
      return "Windows Credential Manager";
    case StorageBackend::MACOS_KEYCHAIN:
      return "macOS Keychain";
    case StorageBackend::LINUX_SECRET_SERVICE:
      return "Linux Secret Service";
    case StorageBackend::ENCRYPTED_PREFERENCES:
      return "Encrypted Preferences";
    case StorageBackend::IN_MEMORY:
      return "In-Memory";
    default:
      return "Unknown";
  }
}

// static
std::unique_ptr<KeyStorage> KeyStorageFactory::CreateWindowsKeyStorage(
    Profile* profile) {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<KeyStorageWindows>(profile);
#else
  LOG(ERROR) << "Windows key storage not available on this platform";
  return CreateEncryptedPrefsKeyStorage(profile);
#endif
}

// static
std::unique_ptr<KeyStorage> KeyStorageFactory::CreateMacOSKeyStorage(
    Profile* profile) {
#if BUILDFLAG(IS_MAC)
  return std::make_unique<KeyStorageMac>(profile);
#else
  LOG(ERROR) << "macOS key storage not available on this platform";
  return CreateEncryptedPrefsKeyStorage(profile);
#endif
}

// static
std::unique_ptr<KeyStorage> KeyStorageFactory::CreateLinuxKeyStorage(
    Profile* profile) {
#if BUILDFLAG(IS_LINUX)
  return std::make_unique<KeyStorageLinux>(profile);
#else
  LOG(ERROR) << "Linux key storage not available on this platform";
  return CreateEncryptedPrefsKeyStorage(profile);
#endif
}

// static
std::unique_ptr<KeyStorage> KeyStorageFactory::CreateEncryptedPrefsKeyStorage(
    Profile* profile) {
  // TODO: Implement encrypted preferences storage as fallback
  LOG(WARNING) << "Encrypted preferences storage not implemented yet, using in-memory storage";
  return CreateInMemoryKeyStorage();
}

// static
std::unique_ptr<KeyStorage> KeyStorageFactory::CreateInMemoryKeyStorage() {
  return std::make_unique<KeyStorageInMemory>();
}

#if BUILDFLAG(IS_LINUX)
// static
bool KeyStorageFactory::CheckLinuxSecretServiceAvailable() {
  // Try to create a SecretServiceClient and see if it can initialize
  // This checks for libsecret availability at runtime
  try {
    auto client = std::make_unique<SecretServiceClient>();
    return client->Initialize();
  } catch (...) {
    // If there's any exception (missing libsecret, etc.), return false
    return false;
  }
}
#endif

}  // namespace nostr