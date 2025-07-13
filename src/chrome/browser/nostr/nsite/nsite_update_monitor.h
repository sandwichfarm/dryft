// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_UPDATE_MONITOR_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_UPDATE_MONITOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace nostr {

class NsiteCacheManager;

// Monitors nsites for updates in the background after serving cached content.
// Implements rate limiting and progressive downloading to avoid disrupting
// the user experience.
class NsiteUpdateMonitor {
 public:
  // Callback for update notifications
  using UpdateCallback = base::RepeatingCallback<void(const std::string& npub, 
                                                      const std::string& path)>;

  explicit NsiteUpdateMonitor(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      NsiteCacheManager* cache_manager);
  ~NsiteUpdateMonitor();

  // Start monitoring an nsite for updates after serving from cache
  void CheckForUpdates(const std::string& npub, 
                       const std::string& path,
                       UpdateCallback callback);

  // Set the minimum interval between checks for the same nsite (default: 5 minutes)
  void SetMinCheckInterval(base::TimeDelta interval);

  // Stop monitoring all nsites (called on shutdown)
  void Stop();

 private:
  struct NsiteCheckInfo {
    base::Time last_check_time;
    base::Time last_update_time;
    UpdateCallback callback;
    bool check_in_progress = false;
  };

  // Performs the actual update check on a background thread
  void PerformUpdateCheck(const std::string& npub, 
                          const std::string& path,
                          UpdateCallback callback);

  // Check if enough time has passed since last check
  bool ShouldCheckForUpdates(const std::string& npub);

  // Handle update check results
  void OnUpdateCheckComplete(const std::string& npub,
                             const std::string& path,
                             UpdateCallback callback,
                             bool has_updates);

  // Download updated files in background
  void DownloadUpdatedFiles(const std::string& npub,
                            const std::string& path);

  // Thread-safe access to check info
  base::Lock lock_;
  std::map<std::string, NsiteCheckInfo> check_info_ GUARDED_BY(lock_);

  // Minimum interval between checks (default: 5 minutes)
  base::TimeDelta min_check_interval_;

  // Dependencies
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  NsiteCacheManager* cache_manager_;  // Not owned

  // Background task runner for update checks
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<NsiteUpdateMonitor> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_UPDATE_MONITOR_H_