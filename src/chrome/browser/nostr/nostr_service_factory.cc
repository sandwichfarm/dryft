// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_service_factory.h"

#include "chrome/browser/nostr/nostr_permission_manager_factory.h"
#include "chrome/browser/nostr/nostr_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace nostr {

// static
NostrService* NostrServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NostrService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NostrService* NostrServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return GetForProfile(Profile::FromBrowserContext(context));
}

// static
NostrServiceFactory* NostrServiceFactory::GetInstance() {
  static base::NoDestructor<NostrServiceFactory> instance;
  return instance.get();
}

NostrServiceFactory::NostrServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NostrService",
          BrowserContextDependencyManager::GetInstance()) {
  // NostrService depends on NostrPermissionManager
  DependsOn(NostrPermissionManagerFactory::GetInstance());
}

NostrServiceFactory::~NostrServiceFactory() = default;

KeyedService* NostrServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new NostrService(profile);
}

content::BrowserContext* NostrServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the regular profile, not the incognito profile
  // Nostr keys should persist across incognito sessions
  return context;
}

bool NostrServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Don't create the service until it's first requested
  return false;
}

}  // namespace nostr