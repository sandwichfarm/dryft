// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/integration_tests/mock_local_relay_service.h"

#include "base/json/json_reader.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/nostr/integration_tests/test_helpers.h"

using ::testing::_;
using ::testing::Invoke;

namespace nostr {

MockLocalRelayService::MockLocalRelayService() {
  SetDefaultBehavior();
}

MockLocalRelayService::~MockLocalRelayService() = default;

void MockLocalRelayService::SetDefaultBehavior() {
  ON_CALL(*this, IsReady())
      .WillByDefault(Invoke([this]() {
        return is_ready_;
      }));
  
  ON_CALL(*this, GetRelayURL())
      .WillByDefault(Invoke([]() {
        return "ws://localhost:4869";
      }));
  
  ON_CALL(*this, PublishEvent(_, _))
      .WillByDefault(Invoke([this](const std::string& event_json,
                                  PublishEventCallback callback) {
        if (!is_ready_) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false, "Relay not ready"));
          return;
        }
        
        // Validate JSON
        auto parsed = base::JSONReader::Read(event_json);
        if (!parsed || !parsed->is_dict()) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false, "Invalid JSON"));
          return;
        }
        
        test_events_.push_back(event_json);
        
        // Notify any subscriptions
        for (const auto& [sub_id, sub_callback] : subscriptions_) {
          if (sub_callback) {
            sub_callback.Run(event_json);
          }
        }
        
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), true, ""));
      }));
  
  ON_CALL(*this, QueryEvents(_, _))
      .WillByDefault(Invoke([this](const std::string& filter_json,
                                  QueryEventsCallback callback) {
        if (!is_ready_) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false,
                            std::vector<std::string>()));
          return;
        }
        
        // For testing, return all events
        // Real implementation would filter based on filter_json
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), true, test_events_));
      }));
  
  ON_CALL(*this, GetEventById(_, _))
      .WillByDefault(Invoke([this](const std::string& event_id,
                                  GetEventByIdCallback callback) {
        if (!is_ready_) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false, ""));
          return;
        }
        
        // Search for event with matching ID
        for (const auto& event_json : test_events_) {
          auto parsed = base::JSONReader::Read(event_json);
          if (parsed && parsed->is_dict()) {
            const auto* id = parsed->GetDict().FindString("id");
            if (id && *id == event_id) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), true, event_json));
              return;
            }
          }
        }
        
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), false, ""));
      }));
  
  ON_CALL(*this, CountEvents(_, _))
      .WillByDefault(Invoke([this](const std::string& filter_json,
                                  CountEventsCallback callback) {
        if (!is_ready_) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false, 0));
          return;
        }
        
        // For testing, return count of all events
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), true, test_events_.size()));
      }));
  
  ON_CALL(*this, Subscribe(_, _, _))
      .WillByDefault(Invoke([this](const std::string& subscription_id,
                                  const std::string& filter_json,
                                  SubscriptionCallback callback) {
        if (!is_ready_) {
          return;
        }
        
        subscriptions_[subscription_id] = callback;
        
        // Send existing events that match (for testing, send all)
        for (const auto& event : test_events_) {
          if (callback) {
            callback.Run(event);
          }
        }
      }));
  
  ON_CALL(*this, Unsubscribe(_))
      .WillByDefault(Invoke([this](const std::string& subscription_id) {
        subscriptions_.erase(subscription_id);
      }));
  
  ON_CALL(*this, GetStorageStats())
      .WillByDefault(Invoke([this]() {
        return test_stats_;
      }));
  
  ON_CALL(*this, DeleteEvents(_, _))
      .WillByDefault(Invoke([this](const std::vector<std::string>& event_ids,
                                  DeleteEventsCallback callback) {
        if (!is_ready_) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), false, 0));
          return;
        }
        
        // For testing, just report success without actually deleting
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), true, event_ids.size()));
      }));
}

void MockLocalRelayService::SetReady(bool ready) {
  is_ready_ = ready;
}

void MockLocalRelayService::AddTestEvent(const std::string& event_json) {
  test_events_.push_back(event_json);
}

void MockLocalRelayService::ClearTestEvents() {
  test_events_.clear();
}

void MockLocalRelayService::SimulateSubscriptionUpdate(
    const std::string& subscription_id,
    const std::string& event_json) {
  auto it = subscriptions_.find(subscription_id);
  if (it != subscriptions_.end() && it->second) {
    it->second.Run(event_json);
  }
}

void MockLocalRelayService::SetStorageStats(const StorageStats& stats) {
  test_stats_ = stats;
}

}  // namespace nostr