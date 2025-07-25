// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the local relay handler implementations for NostrMessageRouter.
// These handlers are stubbed out until the LocalRelayService API is finalized.

#include "chrome/common/nostr_message_router.h"

#include "base/logging.h"
#include "chrome/browser/nostr/nostr_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/nostr_messages.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// Local relay handler implementations

void NostrMessageRouter::OnRelayQuery(int request_id,
                                      const base::Value::Dict& filter,
                                      int limit) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // TODO: Implement once LocalRelayService exposes query API
  // For now, return empty result
  LOG(WARNING) << "Local relay query not yet implemented";
  Send(new NostrMsg_RelayQueryResponse(
      routing_id(), request_id, false, std::vector<NostrEvent>()));
}

void NostrMessageRouter::OnRelayCount(int request_id,
                                      const base::Value::Dict& filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // TODO: Implement once LocalRelayService exposes count API
  LOG(WARNING) << "Local relay count not yet implemented";
  Send(new NostrMsg_RelayCountResponse(
      routing_id(), request_id, false, 0));
}

void NostrMessageRouter::OnRelayDelete(int request_id,
                                       const base::Value::Dict& filter,
                                       const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // Check permission for delete operation
  if (!CheckOriginPermission(origin, "relay.deleteEvents")) {
    Send(new NostrMsg_RelayDeleteResponse(
        routing_id(), request_id, false, 0));
    return;
  }
  
  // TODO: Implement once LocalRelayService exposes delete API
  LOG(WARNING) << "Local relay delete not yet implemented";
  Send(new NostrMsg_RelayDeleteResponse(
      routing_id(), request_id, false, 0));
}

void NostrMessageRouter::OnRelaySubscribe(int subscription_id,
                                          const base::Value::Dict& filter,
                                          const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // TODO: Implement once LocalRelayService exposes subscription API
  LOG(WARNING) << "Local relay subscribe not yet implemented";
  Send(new NostrMsg_RelaySubscribeResponse(
      routing_id(), subscription_id, false));
}

void NostrMessageRouter::OnRelayUnsubscribe(int subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // TODO: Implement once LocalRelayService exposes unsubscribe API
  LOG(WARNING) << "Local relay unsubscribe not yet implemented";
}

void NostrMessageRouter::OnRelayGetStatus(int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  
  // For now, return a hardcoded status indicating the relay is running
  // TODO: Get actual status from LocalRelayService
  bool connected = true;  // Assume relay is always running
  int64_t event_count = 0;  // TODO: Get from database
  int64_t storage_used = 0;  // TODO: Get from database
  
  Send(new NostrMsg_RelayStatusResponse(
      routing_id(), request_id, connected, event_count, storage_used));
}