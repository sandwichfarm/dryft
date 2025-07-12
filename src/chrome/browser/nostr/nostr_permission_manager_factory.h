// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_PERMISSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_NOSTR_NOSTR_PERMISSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

namespace nostr {

class NostrPermissionManager;

// Factory for creating NostrPermissionManager instances
class NostrPermissionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NostrPermissionManager* GetForProfile(Profile* profile);
  static NostrPermissionManagerFactory* GetInstance();

  // Register preferences for the service
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  NostrPermissionManagerFactory(const NostrPermissionManagerFactory&) = delete;
  NostrPermissionManagerFactory& operator=(const NostrPermissionManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<NostrPermissionManagerFactory>;

  NostrPermissionManagerFactory();
  ~NostrPermissionManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_PERMISSION_MANAGER_FACTORY_H_