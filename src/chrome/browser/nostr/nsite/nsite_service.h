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

namespace content {
class WebContents;
}

namespace nostr {

class NsiteStreamingServer;
class NsiteHeaderInjector;
class NsiteUpdateMonitor;
class NsiteNotificationManager;

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

  // Header injection management
  void SetNsiteForTab(content::WebContents* web_contents, 
                      const std::string& npub);
  void ClearNsiteForTab(content::WebContents* web_contents);
  std::string GetNsiteForTab(content::WebContents* web_contents);

  // Notification management
  void ShowUpdateNotification(content::WebContents* web_contents,
                              const std::string& npub,
                              const std::string& path);

 private:
  friend class base::NoDestructor<NsiteService>;
  
  NsiteService();
  ~NsiteService();

  // Thread-safe server management
  struct ServerInfo {
    std::unique_ptr<NsiteStreamingServer> server;
    std::unique_ptr<NsiteHeaderInjector> header_injector;
    std::unique_ptr<NsiteUpdateMonitor> update_monitor;
    std::unique_ptr<NsiteNotificationManager> notification_manager;
    uint16_t port = 0;
  };

  // Get or create header injector for profile
  NsiteHeaderInjector* GetOrCreateHeaderInjector(Profile* profile);

  base::Lock lock_;
  std::map<Profile*, ServerInfo> servers_ GUARDED_BY(lock_);
  std::vector<ServerStateCallback> observers_ GUARDED_BY(lock_);

  void NotifyServerStateChange(bool running, uint16_t port);
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_SERVICE_H_