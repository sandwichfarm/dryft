// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace nostr {
namespace local_relay {

class LocalRelayConfigManager;

// Factory for creating LocalRelayConfigManager instances per profile
class LocalRelayServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LocalRelayServiceFactory* GetInstance();
  
  // Gets the LocalRelayConfigManager for the given browser context
  static LocalRelayConfigManager* GetForBrowserContext(
      content::BrowserContext* context);

  LocalRelayServiceFactory(const LocalRelayServiceFactory&) = delete;
  LocalRelayServiceFactory& operator=(const LocalRelayServiceFactory&) = delete;

 private:
  friend base::NoDestructor<LocalRelayServiceFactory>;

  LocalRelayServiceFactory();
  ~LocalRelayServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_LOCAL_RELAY_SERVICE_FACTORY_H_