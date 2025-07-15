// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/extension_migration_service_factory.h"

#include "chrome/browser/nostr/extension_migration_service.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace nostr {

// static
ExtensionMigrationService* ExtensionMigrationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionMigrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ExtensionMigrationServiceFactory* ExtensionMigrationServiceFactory::GetInstance() {
  static base::NoDestructor<ExtensionMigrationServiceFactory> instance;
  return instance.get();
}

ExtensionMigrationServiceFactory::ExtensionMigrationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionMigrationService",
          BrowserContextDependencyManager::GetInstance()) {
  // Ensure dependencies are created first
  DependsOn(NostrServiceFactory::GetInstance());
  DependsOn(NostrPermissionManagerFactory::GetInstance());
}

ExtensionMigrationServiceFactory::~ExtensionMigrationServiceFactory() = default;

KeyedService* ExtensionMigrationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ExtensionMigrationService(profile);
}

}  // namespace nostr