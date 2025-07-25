// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_MOCK_LOCAL_RELAY_SERVICE_H_
#define CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_MOCK_LOCAL_RELAY_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/nostr/local_relay/local_relay_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace nostr {

class MockLocalRelayService : public LocalRelayService {
 public:
  MockLocalRelayService();
  ~MockLocalRelayService() override;

  // LocalRelayService implementation
  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(bool, IsReady, (), (const, override));
  MOCK_METHOD(std::string, GetRelayURL, (), (const, override));
  
  MOCK_METHOD(void, PublishEvent,
              (const std::string& event_json,
               PublishEventCallback callback),
              (override));
  
  MOCK_METHOD(void, QueryEvents,
              (const std::string& filter_json,
               QueryEventsCallback callback),
              (override));
  
  MOCK_METHOD(void, DeleteEvents,
              (const std::vector<std::string>& event_ids,
               DeleteEventsCallback callback),
              (override));
  
  MOCK_METHOD(void, GetEventById,
              (const std::string& event_id,
               GetEventByIdCallback callback),
              (override));
  
  MOCK_METHOD(void, CountEvents,
              (const std::string& filter_json,
               CountEventsCallback callback),
              (override));
  
  MOCK_METHOD(void, Subscribe,
              (const std::string& subscription_id,
               const std::string& filter_json,
               SubscriptionCallback callback),
              (override));
  
  MOCK_METHOD(void, Unsubscribe,
              (const std::string& subscription_id),
              (override));
  
  MOCK_METHOD(StorageStats, GetStorageStats, (), (const, override));
  
  MOCK_METHOD(void, ImportEvents,
              (const std::vector<std::string>& events,
               ImportEventsCallback callback),
              (override));
  
  MOCK_METHOD(void, ExportEvents,
              (const std::string& filter_json,
               ExportEventsCallback callback),
              (override));

  // Helper methods for testing
  void SetDefaultBehavior();
  void SetReady(bool ready);
  void AddTestEvent(const std::string& event_json);
  void ClearTestEvents();
  void SimulateSubscriptionUpdate(const std::string& subscription_id,
                                 const std::string& event_json);
  void SetStorageStats(const StorageStats& stats);

 private:
  bool is_ready_ = false;
  std::vector<std::string> test_events_;
  std::map<std::string, SubscriptionCallback> subscriptions_;
  StorageStats test_stats_;
};

}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_INTEGRATION_TESTS_MOCK_LOCAL_RELAY_SERVICE_H_