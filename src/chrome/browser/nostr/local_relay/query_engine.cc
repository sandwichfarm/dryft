// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/query_engine.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/nostr/nostr_event.h"
#include "components/nostr/nostr_filter.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace nostr {
namespace local_relay {

namespace {

// Cache configuration
constexpr size_t kMaxCacheSize = 100;  // Maximum number of cached queries
constexpr base::TimeDelta kCacheTTL = base::Minutes(5);  // Cache time-to-live

// Build SQL for tag filters
std::string BuildTagFilterSQL(const std::map<std::string, std::vector<std::string>>& tags,
                             std::vector<std::string>& parameters) {
  if (tags.empty()) {
    return "";
  }
  
  std::vector<std::string> tag_conditions;
  for (const auto& [tag_name, tag_values] : tags) {
    if (tag_values.empty()) continue;
    
    std::vector<std::string> value_conditions;
    for (const auto& value : tag_values) {
      parameters.push_back(tag_name);
      parameters.push_back(value);
      value_conditions.push_back(
          "EXISTS (SELECT 1 FROM tags WHERE tags.event_id = events.id "
          "AND tags.tag_name = ? AND tags.tag_value = ?)");
    }
    
    if (!value_conditions.empty()) {
      tag_conditions.push_back(
          "(" + base::JoinString(value_conditions, " OR ") + ")");
    }
  }
  
  return base::JoinString(tag_conditions, " AND ");
}

}  // namespace

QueryEngine::QueryEngine(sql::Database* database)
    : database_(database),
      query_cache_(kMaxCacheSize) {
  DCHECK(database_);
}

QueryEngine::~QueryEngine() = default;

QueryPlan QueryEngine::BuildQueryPlan(const std::vector<NostrFilter>& filters,
                                     int limit,
                                     bool reverse_order) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  QueryPlan plan;
  plan.primary_table = "events";
  
  // Optimize filter order for best index usage
  auto optimized_filters = OptimizeFilterOrder(filters);
  
  // Build WHERE clause
  plan.sql = "SELECT id, pubkey, created_at, kind, content, sig, "
             "received_at FROM " + plan.primary_table + " WHERE ";
  
  std::vector<std::string> where_conditions;
  
  // Always exclude deleted events
  where_conditions.push_back("deleted = 0");
  
  // Build conditions for each filter
  for (const auto& filter : optimized_filters) {
    std::vector<std::string> filter_conditions;
    
    // Event IDs
    if (!filter.ids.empty()) {
      std::vector<std::string> id_conditions;
      for (const auto& id : filter.ids) {
        plan.parameters.push_back(id);
        if (id.length() == 64) {
          id_conditions.push_back("id = ?");
        } else {
          id_conditions.push_back("id LIKE ? || '%'");
        }
      }
      filter_conditions.push_back(
          "(" + base::JoinString(id_conditions, " OR ") + ")");
      plan.indexes_used.push_back("idx_events_id");
    }
    
    // Authors
    if (!filter.authors.empty()) {
      std::vector<std::string> author_conditions;
      for (const auto& author : filter.authors) {
        plan.parameters.push_back(author);
        if (author.length() == 64) {
          author_conditions.push_back("pubkey = ?");
        } else {
          author_conditions.push_back("pubkey LIKE ? || '%'");
        }
      }
      filter_conditions.push_back(
          "(" + base::JoinString(author_conditions, " OR ") + ")");
      plan.indexes_used.push_back("idx_events_pubkey");
    }
    
    // Kinds
    if (!filter.kinds.empty()) {
      std::vector<std::string> kind_strings;
      for (int kind : filter.kinds) {
        plan.parameters.push_back(base::NumberToString(kind));
        kind_strings.push_back("?");
      }
      filter_conditions.push_back(
          "kind IN (" + base::JoinString(kind_strings, ",") + ")");
      plan.indexes_used.push_back("idx_events_kind");
    }
    
    // Time filters
    if (filter.since.has_value()) {
      plan.parameters.push_back(base::NumberToString(filter.since.value()));
      filter_conditions.push_back("created_at >= ?");
      plan.indexes_used.push_back("idx_events_created_at");
    }
    
    if (filter.until.has_value()) {
      plan.parameters.push_back(base::NumberToString(filter.until.value()));
      filter_conditions.push_back("created_at <= ?");
      plan.indexes_used.push_back("idx_events_created_at");
    }
    
    // Tag filters
    std::string tag_sql = BuildTagFilterSQL(filter.tags, plan.parameters);
    if (!tag_sql.empty()) {
      filter_conditions.push_back(tag_sql);
      plan.needs_tag_join = true;
      plan.indexes_used.push_back("idx_tags_name_value");
    }
    
    if (!filter_conditions.empty()) {
      where_conditions.push_back(
          "(" + base::JoinString(filter_conditions, " AND ") + ")");
    }
  }
  
  plan.sql += base::JoinString(where_conditions, " OR ");
  
  // Add ORDER BY
  plan.sql += BuildOrderByClause(reverse_order);
  
  // Add LIMIT
  plan.sql += " LIMIT ?";
  plan.parameters.push_back(base::NumberToString(limit));
  
  // Estimate query cost
  plan.estimated_rows = EstimateQueryCost(plan.sql);
  
  return plan;
}

std::vector<std::unique_ptr<NostrEvent>> QueryEngine::ExecuteQuery(
    const QueryPlan& plan) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::vector<std::unique_ptr<NostrEvent>> events;
  
  // Check cache
  std::string cache_key = GenerateCacheKey(plan);
  if (caching_enabled_ && GetCachedResult(cache_key, events)) {
    return events;
  }
  
  base::Time start_time = base::Time::Now();
  
  // Execute query
  sql::Statement statement(database_->GetCachedStatement(
      SQL_FROM_HERE, plan.sql.c_str()));
  
  // Bind parameters
  int param_index = 0;
  for (const auto& param : plan.parameters) {
    statement.BindString(param_index++, param);
  }
  
  // Fetch results
  while (statement.Step()) {
    auto event = ParseEventFromRow(statement);
    if (event) {
      events.push_back(std::move(event));
    }
  }
  
  base::TimeDelta execution_time = base::Time::Now() - start_time;
  
  // Log slow queries
  if (execution_time > slow_query_threshold_) {
    LogSlowQuery(plan, execution_time);
  }
  
  // Cache result
  if (caching_enabled_) {
    CacheResult(cache_key, events);
  }
  
  return events;
}

void QueryEngine::ExecuteQueryStreaming(const QueryPlan& plan,
                                       StreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Time start_time = base::Time::Now();
  
  // Execute query
  sql::Statement statement(database_->GetCachedStatement(
      SQL_FROM_HERE, plan.sql.c_str()));
  
  // Bind parameters
  int param_index = 0;
  for (const auto& param : plan.parameters) {
    statement.BindString(param_index++, param);
  }
  
  // Stream results
  while (statement.Step()) {
    auto event = ParseEventFromRow(statement);
    if (event) {
      callback.Run(std::move(event));
    }
  }
  
  base::TimeDelta execution_time = base::Time::Now() - start_time;
  
  // Log slow queries
  if (execution_time > slow_query_threshold_) {
    LogSlowQuery(plan, execution_time);
  }
}

std::unique_ptr<NostrEvent> QueryEngine::GetEventById(const std::string& event_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  sql::Statement statement(database_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, pubkey, created_at, kind, content, sig, received_at "
      "FROM events WHERE id = ? AND deleted = 0"));
  
  statement.BindString(0, event_id);
  
  if (statement.Step()) {
    return ParseEventFromRow(statement);
  }
  
  return nullptr;
}

std::vector<std::unique_ptr<NostrEvent>> QueryEngine::GetEventsByAuthor(
    const std::string& pubkey,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  std::vector<std::unique_ptr<NostrEvent>> events;
  
  sql::Statement statement(database_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, pubkey, created_at, kind, content, sig, received_at "
      "FROM events WHERE pubkey = ? AND deleted = 0 "
      "ORDER BY created_at DESC LIMIT ?"));
  
  statement.BindString(0, pubkey);
  statement.BindInt(1, limit);
  
  while (statement.Step()) {
    auto event = ParseEventFromRow(statement);
    if (event) {
      events.push_back(std::move(event));
    }
  }
  
  return events;
}

std::unique_ptr<NostrEvent> QueryEngine::GetReplaceableEvent(
    const std::string& pubkey,
    int kind,
    const std::string& d_tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  sql::Statement statement(database_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT event_id FROM replaceable_events "
      "WHERE pubkey = ? AND kind = ? AND d_tag = ?"));
  
  statement.BindString(0, pubkey);
  statement.BindInt(1, kind);
  statement.BindString(2, d_tag);
  
  if (statement.Step()) {
    std::string event_id = statement.ColumnString(0);
    return GetEventById(event_id);
  }
  
  return nullptr;
}

void QueryEngine::ClearCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_cache_.Clear();
}

base::Value::Dict QueryEngine::GetCacheStats() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  base::Value::Dict stats;
  stats.Set("size", static_cast<int>(query_cache_.size()));
  stats.Set("max_size", static_cast<int>(kMaxCacheSize));
  stats.Set("enabled", caching_enabled_);
  
  return stats;
}

std::string QueryEngine::BuildWhereClause(const std::vector<NostrFilter>& filters,
                                         std::vector<std::string>& parameters) {
  // Implemented in BuildQueryPlan
  return "";
}

std::string QueryEngine::BuildOrderByClause(bool reverse_order) const {
  return reverse_order ? " ORDER BY created_at ASC" : " ORDER BY created_at DESC";
}

std::vector<NostrFilter> QueryEngine::OptimizeFilterOrder(
    const std::vector<NostrFilter>& filters) const {
  // Sort filters by selectivity (most selective first)
  std::vector<NostrFilter> optimized = filters;
  
  std::sort(optimized.begin(), optimized.end(), 
            [](const NostrFilter& a, const NostrFilter& b) {
    // Prefer filters with IDs (most selective)
    if (!a.ids.empty() && b.ids.empty()) return true;
    if (a.ids.empty() && !b.ids.empty()) return false;
    
    // Then filters with specific authors
    if (!a.authors.empty() && b.authors.empty()) return true;
    if (a.authors.empty() && !b.authors.empty()) return false;
    
    // Then filters with kinds
    if (!a.kinds.empty() && b.kinds.empty()) return true;
    if (a.kinds.empty() && !b.kinds.empty()) return false;
    
    // Finally, filters with time constraints
    if (a.since.has_value() && !b.since.has_value()) return true;
    if (!a.since.has_value() && b.since.has_value()) return false;
    
    return false;
  });
  
  return optimized;
}

int QueryEngine::EstimateQueryCost(const std::string& where_clause) const {
  // Simple estimation based on query complexity
  // In a real implementation, this would use EXPLAIN QUERY PLAN
  int cost = 100;
  
  if (where_clause.find("id =") != std::string::npos) {
    cost = 1;  // Primary key lookup
  } else if (where_clause.find("pubkey =") != std::string::npos) {
    cost = 10;  // Indexed column
  } else if (where_clause.find("kind IN") != std::string::npos) {
    cost = 50;  // Indexed but less selective
  }
  
  return cost;
}

std::unique_ptr<NostrEvent> QueryEngine::ParseEventFromRow(
    sql::Statement& statement) const {
  auto event = std::make_unique<NostrEvent>();
  
  event->id = statement.ColumnString(0);
  event->pubkey = statement.ColumnString(1);
  event->created_at = statement.ColumnInt64(2);
  event->kind = statement.ColumnInt(3);
  event->content = statement.ColumnString(4);
  event->sig = statement.ColumnString(5);
  event->received_at = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(statement.ColumnInt64(6)));
  
  // Load tags
  sql::Statement tag_statement(database_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT tag_name, tag_value FROM tags WHERE event_id = ? "
      "ORDER BY tag_index"));
  
  tag_statement.BindString(0, event->id);
  
  std::string current_tag_name;
  base::Value::List current_tag;
  
  while (tag_statement.Step()) {
    std::string tag_name = tag_statement.ColumnString(0);
    std::string tag_value = tag_statement.ColumnString(1);
    
    if (tag_name != current_tag_name) {
      if (!current_tag.empty()) {
        event->tags.Append(std::move(current_tag));
        current_tag = base::Value::List();
      }
      current_tag_name = tag_name;
      current_tag.Append(tag_name);
    }
    
    current_tag.Append(tag_value);
  }
  
  if (!current_tag.empty()) {
    event->tags.Append(std::move(current_tag));
  }
  
  return event;
}

bool QueryEngine::GetCachedResult(const std::string& cache_key,
                                 std::vector<std::unique_ptr<NostrEvent>>& events) {
  auto it = query_cache_.Get(cache_key);
  if (it == query_cache_.end()) {
    return false;
  }
  
  const auto& cached = it->second;
  
  // Check if cache entry is still valid
  if (base::Time::Now() - cached->query_time > kCacheTTL) {
    query_cache_.Erase(it);
    return false;
  }
  
  // Clone cached events
  events.clear();
  for (const auto& event : cached->events) {
    auto clone = std::make_unique<NostrEvent>();
    *clone = *event;
    events.push_back(std::move(clone));
  }
  
  return true;
}

void QueryEngine::CacheResult(const std::string& cache_key,
                             const std::vector<std::unique_ptr<NostrEvent>>& events) {
  auto result = std::make_unique<QueryResult>();
  result->query_time = base::Time::Now();
  
  // Clone events for cache
  for (const auto& event : events) {
    auto clone = std::make_unique<NostrEvent>();
    *clone = *event;
    result->events.push_back(std::move(clone));
  }
  
  query_cache_.Put(cache_key, std::move(result));
}

std::string QueryEngine::GenerateCacheKey(const QueryPlan& plan) const {
  // Generate a unique key from the query plan
  std::stringstream key;
  key << plan.sql;
  for (const auto& param : plan.parameters) {
    key << "|" << param;
  }
  return key.str();
}

void QueryEngine::LogSlowQuery(const QueryPlan& plan,
                              base::TimeDelta execution_time) {
  LOG(WARNING) << "Slow query detected: " << execution_time.InMilliseconds() 
               << "ms for query: " << plan.sql;
  VLOG(1) << "Query parameters: " << base::JoinString(plan.parameters, ", ");
  VLOG(1) << "Estimated rows: " << plan.estimated_rows;
  VLOG(1) << "Indexes used: " << base::JoinString(plan.indexes_used, ", ");
}

}  // namespace local_relay
}  // namespace nostr