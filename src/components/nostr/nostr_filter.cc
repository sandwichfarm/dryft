// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nostr/nostr_filter.h"

#include <sstream>

#include "base/strings/string_util.h"

namespace nostr {

NostrFilter::NostrFilter() = default;

NostrFilter::~NostrFilter() = default;

base::Value NostrFilter::ToValue() const {
  base::Value::Dict filter;
  
  if (!ids.empty()) {
    base::Value::List ids_list;
    for (const auto& id : ids) {
      ids_list.Append(id);
    }
    filter.Set("ids", std::move(ids_list));
  }
  
  if (!authors.empty()) {
    base::Value::List authors_list;
    for (const auto& author : authors) {
      authors_list.Append(author);
    }
    filter.Set("authors", std::move(authors_list));
  }
  
  if (!kinds.empty()) {
    base::Value::List kinds_list;
    for (int kind : kinds) {
      kinds_list.Append(kind);
    }
    filter.Set("kinds", std::move(kinds_list));
  }
  
  if (since.has_value()) {
    filter.Set("since", static_cast<double>(since.value()));
  }
  
  if (until.has_value()) {
    filter.Set("until", static_cast<double>(until.value()));
  }
  
  if (limit.has_value()) {
    filter.Set("limit", limit.value());
  }
  
  // Add tag filters
  for (const auto& [tag_name, tag_values] : tags) {
    base::Value::List values_list;
    for (const auto& value : tag_values) {
      values_list.Append(value);
    }
    filter.Set("#" + tag_name, std::move(values_list));
  }
  
  return base::Value(std::move(filter));
}

std::unique_ptr<NostrFilter> NostrFilter::FromValue(const base::Value::Dict& value) {
  auto filter = std::make_unique<NostrFilter>();
  
  // Parse IDs
  const auto* ids_list = value.FindList("ids");
  if (ids_list) {
    for (const auto& id : *ids_list) {
      const auto* id_str = id.GetIfString();
      if (id_str) {
        filter->ids.push_back(*id_str);
      }
    }
  }
  
  // Parse authors
  const auto* authors_list = value.FindList("authors");
  if (authors_list) {
    for (const auto& author : *authors_list) {
      const auto* author_str = author.GetIfString();
      if (author_str) {
        filter->authors.push_back(*author_str);
      }
    }
  }
  
  // Parse kinds
  const auto* kinds_list = value.FindList("kinds");
  if (kinds_list) {
    for (const auto& kind : *kinds_list) {
      if (kind.is_int()) {
        filter->kinds.push_back(kind.GetInt());
      }
    }
  }
  
  // Parse time filters
  const auto since_val = value.FindDouble("since");
  if (since_val) {
    filter->since = static_cast<int64_t>(*since_val);
  }
  
  const auto until_val = value.FindDouble("until");
  if (until_val) {
    filter->until = static_cast<int64_t>(*until_val);
  }
  
  // Parse limit
  const auto limit_val = value.FindInt("limit");
  if (limit_val) {
    filter->limit = *limit_val;
  }
  
  // Parse tag filters
  for (const auto& [key, val] : value) {
    if (key.length() == 2 && key[0] == '#') {
      const auto* tag_values = val.GetIfList();
      if (tag_values) {
        std::vector<std::string> values;
        for (const auto& v : *tag_values) {
          const auto* v_str = v.GetIfString();
          if (v_str) {
            values.push_back(*v_str);
          }
        }
        if (!values.empty()) {
          filter->tags[key.substr(1)] = values;
        }
      }
    }
  }
  
  return filter;
}

std::string NostrFilter::ToSQLWhereClause() const {
  std::vector<std::string> conditions;
  
  // Event IDs (with prefix matching)
  if (!ids.empty()) {
    std::vector<std::string> id_conditions;
    for (const auto& id : ids) {
      if (id.length() == 64) {
        id_conditions.push_back("id = '" + id + "'");
      } else {
        id_conditions.push_back("id LIKE '" + id + "%'");
      }
    }
    conditions.push_back("(" + base::JoinString(id_conditions, " OR ") + ")");
  }
  
  // Authors (with prefix matching)
  if (!authors.empty()) {
    std::vector<std::string> author_conditions;
    for (const auto& author : authors) {
      if (author.length() == 64) {
        author_conditions.push_back("pubkey = '" + author + "'");
      } else {
        author_conditions.push_back("pubkey LIKE '" + author + "%'");
      }
    }
    conditions.push_back("(" + base::JoinString(author_conditions, " OR ") + ")");
  }
  
  // Kinds
  if (!kinds.empty()) {
    std::vector<std::string> kind_strs;
    for (int kind : kinds) {
      kind_strs.push_back(std::to_string(kind));
    }
    conditions.push_back("kind IN (" + base::JoinString(kind_strs, ",") + ")");
  }
  
  // Time filters
  if (since.has_value()) {
    conditions.push_back("created_at >= " + std::to_string(since.value()));
  }
  
  if (until.has_value()) {
    conditions.push_back("created_at <= " + std::to_string(until.value()));
  }
  
  // Always exclude deleted events
  conditions.push_back("deleted = 0");
  
  if (conditions.empty()) {
    return "1=1";  // Match everything
  }
  
  return base::JoinString(conditions, " AND ");
}

bool NostrFilter::IsEmpty() const {
  return ids.empty() && 
         authors.empty() && 
         kinds.empty() && 
         !since.has_value() && 
         !until.has_value() && 
         tags.empty();
}

}  // namespace nostr