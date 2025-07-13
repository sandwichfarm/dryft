// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_NOTIFICATION_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace content {
class WebContents;
}

namespace nostr {

// Manages update notification banners for nsite content.
// Shows non-intrusive banners when updates are available and handles
// user interactions (reload, dismiss).
class NsiteNotificationManager {
 public:
  NsiteNotificationManager();
  ~NsiteNotificationManager();

  // Show update notification banner for a specific nsite in a tab
  void ShowUpdateNotification(content::WebContents* web_contents,
                              const std::string& npub,
                              const std::string& path);

  // Dismiss notification for a specific nsite (user clicked dismiss)
  void DismissNotification(const std::string& npub);

  // Check if notification is dismissed for an nsite
  bool IsNotificationDismissed(const std::string& npub) const;

  // Clear all dismissed notifications (for testing)
  void ClearDismissedNotifications();

 private:
  // Information about an active notification
  struct NotificationInfo {
    content::WebContents* web_contents = nullptr;
    std::string npub;
    std::string path;
    base::Time show_time;
    std::unique_ptr<base::OneShotTimer> auto_hide_timer;
  };

  // JavaScript for injecting the notification banner
  std::string GetNotificationScript(const std::string& npub,
                                    const std::string& path) const;

  // JavaScript for removing the notification banner
  std::string GetRemoveNotificationScript() const;

  // Handle auto-hide timer expiry
  void OnAutoHideTimer(const std::string& notification_id);

  // Generate unique notification ID
  std::string GenerateNotificationId(content::WebContents* web_contents,
                                     const std::string& npub) const;

  // Check if WebContents is visible (active tab)
  bool IsWebContentsVisible(content::WebContents* web_contents) const;

  // Set of npubs for which notifications have been dismissed
  std::set<std::string> dismissed_notifications_;

  // Active notifications by ID
  std::map<std::string, std::unique_ptr<NotificationInfo>> active_notifications_;

  // Auto-hide timeout (30 seconds)
  static constexpr base::TimeDelta kAutoHideTimeout = base::Seconds(30);

  base::WeakPtrFactory<NsiteNotificationManager> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_NOTIFICATION_MANAGER_H_