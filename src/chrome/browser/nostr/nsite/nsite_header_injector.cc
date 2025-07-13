// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_header_injector.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/nostr/nsite/nsite_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace nostr {

// NsiteNavigationContext implementation

NsiteNavigationContext::NsiteNavigationContext(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

NsiteNavigationContext::~NsiteNavigationContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NsiteNavigationContext::SetCurrentNsite(const std::string& npub) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);
  current_npub_ = npub;
  LOG(INFO) << "Set nsite context: " << npub;
}

std::string NsiteNavigationContext::GetCurrentNsite() const {
  base::AutoLock lock(lock_);
  return current_npub_;
}

void NsiteNavigationContext::ClearNsite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);
  if (!current_npub_.empty()) {
    LOG(INFO) << "Cleared nsite context: " << current_npub_;
    current_npub_.clear();
  }
}

void NsiteNavigationContext::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!navigation_handle->IsInPrimaryMainFrame() || 
      !navigation_handle->HasCommitted()) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  
  // Clear nsite context if navigating away from localhost streaming server
  if (!url.SchemeIs("http") || !net::IsLocalhost(url)) {
    ClearNsite();
  }
}

void NsiteNavigationContext::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearNsite();
}

// NsiteHeaderInjector implementation

NsiteHeaderInjector::NsiteHeaderInjector(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  
  // Get nsite service
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  nsite_service_ = NsiteService::GetInstance();
}

NsiteHeaderInjector::~NsiteHeaderInjector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Shutdown();
}

void NsiteHeaderInjector::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (initialized_) {
    return;
  }

  // Register web request interceptor
  extensions::WebRequestAPI* web_request_api = 
      extensions::WebRequestAPI::Get(browser_context_);
  
  if (web_request_api) {
    // TODO: Register request interceptor
    // For now, we'll implement the basic structure
    initialized_ = true;
    LOG(INFO) << "NsiteHeaderInjector initialized";
  }
}

void NsiteHeaderInjector::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!initialized_) {
    return;
  }

  // Clear all navigation contexts
  base::AutoLock lock(contexts_lock_);
  navigation_contexts_.clear();
  
  initialized_ = false;
  LOG(INFO) << "NsiteHeaderInjector shutdown";
}

void NsiteHeaderInjector::SetNsiteForTab(content::WebContents* web_contents,
                                         const std::string& npub) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  
  base::AutoLock lock(contexts_lock_);
  
  auto it = navigation_contexts_.find(web_contents);
  if (it == navigation_contexts_.end()) {
    // Create new navigation context
    auto context = std::make_unique<NsiteNavigationContext>(web_contents);
    context->SetCurrentNsite(npub);
    navigation_contexts_[web_contents] = std::move(context);
  } else {
    // Update existing context
    it->second->SetCurrentNsite(npub);
  }
  
  LOG(INFO) << "Set nsite for tab: " << npub;
}

void NsiteHeaderInjector::ClearNsiteForTab(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  
  base::AutoLock lock(contexts_lock_);
  
  auto it = navigation_contexts_.find(web_contents);
  if (it != navigation_contexts_.end()) {
    it->second->ClearNsite();
    navigation_contexts_.erase(it);
  }
}

std::string NsiteHeaderInjector::GetNsiteForTab(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  
  base::AutoLock lock(contexts_lock_);
  
  auto it = navigation_contexts_.find(web_contents);
  if (it != navigation_contexts_.end()) {
    return it->second->GetCurrentNsite();
  }
  
  return "";
}

void NsiteHeaderInjector::OnBeforeRequest(
    const extensions::WebRequestInfo& info,
    extensions::WebRequestAPI::RequestStage stage,
    net::HttpRequestHeaders* headers,
    extensions::WebRequestAPI::Response* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (!IsStreamingServerRequest(info.url)) {
    return;
  }
  
  // Get WebContents for this request
  content::WebContents* web_contents = 
      content::WebContents::FromFrameTreeNodeId(info.frame_tree_node_id);
  
  if (!web_contents) {
    return;
  }
  
  // Get current nsite for this tab
  std::string npub = GetNsiteForTab(web_contents);
  if (npub.empty()) {
    return;
  }
  
  // Inject X-Npub header
  if (headers) {
    headers->SetHeader("X-Npub", npub);
    LOG(INFO) << "Injected header X-Npub: " << npub 
              << " for URL: " << info.url;
  }
}

bool NsiteHeaderInjector::IsStreamingServerRequest(const GURL& url) {
  if (!url.SchemeIs("http") || !net::IsLocalhost(url)) {
    return false;
  }
  
  uint16_t server_port = GetStreamingServerPort();
  if (server_port == 0) {
    return false;
  }
  
  return url.EffectiveIntPort() == server_port;
}

uint16_t NsiteHeaderInjector::GetStreamingServerPort() {
  if (!nsite_service_) {
    return 0;
  }
  
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  return nsite_service_->GetServerPort(profile);
}

}  // namespace nostr