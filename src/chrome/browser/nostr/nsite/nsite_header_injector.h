// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_NSITE_NSITE_HEADER_INJECTOR_H_
#define CHROME_BROWSER_NOSTR_NSITE_NSITE_HEADER_INJECTOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace nostr {

class NsiteService;

// Tracks nsite navigation context for a WebContents
class NsiteNavigationContext : public content::WebContentsObserver {
 public:
  explicit NsiteNavigationContext(content::WebContents* web_contents);
  ~NsiteNavigationContext() override;

  // Set the current nsite for this tab
  void SetCurrentNsite(const std::string& npub);
  
  // Get the current nsite for this tab
  std::string GetCurrentNsite() const;
  
  // Clear the current nsite
  void ClearNsite();

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  mutable base::Lock lock_;
  std::string current_npub_ GUARDED_BY(lock_);
  
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NsiteNavigationContext> weak_factory_{this};
};

// Injects X-Npub headers for requests to the streaming server
class NsiteHeaderInjector {
 public:
  explicit NsiteHeaderInjector(content::BrowserContext* browser_context);
  ~NsiteHeaderInjector();

  // Initialize the header injection system
  void Initialize();
  
  // Shutdown the header injection system
  void Shutdown();

  // Set nsite context for a specific tab
  void SetNsiteForTab(content::WebContents* web_contents, 
                      const std::string& npub);
  
  // Clear nsite context for a specific tab
  void ClearNsiteForTab(content::WebContents* web_contents);
  
  // Get current nsite for a tab
  std::string GetNsiteForTab(content::WebContents* web_contents);

 private:
  // Web request callback for modifying headers
  void OnBeforeRequest(
      const extensions::WebRequestInfo& info,
      extensions::WebRequestAPI::RequestStage stage,
      net::HttpRequestHeaders* headers,
      extensions::WebRequestAPI::Response* response);

  // Check if URL is targeting the streaming server
  bool IsStreamingServerRequest(const GURL& url);
  
  // Get streaming server port
  uint16_t GetStreamingServerPort();

  content::BrowserContext* browser_context_;
  NsiteService* nsite_service_;
  
  // Track navigation contexts by WebContents
  mutable base::Lock contexts_lock_;
  std::map<content::WebContents*, std::unique_ptr<NsiteNavigationContext>> 
      navigation_contexts_ GUARDED_BY(contexts_lock_);
  
  bool initialized_ = false;
  
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NsiteHeaderInjector> weak_factory_{this};
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_NSITE_NSITE_HEADER_INJECTOR_H_