// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOSTR_NOSTR_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace nostr {

class NostrService;

// Factory for creating NostrService instances
class NostrServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NostrService* GetForProfile(Profile* profile);
  static NostrService* GetForBrowserContext(content::BrowserContext* context);
  static NostrServiceFactory* GetInstance();

  NostrServiceFactory(const NostrServiceFactory&) = delete;
  NostrServiceFactory& operator=(const NostrServiceFactory&) = delete;

  // Register preferences
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class base::NoDestructor<NostrServiceFactory>;

  NostrServiceFactory();
  ~NostrServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_SERVICE_FACTORY_H_