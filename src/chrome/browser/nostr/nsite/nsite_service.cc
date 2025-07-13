// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_service.h"

#include "base/logging.h"
#include "chrome/browser/nostr/nsite/nsite_streaming_server.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace nostr {

// static
NsiteService* NsiteService::GetInstance() {
  static base::NoDestructor<NsiteService> instance;
  return instance.get();
}

NsiteService::NsiteService() = default;

NsiteService::~NsiteService() {
  // Stop all servers
  base::AutoLock lock(lock_);
  servers_.clear();
}

uint16_t NsiteService::GetOrStartServer(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  
  if (profile->IsOffTheRecord()) {
    LOG(WARNING) << "Nsite streaming server not supported in incognito mode";
    return 0;
  }

  base::AutoLock lock(lock_);
  
  // Check if server already exists for this profile
  auto it = servers_.find(profile);
  if (it != servers_.end() && it->second.server && it->second.server->IsRunning()) {
    return it->second.port;
  }

  // Create new server
  auto server = std::make_unique<NsiteStreamingServer>(profile->GetPath());
  uint16_t port = server->Start();
  
  if (port == 0) {
    LOG(ERROR) << "Failed to start nsite streaming server for profile";
    return 0;
  }

  // Store server info
  ServerInfo& info = servers_[profile];
  info.server = std::move(server);
  info.port = port;

  // Notify observers
  NotifyServerStateChange(true, port);
  
  LOG(INFO) << "Started nsite streaming server on port " << port;
  return port;
}

void NsiteService::StopServer(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  base::AutoLock lock(lock_);
  
  auto it = servers_.find(profile);
  if (it == servers_.end()) {
    return;
  }

  it->second.server.reset();
  it->second.port = 0;
  servers_.erase(it);

  // Notify observers
  NotifyServerStateChange(false, 0);
  
  LOG(INFO) << "Stopped nsite streaming server for profile";
}

uint16_t NsiteService::GetServerPort(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  base::AutoLock lock(lock_);
  
  auto it = servers_.find(profile);
  if (it != servers_.end() && it->second.server && it->second.server->IsRunning()) {
    return it->second.port;
  }
  
  return 0;
}

bool NsiteService::IsServerRunning(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  base::AutoLock lock(lock_);
  
  auto it = servers_.find(profile);
  return it != servers_.end() && it->second.server && it->second.server->IsRunning();
}

void NsiteService::AddServerStateObserver(ServerStateCallback callback) {
  base::AutoLock lock(lock_);
  observers_.push_back(std::move(callback));
}

void NsiteService::NotifyServerStateChange(bool running, uint16_t port) {
  // Called with lock_ held
  for (const auto& observer : observers_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(observer, running, port));
  }
}

}  // namespace nostr