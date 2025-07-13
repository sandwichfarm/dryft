// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/nostr_database.h"

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace local_relay {

class NostrDatabaseTest : public testing::Test {
 public:
  NostrDatabaseTest() = default;
  ~NostrDatabaseTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    
    // Create database with test configuration
    NostrDatabase::Config config;
    config.max_size_bytes = 10 * 1024 * 1024;  // 10MB for testing
    config.max_event_count = 1000;
    config.auto_vacuum = true;
    
    db_ = std::make_unique<NostrDatabase>(
        temp_dir_.GetPath().AppendASCII("nostr_test.db"), config);
    
    // Initialize database
    base::RunLoop run_loop;
    db_->Initialize(base::BindOnce(&NostrDatabaseTest::OnInitialized,
                                  base::Unretained(this),
                                  run_loop.QuitClosure()));
    run_loop.Run();
    
    ASSERT_TRUE(initialized_);
  }

  void TearDown() override {
    db_.reset();
  }

 protected:
  // Helper to create a test event
  std::unique_ptr<NostrEvent> CreateTestEvent(
      const std::string& id,
      const std::string& pubkey,
      int kind,
      const std::string& content) {
    auto event = std::make_unique<NostrEvent>();
    event->id = id;
    event->pubkey = pubkey;
    event->created_at = base::Time::Now().ToTimeT();
    event->kind = kind;
    event->content = content;
    event->sig = std::string(128, 'a');  // Mock signature
    event->received_at = base::Time::Now();
    
    // Add some test tags
    base::Value::List tag1;
    tag1.Append("e");
    tag1.Append("referenced_event_id");
    event->tags.Append(std::move(tag1));
    
    base::Value::List tag2;
    tag2.Append("p");
    tag2.Append("referenced_pubkey");
    event->tags.Append(std::move(tag2));
    
    return event;
  }

  // Helper to wait for async operation
  template<typename T>
  T WaitForResult(base::OnceCallback<void(base::OnceCallback<void(T)>)> async_op) {
    base::RunLoop run_loop;
    T result;
    
    std::move(async_op).Run(base::BindOnce(
        [](T* out_result, base::OnceClosure quit, T result) {
          *out_result = std::move(result);
          std::move(quit).Run();
        },
        &result, run_loop.QuitClosure()));
    
    run_loop.Run();
    return result;
  }

  void OnInitialized(base::OnceClosure quit_closure, bool success) {
    initialized_ = success;
    std::move(quit_closure).Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<NostrDatabase> db_;
  bool initialized_ = false;
};

// Test database initialization
TEST_F(NostrDatabaseTest, Initialization) {
  // Database should be initialized in SetUp
  EXPECT_TRUE(initialized_);
  
  // Check schema version
  EXPECT_EQ(1, db_->GetSchemaVersion());
}

// Test storing and retrieving a single event
TEST_F(NostrDatabaseTest, StoreAndRetrieveEvent) {
  // Create test event
  auto event = CreateTestEvent(
      "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
      "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
      1,  // Text note
      "Hello, Nostr!");
  
  std::string event_id = event->id;
  
  // Store event
  bool store_success = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(event)));
  EXPECT_TRUE(store_success);
  
  // Retrieve event
  auto retrieved = WaitForResult<std::unique_ptr<NostrEvent>>(
      base::BindOnce(&NostrDatabase::GetEvent, base::Unretained(db_.get()),
                     event_id));
  
  ASSERT_TRUE(retrieved);
  EXPECT_EQ(event_id, retrieved->id);
  EXPECT_EQ("Hello, Nostr!", retrieved->content);
  EXPECT_EQ(1, retrieved->kind);
}

// Test storing duplicate events
TEST_F(NostrDatabaseTest, StoreDuplicateEvent) {
  auto event1 = CreateTestEvent("duplicate_id", "pubkey1", 1, "First");
  auto event2 = CreateTestEvent("duplicate_id", "pubkey2", 1, "Second");
  
  // Store first event
  bool success1 = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(event1)));
  EXPECT_TRUE(success1);
  
  // Try to store duplicate
  bool success2 = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(event2)));
  EXPECT_FALSE(success2);  // Should fail
}

// Test querying events with filters
TEST_F(NostrDatabaseTest, QueryEventsWithFilters) {
  // Store multiple events
  std::vector<std::string> event_ids;
  for (int i = 0; i < 5; ++i) {
    auto event = CreateTestEvent(
        base::StringPrintf("event%d", i),
        "test_author",
        i % 2,  // Alternating kinds 0 and 1
        base::StringPrintf("Content %d", i));
    
    event_ids.push_back(event->id);
    
    bool success = WaitForResult<bool>(
        base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                       std::move(event)));
    EXPECT_TRUE(success);
  }
  
  // Query by author
  NostrFilter author_filter;
  author_filter.authors.push_back("test_author");
  
  auto author_results = WaitForResult<std::vector<std::unique_ptr<NostrEvent>>>(
      base::BindOnce(&NostrDatabase::QueryEvents, base::Unretained(db_.get()),
                     std::vector<NostrFilter>{author_filter}, 10));
  
  EXPECT_EQ(5U, author_results.size());
  
  // Query by kind
  NostrFilter kind_filter;
  kind_filter.kinds.push_back(1);
  
  auto kind_results = WaitForResult<std::vector<std::unique_ptr<NostrEvent>>>(
      base::BindOnce(&NostrDatabase::QueryEvents, base::Unretained(db_.get()),
                     std::vector<NostrFilter>{kind_filter}, 10));
  
  EXPECT_EQ(2U, kind_results.size());  // Events 1 and 3
  for (const auto& event : kind_results) {
    EXPECT_EQ(1, event->kind);
  }
}

// Test replaceable events (NIP-16)
TEST_F(NostrDatabaseTest, ReplaceableEvents) {
  // Store initial metadata event (kind 0)
  auto metadata1 = CreateTestEvent("metadata1", "author1", 0, 
                                  "{\"name\":\"Alice\"}");
  metadata1->created_at = 1000;
  
  bool success1 = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(metadata1)));
  EXPECT_TRUE(success1);
  
  // Store newer metadata event for same author
  auto metadata2 = CreateTestEvent("metadata2", "author1", 0,
                                  "{\"name\":\"Alice Updated\"}");
  metadata2->created_at = 2000;
  
  bool success2 = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(metadata2)));
  EXPECT_TRUE(success2);
  
  // Query should return only the newer event
  NostrFilter filter;
  filter.authors.push_back("author1");
  filter.kinds.push_back(0);
  
  auto results = WaitForResult<std::vector<std::unique_ptr<NostrEvent>>>(
      base::BindOnce(&NostrDatabase::QueryEvents, base::Unretained(db_.get()),
                     std::vector<NostrFilter>{filter}, 10));
  
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ("metadata2", results[0]->id);
  EXPECT_EQ("{\"name\":\"Alice Updated\"}", results[0]->content);
}

// Test parameterized replaceable events (NIP-33)
TEST_F(NostrDatabaseTest, ParameterizedReplaceableEvents) {
  // Create event with 'd' tag
  auto event1 = CreateTestEvent("param1", "author1", 30000, "Version 1");
  base::Value::List d_tag;
  d_tag.Append("d");
  d_tag.Append("article-slug");
  event1->tags.Append(std::move(d_tag));
  event1->created_at = 1000;
  
  bool success1 = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(event1)));
  EXPECT_TRUE(success1);
  
  // Create newer version with same 'd' tag
  auto event2 = CreateTestEvent("param2", "author1", 30000, "Version 2");
  base::Value::List d_tag2;
  d_tag2.Append("d");
  d_tag2.Append("article-slug");
  event2->tags.Append(std::move(d_tag2));
  event2->created_at = 2000;
  
  bool success2 = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(event2)));
  EXPECT_TRUE(success2);
  
  // Get current replaceable event
  auto current = WaitForResult<std::unique_ptr<NostrEvent>>(
      base::BindOnce(&NostrDatabase::GetReplaceableEvent, 
                     base::Unretained(db_.get()),
                     "author1", 30000, "article-slug"));
  
  ASSERT_TRUE(current);
  EXPECT_EQ("param2", current->id);
  EXPECT_EQ("Version 2", current->content);
}

// Test event deletion (soft delete)
TEST_F(NostrDatabaseTest, DeleteEvent) {
  auto event = CreateTestEvent("to_delete", "author1", 1, "Delete me");
  std::string event_id = event->id;
  
  // Store event
  bool store_success = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(event)));
  EXPECT_TRUE(store_success);
  
  // Delete event
  bool delete_success = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::DeleteEvent, base::Unretained(db_.get()),
                     event_id));
  EXPECT_TRUE(delete_success);
  
  // Try to retrieve deleted event
  auto retrieved = WaitForResult<std::unique_ptr<NostrEvent>>(
      base::BindOnce(&NostrDatabase::GetEvent, base::Unretained(db_.get()),
                     event_id));
  
  EXPECT_FALSE(retrieved);  // Should not be found
}

// Test NIP-09 deletion events
TEST_F(NostrDatabaseTest, ProcessDeletionEvent) {
  // Store original event
  auto original = CreateTestEvent("original_event", "author1", 1, "Original");
  std::string original_id = original->id;
  
  bool store_success = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                     std::move(original)));
  EXPECT_TRUE(store_success);
  
  // Create deletion event
  auto deletion = CreateTestEvent("deletion_event", "author1", 5, "");
  base::Value::List e_tag;
  e_tag.Append("e");
  e_tag.Append(original_id);
  deletion->tags.Clear();
  deletion->tags.Append(std::move(e_tag));
  
  // Process deletion
  bool delete_success = WaitForResult<bool>(
      base::BindOnce(&NostrDatabase::ProcessDeletionEvent, 
                     base::Unretained(db_.get()),
                     *deletion));
  EXPECT_TRUE(delete_success);
  
  // Original event should be marked as deleted
  auto retrieved = WaitForResult<std::unique_ptr<NostrEvent>>(
      base::BindOnce(&NostrDatabase::GetEvent, base::Unretained(db_.get()),
                     original_id));
  
  EXPECT_FALSE(retrieved);  // Should be deleted
}

// Test time-based filters
TEST_F(NostrDatabaseTest, TimeBasedFilters) {
  // Store events with different timestamps
  for (int i = 0; i < 5; ++i) {
    auto event = CreateTestEvent(
        base::StringPrintf("time_event%d", i),
        "time_author",
        1,
        base::StringPrintf("Time %d", i));
    event->created_at = 1000 + i * 100;  // 1000, 1100, 1200, 1300, 1400
    
    bool success = WaitForResult<bool>(
        base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                       std::move(event)));
    EXPECT_TRUE(success);
  }
  
  // Query with since filter
  NostrFilter since_filter;
  since_filter.since = 1200;
  
  auto since_results = WaitForResult<std::vector<std::unique_ptr<NostrEvent>>>(
      base::BindOnce(&NostrDatabase::QueryEvents, base::Unretained(db_.get()),
                     std::vector<NostrFilter>{since_filter}, 10));
  
  EXPECT_EQ(3U, since_results.size());  // Events at 1200, 1300, 1400
  
  // Query with until filter
  NostrFilter until_filter;
  until_filter.until = 1200;
  
  auto until_results = WaitForResult<std::vector<std::unique_ptr<NostrEvent>>>(
      base::BindOnce(&NostrDatabase::QueryEvents, base::Unretained(db_.get()),
                     std::vector<NostrFilter>{until_filter}, 10));
  
  EXPECT_EQ(3U, until_results.size());  // Events at 1000, 1100, 1200
  
  // Query with both
  NostrFilter range_filter;
  range_filter.since = 1100;
  range_filter.until = 1300;
  
  auto range_results = WaitForResult<std::vector<std::unique_ptr<NostrEvent>>>(
      base::BindOnce(&NostrDatabase::QueryEvents, base::Unretained(db_.get()),
                     std::vector<NostrFilter>{range_filter}, 10));
  
  EXPECT_EQ(3U, range_results.size());  // Events at 1100, 1200, 1300
}

// Test limit enforcement
TEST_F(NostrDatabaseTest, QueryLimit) {
  // Store 10 events
  for (int i = 0; i < 10; ++i) {
    auto event = CreateTestEvent(
        base::StringPrintf("limit_event%d", i),
        "limit_author",
        1,
        base::StringPrintf("Limit %d", i));
    
    bool success = WaitForResult<bool>(
        base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                       std::move(event)));
    EXPECT_TRUE(success);
  }
  
  // Query with limit
  NostrFilter filter;
  filter.authors.push_back("limit_author");
  
  auto results = WaitForResult<std::vector<std::unique_ptr<NostrEvent>>>(
      base::BindOnce(&NostrDatabase::QueryEvents, base::Unretained(db_.get()),
                     std::vector<NostrFilter>{filter}, 5));  // Limit to 5
  
  EXPECT_EQ(5U, results.size());
}

// Test database statistics
TEST_F(NostrDatabaseTest, DatabaseStats) {
  // Store some events
  for (int i = 0; i < 3; ++i) {
    auto event = CreateTestEvent(
        base::StringPrintf("stats_event%d", i),
        "stats_author",
        i,  // Different kinds
        "Stats content");
    
    bool success = WaitForResult<bool>(
        base::BindOnce(&NostrDatabase::StoreEvent, base::Unretained(db_.get()),
                       std::move(event)));
    EXPECT_TRUE(success);
  }
  
  // Get stats
  auto stats = WaitForResult<base::Value::Dict>(
      base::BindOnce(&NostrDatabase::GetDatabaseStats, 
                     base::Unretained(db_.get())));
  
  // Check some basic stats exist
  EXPECT_TRUE(stats.contains("total_events"));
  EXPECT_TRUE(stats.contains("database_size_bytes"));
  
  // Count total events
  int64_t count = WaitForResult<int64_t>(
      base::BindOnce(&NostrDatabase::CountEvents, base::Unretained(db_.get())));
  EXPECT_EQ(3, count);
  
  // Count by author
  int64_t author_count = WaitForResult<int64_t>(
      base::BindOnce(&NostrDatabase::CountEventsByAuthor, 
                     base::Unretained(db_.get()),
                     "stats_author"));
  EXPECT_EQ(3, author_count);
}

}  // namespace local_relay
}  // namespace nostr