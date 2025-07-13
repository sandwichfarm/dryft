// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/query_engine.h"

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace local_relay {

class QueryEngineTest : public testing::Test {
 public:
  QueryEngineTest() = default;
  ~QueryEngineTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    
    // Create and open database
    database_ = std::make_unique<sql::Database>();
    ASSERT_TRUE(database_->Open(temp_dir_.GetPath().AppendASCII("test.db")));
    
    // Create minimal schema for testing
    ASSERT_TRUE(database_->Execute(
        "CREATE TABLE events ("
        "  id TEXT PRIMARY KEY,"
        "  pubkey TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL,"
        "  kind INTEGER NOT NULL,"
        "  content TEXT NOT NULL,"
        "  sig TEXT NOT NULL,"
        "  deleted INTEGER DEFAULT 0,"
        "  received_at INTEGER NOT NULL"
        ")"));
    
    ASSERT_TRUE(database_->Execute(
        "CREATE TABLE tags ("
        "  event_id TEXT NOT NULL,"
        "  tag_index INTEGER NOT NULL,"
        "  tag_name TEXT NOT NULL,"
        "  tag_value TEXT NOT NULL,"
        "  PRIMARY KEY (event_id, tag_index, tag_value)"
        ")"));
    
    // Create indexes
    ASSERT_TRUE(database_->Execute(
        "CREATE INDEX idx_events_pubkey ON events(pubkey)"));
    ASSERT_TRUE(database_->Execute(
        "CREATE INDEX idx_events_kind ON events(kind)"));
    ASSERT_TRUE(database_->Execute(
        "CREATE INDEX idx_events_created_at ON events(created_at)"));
    ASSERT_TRUE(database_->Execute(
        "CREATE INDEX idx_tags_name_value ON tags(tag_name, tag_value)"));
    
    // Create query engine
    query_engine_ = std::make_unique<QueryEngine>(database_.get());
  }

  void TearDown() override {
    query_engine_.reset();
    database_.reset();
  }

 protected:
  // Helper to insert test events
  void InsertTestEvent(const std::string& id,
                      const std::string& pubkey,
                      int64_t created_at,
                      int kind,
                      const std::string& content = "test") {
    sql::Statement stmt(database_->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO events (id, pubkey, created_at, kind, content, sig, "
        "deleted, received_at) VALUES (?, ?, ?, ?, ?, ?, 0, ?)"));
    
    stmt.BindString(0, id);
    stmt.BindString(1, pubkey);
    stmt.BindInt64(2, created_at);
    stmt.BindInt(3, kind);
    stmt.BindString(4, content);
    stmt.BindString(5, std::string(128, 'a'));  // Mock signature
    stmt.BindInt64(6, base::Time::Now().ToInternalValue());
    
    ASSERT_TRUE(stmt.Run());
  }
  
  // Helper to insert tags
  void InsertTag(const std::string& event_id,
                int tag_index,
                const std::string& tag_name,
                const std::string& tag_value) {
    sql::Statement stmt(database_->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO tags (event_id, tag_index, tag_name, tag_value) "
        "VALUES (?, ?, ?, ?)"));
    
    stmt.BindString(0, event_id);
    stmt.BindInt(1, tag_index);
    stmt.BindString(2, tag_name);
    stmt.BindString(3, tag_value);
    
    ASSERT_TRUE(stmt.Run());
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> database_;
  std::unique_ptr<QueryEngine> query_engine_;
};

// Test basic query plan building
TEST_F(QueryEngineTest, BuildQueryPlan) {
  NostrFilter filter;
  filter.authors.push_back("author1");
  filter.kinds.push_back(1);
  filter.since = 1000;
  
  auto plan = query_engine_->BuildQueryPlan({filter}, 100, false);
  
  EXPECT_EQ("events", plan.primary_table);
  EXPECT_FALSE(plan.needs_tag_join);
  
  // Should use author and kind indexes
  EXPECT_TRUE(std::find(plan.indexes_used.begin(), plan.indexes_used.end(),
                       "idx_events_pubkey") != plan.indexes_used.end());
  EXPECT_TRUE(std::find(plan.indexes_used.begin(), plan.indexes_used.end(),
                       "idx_events_kind") != plan.indexes_used.end());
  
  // Check SQL contains expected clauses
  EXPECT_NE(std::string::npos, plan.sql.find("pubkey = ?"));
  EXPECT_NE(std::string::npos, plan.sql.find("kind IN"));
  EXPECT_NE(std::string::npos, plan.sql.find("created_at >= ?"));
  EXPECT_NE(std::string::npos, plan.sql.find("LIMIT ?"));
  
  // Check parameters
  EXPECT_GE(plan.parameters.size(), 4u);  // author, kind, since, limit
}

// Test query plan with tag filters
TEST_F(QueryEngineTest, BuildQueryPlanWithTags) {
  NostrFilter filter;
  filter.tags["e"] = {"event123"};
  filter.tags["p"] = {"pubkey456"};
  
  auto plan = query_engine_->BuildQueryPlan({filter}, 50, false);
  
  EXPECT_TRUE(plan.needs_tag_join);
  EXPECT_TRUE(std::find(plan.indexes_used.begin(), plan.indexes_used.end(),
                       "idx_tags_name_value") != plan.indexes_used.end());
  
  // Should have EXISTS subqueries for tags
  EXPECT_NE(std::string::npos, plan.sql.find("EXISTS"));
  EXPECT_NE(std::string::npos, plan.sql.find("tags.tag_name = ?"));
}

// Test query execution
TEST_F(QueryEngineTest, ExecuteQuery) {
  // Insert test data
  InsertTestEvent("event1", "author1", 1000, 1, "Hello");
  InsertTestEvent("event2", "author1", 2000, 1, "World");
  InsertTestEvent("event3", "author2", 1500, 2, "Test");
  
  // Query by author
  NostrFilter filter;
  filter.authors.push_back("author1");
  
  auto plan = query_engine_->BuildQueryPlan({filter}, 10, false);
  auto events = query_engine_->ExecuteQuery(plan);
  
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ("author1", events[0]->pubkey);
  EXPECT_EQ("author1", events[1]->pubkey);
}

// Test query with multiple filters
TEST_F(QueryEngineTest, ExecuteQueryMultipleFilters) {
  // Insert test data
  InsertTestEvent("event1", "author1", 1000, 1);
  InsertTestEvent("event2", "author1", 2000, 2);
  InsertTestEvent("event3", "author2", 1500, 1);
  InsertTestEvent("event4", "author2", 2500, 2);
  
  // Query with OR semantics (multiple filters)
  NostrFilter filter1;
  filter1.authors.push_back("author1");
  filter1.kinds.push_back(1);
  
  NostrFilter filter2;
  filter2.kinds.push_back(2);
  filter2.since = 2000;
  
  auto plan = query_engine_->BuildQueryPlan({filter1, filter2}, 10, false);
  auto events = query_engine_->ExecuteQuery(plan);
  
  // Should match event1 (author1, kind1) and event2, event4 (kind2, since 2000)
  ASSERT_EQ(3u, events.size());
}

// Test time-based filtering
TEST_F(QueryEngineTest, TimeBasedFiltering) {
  // Insert events with different timestamps
  InsertTestEvent("event1", "author1", 1000, 1);
  InsertTestEvent("event2", "author1", 1500, 1);
  InsertTestEvent("event3", "author1", 2000, 1);
  InsertTestEvent("event4", "author1", 2500, 1);
  
  // Query with time range
  NostrFilter filter;
  filter.since = 1500;
  filter.until = 2000;
  
  auto plan = query_engine_->BuildQueryPlan({filter}, 10, false);
  auto events = query_engine_->ExecuteQuery(plan);
  
  ASSERT_EQ(2u, events.size());
  EXPECT_GE(events[0]->created_at, 1500);
  EXPECT_LE(events[0]->created_at, 2000);
  EXPECT_GE(events[1]->created_at, 1500);
  EXPECT_LE(events[1]->created_at, 2000);
}

// Test reverse order
TEST_F(QueryEngineTest, ReverseOrder) {
  // Insert events
  InsertTestEvent("event1", "author1", 1000, 1);
  InsertTestEvent("event2", "author1", 2000, 1);
  InsertTestEvent("event3", "author1", 3000, 1);
  
  NostrFilter filter;
  filter.authors.push_back("author1");
  
  // Normal order (DESC)
  auto plan = query_engine_->BuildQueryPlan({filter}, 10, false);
  auto events = query_engine_->ExecuteQuery(plan);
  
  ASSERT_EQ(3u, events.size());
  EXPECT_EQ(3000, events[0]->created_at);  // Newest first
  EXPECT_EQ(2000, events[1]->created_at);
  EXPECT_EQ(1000, events[2]->created_at);
  
  // Reverse order (ASC)
  plan = query_engine_->BuildQueryPlan({filter}, 10, true);
  events = query_engine_->ExecuteQuery(plan);
  
  ASSERT_EQ(3u, events.size());
  EXPECT_EQ(1000, events[0]->created_at);  // Oldest first
  EXPECT_EQ(2000, events[1]->created_at);
  EXPECT_EQ(3000, events[2]->created_at);
}

// Test prefix matching
TEST_F(QueryEngineTest, PrefixMatching) {
  // Insert events with similar IDs
  InsertTestEvent("abc123def", "author1", 1000, 1);
  InsertTestEvent("abc456def", "author1", 2000, 1);
  InsertTestEvent("xyz789def", "author1", 3000, 1);
  
  // Query with ID prefix
  NostrFilter filter;
  filter.ids.push_back("abc");  // Prefix match
  
  auto plan = query_engine_->BuildQueryPlan({filter}, 10, false);
  auto events = query_engine_->ExecuteQuery(plan);
  
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(events[0]->id.starts_with("abc"));
  EXPECT_TRUE(events[1]->id.starts_with("abc"));
}

// Test GetEventById
TEST_F(QueryEngineTest, GetEventById) {
  InsertTestEvent("unique123", "author1", 1000, 1, "Special event");
  
  auto event = query_engine_->GetEventById("unique123");
  
  ASSERT_TRUE(event);
  EXPECT_EQ("unique123", event->id);
  EXPECT_EQ("Special event", event->content);
  
  // Non-existent event
  auto missing = query_engine_->GetEventById("nonexistent");
  EXPECT_FALSE(missing);
}

// Test GetEventsByAuthor
TEST_F(QueryEngineTest, GetEventsByAuthor) {
  // Insert events from different authors
  InsertTestEvent("event1", "author1", 1000, 1);
  InsertTestEvent("event2", "author1", 2000, 1);
  InsertTestEvent("event3", "author2", 1500, 1);
  InsertTestEvent("event4", "author1", 3000, 1);
  
  auto events = query_engine_->GetEventsByAuthor("author1", 2);
  
  ASSERT_EQ(2u, events.size());  // Limited to 2
  EXPECT_EQ("author1", events[0]->pubkey);
  EXPECT_EQ("author1", events[1]->pubkey);
  EXPECT_EQ(3000, events[0]->created_at);  // Newest first
  EXPECT_EQ(2000, events[1]->created_at);
}

// Test cache functionality
TEST_F(QueryEngineTest, QueryCaching) {
  // Insert test data
  InsertTestEvent("event1", "author1", 1000, 1);
  
  NostrFilter filter;
  filter.authors.push_back("author1");
  
  // First query (cache miss)
  auto plan = query_engine_->BuildQueryPlan({filter}, 10, false);
  auto events1 = query_engine_->ExecuteQuery(plan);
  ASSERT_EQ(1u, events1.size());
  
  // Second query (should hit cache)
  auto events2 = query_engine_->ExecuteQuery(plan);
  ASSERT_EQ(1u, events2.size());
  
  // Verify cache stats
  auto stats = query_engine_->GetCacheStats();
  EXPECT_GT(*stats.FindInt("size"), 0);
  
  // Clear cache and query again
  query_engine_->ClearCache();
  auto events3 = query_engine_->ExecuteQuery(plan);
  ASSERT_EQ(1u, events3.size());
}

// Test streaming execution
TEST_F(QueryEngineTest, StreamingExecution) {
  // Insert multiple events
  for (int i = 0; i < 10; ++i) {
    InsertTestEvent("event" + base::NumberToString(i), 
                   "author1", 1000 + i, 1);
  }
  
  NostrFilter filter;
  filter.authors.push_back("author1");
  
  auto plan = query_engine_->BuildQueryPlan({filter}, 5, false);
  
  // Stream results
  std::vector<std::string> streamed_ids;
  query_engine_->ExecuteQueryStreaming(
      plan,
      base::BindRepeating([](std::vector<std::string>* ids,
                           std::unique_ptr<NostrEvent> event) {
        if (event) {
          ids->push_back(event->id);
        }
      }, &streamed_ids));
  
  EXPECT_EQ(5u, streamed_ids.size());  // Limited to 5
}

}  // namespace local_relay
}  // namespace nostr