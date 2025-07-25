// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_EXTENSION_MIGRATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOSTR_EXTENSION_MIGRATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace nostr {

class ExtensionMigrationService;

// Factory for creating ExtensionMigrationService instances per profile
class ExtensionMigrationServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionMigrationService* GetForProfile(Profile* profile);
  static ExtensionMigrationServiceFactory* GetInstance();

  ExtensionMigrationServiceFactory(const ExtensionMigrationServiceFactory&) = delete;
  ExtensionMigrationServiceFactory& operator=(const ExtensionMigrationServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ExtensionMigrationServiceFactory>;

  ExtensionMigrationServiceFactory();
  ~ExtensionMigrationServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_EXTENSION_MIGRATION_SERVICE_FACTORY_H_