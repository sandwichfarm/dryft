// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_SERVICE_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chrome/browser/profiles/profile.h"

namespace nostr {

class NsiteStreamingServer;

// Singleton service managing the nsite streaming server
class NsiteService {
 public:
  // Get the singleton instance
  static NsiteService* GetInstance();

  // Get or start the streaming server for a profile
  // Returns the server port, or 0 on failure
  uint16_t GetOrStartServer(Profile* profile);

  // Stop the server for a profile
  void StopServer(Profile* profile);

  // Get the current server port (0 if not running)
  uint16_t GetServerPort(Profile* profile);

  // Check if server is running for a profile
  bool IsServerRunning(Profile* profile);

  // Callback for server state changes
  using ServerStateCallback = base::RepeatingCallback<void(bool running, uint16_t port)>;
  void AddServerStateObserver(ServerStateCallback callback);

 private:
  friend class base::NoDestructor<NsiteService>;
  
  NsiteService();
  ~NsiteService();

  // Thread-safe server management
  struct ServerInfo {
    std::unique_ptr<NsiteStreamingServer> server;
    uint16_t port = 0;
  };

  base::Lock lock_;
  std::map<Profile*, ServerInfo> servers_ GUARDED_BY(lock_);
  std::vector<ServerStateCallback> observers_ GUARDED_BY(lock_);

  void NotifyServerStateChange(bool running, uint16_t port);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_SERVICE_H_