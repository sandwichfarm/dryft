// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_permission_manager_factory.h"

#include "chrome/browser/nostr/nostr_permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_prefs/user_prefs.h"

namespace nostr {

// static
NostrPermissionManager* NostrPermissionManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<NostrPermissionManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NostrPermissionManagerFactory* NostrPermissionManagerFactory::GetInstance() {
  static base::NoDestructor<NostrPermissionManagerFactory> instance;
  return instance.get();
}

NostrPermissionManagerFactory::NostrPermissionManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "NostrPermissionManager",
          BrowserContextDependencyManager::GetInstance()) {
  // No dependencies for now - permissions are self-contained
}

// static
void NostrPermissionManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  NostrPermissionManager::RegisterProfilePrefs(registry);
}

NostrPermissionManagerFactory::~NostrPermissionManagerFactory() = default;

KeyedService* NostrPermissionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new NostrPermissionManager(profile);
}

content::BrowserContext* NostrPermissionManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the regular profile, not the incognito profile
  // Permissions should persist across incognito sessions
  return context;
}

bool NostrPermissionManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  // Don't create the service until it's first requested
  return false;
}

}  // namespace nostr