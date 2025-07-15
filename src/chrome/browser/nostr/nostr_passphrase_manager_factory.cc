// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nostr_passphrase_manager_factory.h"

#include "chrome/browser/nostr/nostr_passphrase_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace nostr {

// static
NostrPassphraseManager* NostrPassphraseManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<NostrPassphraseManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NostrPassphraseManagerFactory* NostrPassphraseManagerFactory::GetInstance() {
  static base::NoDestructor<NostrPassphraseManagerFactory> instance;
  return instance.get();
}

NostrPassphraseManagerFactory::NostrPassphraseManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "NostrPassphraseManager",
          BrowserContextDependencyManager::GetInstance()) {}

NostrPassphraseManagerFactory::~NostrPassphraseManagerFactory() = default;

KeyedService* NostrPassphraseManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new NostrPassphraseManager(profile);
}

}  // namespace nostr