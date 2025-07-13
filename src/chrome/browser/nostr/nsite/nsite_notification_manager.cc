// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_notification_manager.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"

namespace nostr {

NsiteNotificationManager::NsiteNotificationManager() = default;

NsiteNotificationManager::~NsiteNotificationManager() = default;

void NsiteNotificationManager::ShowUpdateNotification(
    content::WebContents* web_contents,
    const std::string& npub,
    const std::string& path) {
  DCHECK(web_contents);
  DCHECK(!npub.empty());

  // Don't show if notification was dismissed for this nsite
  if (IsNotificationDismissed(npub)) {
    VLOG(2) << "Notification dismissed for nsite: " << npub;
    return;
  }

  // Only show for visible tabs
  if (!IsWebContentsVisible(web_contents)) {
    VLOG(2) << "Skipping notification for non-visible tab";
    return;
  }

  std::string notification_id = GenerateNotificationId(web_contents, npub);

  // Remove existing notification if any
  auto existing_it = active_notifications_.find(notification_id);
  if (existing_it != active_notifications_.end()) {
    VLOG(2) << "Replacing existing notification for " << npub;
    active_notifications_.erase(existing_it);
  }

  // Create notification info
  auto notification_info = std::make_unique<NotificationInfo>();
  notification_info->web_contents = web_contents;
  notification_info->npub = npub;
  notification_info->path = path;
  notification_info->show_time = base::Time::Now();

  // Set up auto-hide timer
  notification_info->auto_hide_timer = std::make_unique<base::OneShotTimer>();
  notification_info->auto_hide_timer->Start(
      FROM_HERE, kAutoHideTimeout,
      base::BindOnce(&NsiteNotificationManager::OnAutoHideTimer,
                     weak_factory_.GetWeakPtr(), notification_id));

  active_notifications_[notification_id] = std::move(notification_info);

  // Inject notification banner via JavaScript
  std::string script = GetNotificationScript(npub, path);
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(script), base::NullCallback());

  VLOG(1) << "Showing update notification for nsite: " << npub;
}

void NsiteNotificationManager::DismissNotification(const std::string& npub) {
  DCHECK(!npub.empty());

  // Mark as dismissed
  dismissed_notifications_.insert(npub);

  // Remove any active notifications for this nsite
  auto it = active_notifications_.begin();
  while (it != active_notifications_.end()) {
    if (it->second->npub == npub) {
      // Remove banner from page
      if (it->second->web_contents) {
        std::string script = GetRemoveNotificationScript();
        it->second->web_contents->GetPrimaryMainFrame()->ExecuteJavaScript(
            base::UTF8ToUTF16(script), base::NullCallback());
      }
      it = active_notifications_.erase(it);
    } else {
      ++it;
    }
  }

  VLOG(1) << "Dismissed notification for nsite: " << npub;
}

bool NsiteNotificationManager::IsNotificationDismissed(
    const std::string& npub) const {
  return dismissed_notifications_.find(npub) != dismissed_notifications_.end();
}

void NsiteNotificationManager::ClearDismissedNotifications() {
  dismissed_notifications_.clear();
}

std::string NsiteNotificationManager::GetNotificationScript(
    const std::string& npub,
    const std::string& path) const {
  return base::StringPrintf(R"(
(function() {
  // Remove any existing notification
  const existing = document.getElementById('nsite-update-notification');
  if (existing) {
    existing.remove();
  }

  // Create notification banner
  const banner = document.createElement('div');
  banner.id = 'nsite-update-notification';
  banner.setAttribute('role', 'alert');
  banner.setAttribute('aria-live', 'polite');
  
  // Banner styles
  banner.style.cssText = `
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    z-index: 999999;
    background: linear-gradient(135deg, #667eea 0%%, #764ba2 100%%);
    color: white;
    padding: 12px 16px;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', sans-serif;
    font-size: 14px;
    box-shadow: 0 2px 8px rgba(0,0,0,0.15);
    display: flex;
    align-items: center;
    justify-content: space-between;
    animation: slideDown 0.3s ease-out;
  `;

  // Add animation keyframes
  if (!document.getElementById('nsite-notification-styles')) {
    const style = document.createElement('style');
    style.id = 'nsite-notification-styles';
    style.textContent = `
      @keyframes slideDown {
        from { transform: translateY(-100%%); }
        to { transform: translateY(0); }
      }
      @keyframes slideUp {
        from { transform: translateY(0); }
        to { transform: translateY(-100%%); }
      }
      .nsite-notification-hide {
        animation: slideUp 0.3s ease-in forwards !important;
      }
    `;
    document.head.appendChild(style);
  }

  // Message content
  const message = document.createElement('div');
  message.innerHTML = `
    <strong>📡 Nsite Update Available</strong>
    <br><span style="font-size: 12px; opacity: 0.9;">New content is ready for <code style="background: rgba(255,255,255,0.2); padding: 2px 4px; border-radius: 3px;">%s</code></span>
  `;

  // Button container
  const buttons = document.createElement('div');
  buttons.style.cssText = 'display: flex; gap: 8px; align-items: center;';

  // Reload button
  const reloadBtn = document.createElement('button');
  reloadBtn.textContent = '🔄 Reload';
  reloadBtn.style.cssText = `
    background: rgba(255,255,255,0.2);
    color: white;
    border: 1px solid rgba(255,255,255,0.3);
    padding: 6px 12px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 12px;
    transition: background 0.2s;
  `;
  reloadBtn.onmouseover = () => reloadBtn.style.background = 'rgba(255,255,255,0.3)';
  reloadBtn.onmouseout = () => reloadBtn.style.background = 'rgba(255,255,255,0.2)';
  reloadBtn.onclick = () => window.location.reload();

  // Dismiss button
  const dismissBtn = document.createElement('button');
  dismissBtn.textContent = '✕ Dismiss';
  dismissBtn.style.cssText = `
    background: transparent;
    color: rgba(255,255,255,0.8);
    border: 1px solid rgba(255,255,255,0.3);
    padding: 6px 12px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 12px;
    transition: all 0.2s;
  `;
  dismissBtn.onmouseover = () => {
    dismissBtn.style.background = 'rgba(255,255,255,0.1)';
    dismissBtn.style.color = 'white';
  };
  dismissBtn.onmouseout = () => {
    dismissBtn.style.background = 'transparent';
    dismissBtn.style.color = 'rgba(255,255,255,0.8)';
  };
  dismissBtn.onclick = () => {
    banner.classList.add('nsite-notification-hide');
    setTimeout(() => banner.remove(), 300);
    // TODO: Notify backend about dismissal
  };

  buttons.appendChild(reloadBtn);
  buttons.appendChild(dismissBtn);

  banner.appendChild(message);
  banner.appendChild(buttons);

  // Insert at top of body
  document.body.insertBefore(banner, document.body.firstChild);

  // Auto-hide after 30 seconds
  setTimeout(() => {
    if (banner.parentNode) {
      banner.classList.add('nsite-notification-hide');
      setTimeout(() => banner.remove(), 300);
    }
  }, 30000);
})();
)", npub.c_str());
}

std::string NsiteNotificationManager::GetRemoveNotificationScript() const {
  return R"(
(function() {
  const banner = document.getElementById('nsite-update-notification');
  if (banner) {
    banner.classList.add('nsite-notification-hide');
    setTimeout(() => banner.remove(), 300);
  }
})();
)";
}

void NsiteNotificationManager::OnAutoHideTimer(
    const std::string& notification_id) {
  auto it = active_notifications_.find(notification_id);
  if (it != active_notifications_.end()) {
    // Remove banner from page
    if (it->second->web_contents) {
      std::string script = GetRemoveNotificationScript();
      it->second->web_contents->GetPrimaryMainFrame()->ExecuteJavaScript(
          base::UTF8ToUTF16(script), base::NullCallback());
    }
    
    active_notifications_.erase(it);
    VLOG(2) << "Auto-hid notification: " << notification_id;
  }
}

std::string NsiteNotificationManager::GenerateNotificationId(
    content::WebContents* web_contents,
    const std::string& npub) const {
  // Use WebContents pointer and npub to create unique ID
  return base::StringPrintf("%p_%s", web_contents, npub.c_str());
}

bool NsiteNotificationManager::IsWebContentsVisible(
    content::WebContents* web_contents) const {
  // Check if the WebContents is the active tab
  // In a real implementation, this would check tab visibility
  // For now, assume visible if web_contents is valid
  return web_contents != nullptr && !web_contents->IsBeingDestroyed();
}

}  // namespace nostr