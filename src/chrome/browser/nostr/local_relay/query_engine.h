// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_LOCAL_RELAY_QUERY_ENGINE_H_
#define CHROME_BROWSER_NOSTR_LOCAL_RELAY_QUERY_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace nostr {

// Forward declarations
struct NostrEvent;
struct NostrFilter;

namespace local_relay {

// Query execution plan for optimization
struct QueryPlan {
  // Primary table to query (events or replaceable_events)
  std::string primary_table = "events";
  
  // Whether to join with tags table
  bool needs_tag_join = false;
  
  // Estimated row count
  int estimated_rows = 0;
  
  // Indexes that will be used
  std::vector<std::string> indexes_used;
  
  // SQL query string
  std::string sql;
  
  // Query parameters
  std::vector<std::string> parameters;
};

// Query result for caching
struct QueryResult {
  std::vector<std::unique_ptr<NostrEvent>> events;
  base::Time query_time;
  base::TimeDelta execution_time;
};

// Optimizes and executes database queries for Nostr events
class QueryEngine {
 public:
  using StreamCallback = base::RepeatingCallback<void(std::unique_ptr<NostrEvent> event)>;
  
  explicit QueryEngine(sql::Database* database);
  ~QueryEngine();

  // Build optimized query plan from filters
  QueryPlan BuildQueryPlan(const std::vector<NostrFilter>& filters,
                           int limit = 1000,
                           bool reverse_order = false);
  
  // Execute query and return all results
  std::vector<std::unique_ptr<NostrEvent>> ExecuteQuery(
      const QueryPlan& plan);
  
  // Execute query with streaming results
  void ExecuteQueryStreaming(const QueryPlan& plan,
                            StreamCallback callback);
  
  // Get single event by ID (optimized)
  std::unique_ptr<NostrEvent> GetEventById(const std::string& event_id);
  
  // Get events by author (optimized)
  std::vector<std::unique_ptr<NostrEvent>> GetEventsByAuthor(
      const std::string& pubkey,
      int limit = 100);
  
  // Get replaceable event
  std::unique_ptr<NostrEvent> GetReplaceableEvent(
      const std::string& pubkey,
      int kind,
      const std::string& d_tag = "");
  
  // Clear query cache
  void ClearCache();
  
  // Get cache statistics
  base::Value::Dict GetCacheStats() const;
  
  // Enable/disable query caching
  void SetCachingEnabled(bool enabled) { caching_enabled_ = enabled; }

 private:
  // Build WHERE clause from filters
  std::string BuildWhereClause(const std::vector<NostrFilter>& filters,
                              std::vector<std::string>& parameters);
  
  // Build ORDER BY clause
  std::string BuildOrderByClause(bool reverse_order) const;
  
  // Optimize filter order for best index usage
  std::vector<NostrFilter> OptimizeFilterOrder(
      const std::vector<NostrFilter>& filters) const;
  
  // Estimate query cost
  int EstimateQueryCost(const std::string& where_clause) const;
  
  // Parse event from database row
  std::unique_ptr<NostrEvent> ParseEventFromRow(sql::Statement& statement) const;
  
  // Check if query result is in cache
  bool GetCachedResult(const std::string& cache_key,
                      std::vector<std::unique_ptr<NostrEvent>>& events);
  
  // Cache query result
  void CacheResult(const std::string& cache_key,
                  const std::vector<std::unique_ptr<NostrEvent>>& events);
  
  // Generate cache key from query plan
  std::string GenerateCacheKey(const QueryPlan& plan) const;
  
  // Log slow queries for analysis
  void LogSlowQuery(const QueryPlan& plan, base::TimeDelta execution_time);
  
  // Database connection (not owned)
  sql::Database* database_;
  
  // Query result cache
  base::LRUCache<std::string, std::unique_ptr<QueryResult>> query_cache_;
  
  // Whether caching is enabled
  bool caching_enabled_ = true;
  
  // Slow query threshold
  base::TimeDelta slow_query_threshold_ = base::Milliseconds(100);
  
  // Thread checker
  SEQUENCE_CHECKER(sequence_checker_);
  
  // Weak pointer factory
  base::WeakPtrFactory<QueryEngine> weak_factory_{this};
};

}  // namespace local_relay
}  // namespace nostr

#endif  // CHROME_BROWSER_NOSTR_LOCAL_RELAY_QUERY_ENGINE_H_