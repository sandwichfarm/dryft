// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/event_storage.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nostr/local_relay/nostr_database.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace local_relay {

class EventStorageTest : public testing::Test {
 public:
  EventStorageTest() = default;
  ~EventStorageTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    
    // Create database
    NostrDatabase::Config db_config;
    db_config.max_size_bytes = 10 * 1024 * 1024;  // 10MB
    db_config.max_event_count = 1000;
    
    database_ = std::make_unique<NostrDatabase>(
        temp_dir_.GetPath().AppendASCII("test.db"), db_config);
    
    // Initialize database
    base::RunLoop init_loop;
    database_->Initialize(base::BindOnce(
        [](base::OnceClosure quit, bool success) {
          ASSERT_TRUE(success);
          std::move(quit).Run();
        },
        init_loop.QuitClosure()));
    init_loop.Run();
    
    // Create event storage
    event_storage_ = std::make_unique<EventStorage>(database_.get());
  }

  void TearDown() override {
    event_storage_.reset();
    database_.reset();
  }

 protected:
  // Helper to create a test event
  std::unique_ptr<NostrEvent> CreateTestEvent(
      const std::string& id = "",
      const std::string& content = "Test event",
      int kind = 1) {
    auto event = std::make_unique<NostrEvent>();
    
    // Generate or use provided ID
    if (id.empty()) {
      event->id = std::string(64, 'a');
      // Make ID unique by including timestamp
      auto now = base::Time::Now().ToTimeT();
      std::string time_str = base::NumberToString(now);
      for (size_t i = 0; i < time_str.length() && i < 64; ++i) {
        event->id[i] = time_str[i];
      }
    } else {
      event->id = id;
    }
    
    event->pubkey = std::string(64, 'b');
    event->created_at = base::Time::Now().ToTimeT();
    event->kind = kind;
    event->content = content;
    event->sig = std::string(128, 'c');
    
    // Add some tags
    base::Value::List tag1;
    tag1.Append("e");
    tag1.Append("referenced_event");
    event->tags.Append(std::move(tag1));
    
    base::Value::List tag2;
    tag2.Append("p");
    tag2.Append("referenced_pubkey");
    event->tags.Append(std::move(tag2));
    
    return event;
  }
  
  // Helper to wait for async operation
  bool StoreEventAndWait(std::unique_ptr<NostrEvent> event,
                        std::string* error_out = nullptr) {
    base::RunLoop run_loop;
    bool result = false;
    std::string error;
    
    event_storage_->StoreEvent(
        std::move(event),
        base::BindOnce(
            [](bool* out_result, std::string* out_error,
               base::OnceClosure quit,
               bool success, const std::string& error) {
              *out_result = success;
              *out_error = error;
              std::move(quit).Run();
            },
            &result, &error, run_loop.QuitClosure()));
    
    run_loop.Run();
    
    if (error_out) {
      *error_out = error;
    }
    
    return result;
  }
  
  std::vector<std::unique_ptr<NostrEvent>> QueryEventsAndWait(
      const std::vector<NostrFilter>& filters,
      const QueryOptions& options = QueryOptions()) {
    base::RunLoop run_loop;
    std::vector<std::unique_ptr<NostrEvent>> results;
    
    event_storage_->QueryEvents(
        filters,
        options,
        base::BindOnce(
            [](std::vector<std::unique_ptr<NostrEvent>>* out_results,
               base::OnceClosure quit,
               std::vector<std::unique_ptr<NostrEvent>> events) {
              *out_results = std::move(events);
              std::move(quit).Run();
            },
            &results, run_loop.QuitClosure()));
    
    run_loop.Run();
    return results;
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<NostrDatabase> database_;
  std::unique_ptr<EventStorage> event_storage_;
};

// Test storing and retrieving a single event
TEST_F(EventStorageTest, StoreAndRetrieveEvent) {
  auto event = CreateTestEvent("test_event_001", "Hello, Nostr!");
  std::string event_id = event->id;
  
  // Store event
  EXPECT_TRUE(StoreEventAndWait(std::move(event)));
  
  // Query event back
  NostrFilter filter;
  filter.ids.push_back(event_id);
  
  auto results = QueryEventsAndWait({filter});
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(event_id, results[0]->id);
  EXPECT_EQ("Hello, Nostr!", results[0]->content);
}

// Test storing duplicate events
TEST_F(EventStorageTest, StoreDuplicateEvent) {
  auto event1 = CreateTestEvent("duplicate_001", "First");
  auto event2 = CreateTestEvent("duplicate_001", "Second");
  
  // Store first event
  EXPECT_TRUE(StoreEventAndWait(std::move(event1)));
  
  // Try to store duplicate
  std::string error;
  EXPECT_FALSE(StoreEventAndWait(std::move(event2), &error));
  EXPECT_FALSE(error.empty());
}

// Test event validation
TEST_F(EventStorageTest, EventValidation) {
  // Invalid ID length
  auto event = CreateTestEvent();
  event->id = "too_short";
  EXPECT_FALSE(StoreEventAndWait(std::move(event)));
  
  // Invalid pubkey length
  event = CreateTestEvent();
  event->pubkey = "invalid";
  EXPECT_FALSE(StoreEventAndWait(std::move(event)));
  
  // Invalid signature length
  event = CreateTestEvent();
  event->sig = "invalid_sig";
  EXPECT_FALSE(StoreEventAndWait(std::move(event)));
  
  // Invalid timestamp
  event = CreateTestEvent();
  event->created_at = 0;
  EXPECT_FALSE(StoreEventAndWait(std::move(event)));
}

// Test querying with filters
TEST_F(EventStorageTest, QueryWithFilters) {
  // Store events from different authors
  std::string author1 = std::string(64, 'a');
  std::string author2 = std::string(64, 'b');
  
  for (int i = 0; i < 5; ++i) {
    auto event = CreateTestEvent();
    event->pubkey = (i % 2 == 0) ? author1 : author2;
    event->kind = i % 3;  // Kinds 0, 1, 2
    EXPECT_TRUE(StoreEventAndWait(std::move(event)));
  }
  
  // Query by author
  NostrFilter author_filter;
  author_filter.authors.push_back(author1);
  
  auto results = QueryEventsAndWait({author_filter});
  EXPECT_EQ(3U, results.size());  // Events 0, 2, 4
  for (const auto& event : results) {
    EXPECT_EQ(author1, event->pubkey);
  }
  
  // Query by kind
  NostrFilter kind_filter;
  kind_filter.kinds.push_back(1);
  
  results = QueryEventsAndWait({kind_filter});
  EXPECT_EQ(2U, results.size());  // Events 1, 4
  for (const auto& event : results) {
    EXPECT_EQ(1, event->kind);
  }
}

// Test replaceable events
TEST_F(EventStorageTest, ReplaceableEvents) {
  std::string author = std::string(64, 'a');
  
  // Store initial metadata event (kind 0 is replaceable)
  auto event1 = CreateTestEvent("", "Metadata v1", 0);
  event1->pubkey = author;
  event1->created_at = 1000;
  EXPECT_TRUE(StoreEventAndWait(std::move(event1)));
  
  // Store newer metadata
  auto event2 = CreateTestEvent("", "Metadata v2", 0);
  event2->pubkey = author;
  event2->created_at = 2000;
  EXPECT_TRUE(StoreEventAndWait(std::move(event2)));
  
  // Query should return only the newer one
  NostrFilter filter;
  filter.authors.push_back(author);
  filter.kinds.push_back(0);
  
  auto results = QueryEventsAndWait({filter});
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ("Metadata v2", results[0]->content);
}

// Test time-based filters
TEST_F(EventStorageTest, TimeBasedFilters) {
  // Store events with different timestamps
  for (int i = 0; i < 5; ++i) {
    auto event = CreateTestEvent();
    event->created_at = 1000 + i * 100;  // 1000, 1100, 1200, 1300, 1400
    EXPECT_TRUE(StoreEventAndWait(std::move(event)));
  }
  
  // Query with since filter
  NostrFilter since_filter;
  since_filter.since = 1200;
  
  auto results = QueryEventsAndWait({since_filter});
  EXPECT_EQ(3U, results.size());  // Events at 1200, 1300, 1400
  
  // Query with until filter
  NostrFilter until_filter;
  until_filter.until = 1200;
  
  results = QueryEventsAndWait({until_filter});
  EXPECT_EQ(3U, results.size());  // Events at 1000, 1100, 1200
}

// Test batch storage
TEST_F(EventStorageTest, BatchStorage) {
  std::vector<std::unique_ptr<NostrEvent>> events;
  for (int i = 0; i < 10; ++i) {
    events.push_back(CreateTestEvent());
  }
  
  base::RunLoop run_loop;
  int stored_count = 0;
  
  event_storage_->StoreEvents(
      std::move(events),
      base::BindOnce(
          [](int* out_count, base::OnceClosure quit, int count) {
            *out_count = count;
            std::move(quit).Run();
          },
          &stored_count, run_loop.QuitClosure()));
  
  run_loop.Run();
  
  EXPECT_EQ(10, stored_count);
}

// Test storage statistics
TEST_F(EventStorageTest, StorageStats) {
  // Store some events
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(StoreEventAndWait(CreateTestEvent()));
  }
  
  base::RunLoop run_loop;
  StorageStats stats;
  
  event_storage_->GetStorageStats(
      base::BindOnce(
          [](StorageStats* out_stats, base::OnceClosure quit,
             const StorageStats& stats) {
            *out_stats = stats;
            std::move(quit).Run();
          },
          &stats, run_loop.QuitClosure()));
  
  run_loop.Run();
  
  EXPECT_EQ(5, stats.total_events);
  EXPECT_GT(stats.total_size_bytes, 0);
}

// Test event deletion
TEST_F(EventStorageTest, DeleteEvent) {
  auto event = CreateTestEvent("to_delete", "Delete me");
  std::string event_id = event->id;
  
  // Store event
  EXPECT_TRUE(StoreEventAndWait(std::move(event)));
  
  // Delete event
  base::RunLoop delete_loop;
  bool delete_success = false;
  
  event_storage_->DeleteEvent(
      event_id,
      base::BindOnce(
          [](bool* out_success, base::OnceClosure quit, bool success) {
            *out_success = success;
            std::move(quit).Run();
          },
          &delete_success, delete_loop.QuitClosure()));
  
  delete_loop.Run();
  EXPECT_TRUE(delete_success);
  
  // Query should return empty
  NostrFilter filter;
  filter.ids.push_back(event_id);
  
  auto results = QueryEventsAndWait({filter});
  EXPECT_EQ(0U, results.size());
}

// Test streaming query
TEST_F(EventStorageTest, StreamingQuery) {
  // Store multiple events
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(StoreEventAndWait(CreateTestEvent()));
  }
  
  // Query with streaming
  base::RunLoop run_loop;
  std::vector<std::string> received_ids;
  
  NostrFilter filter;  // Match all
  QueryOptions options;
  options.limit = 5;
  
  event_storage_->QueryEventsStreaming(
      {filter},
      options,
      base::BindRepeating(
          [](std::vector<std::string>* ids,
             std::unique_ptr<NostrEvent> event) {
            if (event) {
              ids->push_back(event->id);
            }
          },
          &received_ids),
      run_loop.QuitClosure());
  
  run_loop.Run();
  
  EXPECT_EQ(5U, received_ids.size());
}

}  // namespace local_relay
}  // namespace nostr