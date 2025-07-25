// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/local_relay_service_factory.h"

#include "chrome/browser/nostr/local_relay/local_relay_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"

namespace nostr {
namespace local_relay {

// static
LocalRelayServiceFactory* LocalRelayServiceFactory::GetInstance() {
  static base::NoDestructor<LocalRelayServiceFactory> instance;
  return instance.get();
}

// static
LocalRelayConfigManager* LocalRelayServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalRelayConfigManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

LocalRelayServiceFactory::LocalRelayServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalRelayService",
          BrowserContextDependencyManager::GetInstance()) {}

LocalRelayServiceFactory::~LocalRelayServiceFactory() = default;

std::unique_ptr<KeyedService>
LocalRelayServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsOffTheRecord()) {
    return nullptr;
  }

  return std::make_unique<LocalRelayConfigManager>(profile->GetPrefs());
}

}  // namespace local_relay
}  // namespace nostr