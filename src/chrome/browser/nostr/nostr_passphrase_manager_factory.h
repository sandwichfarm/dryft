// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NOSTR_PASSPHRASE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_NOSTR_NOSTR_PASSPHRASE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace nostr {

class NostrPassphraseManager;

// Factory for creating NostrPassphraseManager instances per profile
class NostrPassphraseManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NostrPassphraseManager* GetForProfile(Profile* profile);
  static NostrPassphraseManagerFactory* GetInstance();

  NostrPassphraseManagerFactory(const NostrPassphraseManagerFactory&) = delete;
  NostrPassphraseManagerFactory& operator=(const NostrPassphraseManagerFactory&) = delete;

 private:
  friend base::NoDestructor<NostrPassphraseManagerFactory>;

  NostrPassphraseManagerFactory();
  ~NostrPassphraseManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NOSTR_PASSPHRASE_MANAGER_FACTORY_H_