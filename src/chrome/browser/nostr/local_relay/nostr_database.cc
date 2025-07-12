// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/nostr_database.h"

#include <algorithm>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/nostr/local_relay/nostr_database_schema.h"
#include "crypto/sha2.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace nostr {
namespace local_relay {

namespace {

// Database task runner traits
constexpr base::TaskTraits kDatabaseTaskTraits = {
    base::MayBlock(),
    base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN
};

// Replaceable event kinds as per NIP-16
bool IsReplaceableKindInternal(int kind) {
  return kind == 0 ||      // Metadata
         kind == 3 ||      // Contact list
         (kind >= 10000 && kind < 20000);  // Replaceable events
}

// Parameterized replaceable event kinds
bool IsParameterizedReplaceableKind(int kind) {
  return kind >= 30000 && kind < 40000;
}

// Extract 'd' tag value from tags
std::string ExtractDTagValue(const base::Value::List& tags) {
  for (const auto& tag_value : tags) {
    if (!tag_value.is_list()) continue;
    
    const auto& tag = tag_value.GetList();
    if (tag.size() < 2) continue;
    
    const auto* tag_name = tag[0].GetIfString();
    if (!tag_name || *tag_name != "d") continue;
    
    const auto* tag_value_str = tag[1].GetIfString();
    return tag_value_str ? *tag_value_str : "";
  }
  return "";
}

// Convert tags to JSON string for storage
std::string TagsToJson(const base::Value::List& tags) {
  std::string json;
  base::JSONWriter::Write(tags, &json);
  return json;
}

// Parse tags from JSON string
base::Value::List TagsFromJson(const std::string& json) {
  auto value = base::JSONReader::Read(json);
  if (value && value->is_list()) {
    return std::move(value->GetList());
  }
  return base::Value::List();
}

}  // namespace

// NostrEvent implementation

base::Value::Dict NostrEvent::ToDict() const {
  base::Value::Dict dict;
  dict.Set("id", id);
  dict.Set("pubkey", pubkey);
  dict.Set("created_at", static_cast<double>(created_at));
  dict.Set("kind", kind);
  dict.Set("tags", tags.Clone());
  dict.Set("content", content);
  dict.Set("sig", sig);
  return dict;
}

std::unique_ptr<NostrEvent> NostrEvent::FromDict(const base::Value::Dict& dict) {
  auto event = std::make_unique<NostrEvent>();
  
  const auto* id_str = dict.FindString("id");
  if (!id_str || id_str->length() != 64) return nullptr;
  event->id = *id_str;
  
  const auto* pubkey_str = dict.FindString("pubkey");
  if (!pubkey_str || pubkey_str->length() != 64) return nullptr;
  event->pubkey = *pubkey_str;
  
  auto created_at_val = dict.FindDouble("created_at");
  if (!created_at_val) return nullptr;
  event->created_at = static_cast<int64_t>(*created_at_val);
  
  auto kind_val = dict.FindInt("kind");
  if (!kind_val) return nullptr;
  event->kind = *kind_val;
  
  const auto* tags_list = dict.FindList("tags");
  if (!tags_list) return nullptr;
  event->tags = tags_list->Clone();
  
  const auto* content_str = dict.FindString("content");
  if (!content_str) return nullptr;
  event->content = *content_str;
  
  const auto* sig_str = dict.FindString("sig");
  if (!sig_str || sig_str->length() != 128) return nullptr;
  event->sig = *sig_str;
  
  event->received_at = base::Time::Now();
  
  return event;
}

bool NostrEvent::IsValid() const {
  // Basic validation - proper validation would check signature
  return id.length() == 64 &&
         pubkey.length() == 64 &&
         created_at > 0 &&
         kind >= 0 &&
         sig.length() == 128;
}

std::string NostrEvent::ComputeId() const {
  // Create canonical serialization for ID computation
  base::Value::List signing_array;
  signing_array.Append(0);  // Always 0 for events
  signing_array.Append(pubkey);
  signing_array.Append(static_cast<double>(created_at));
  signing_array.Append(kind);
  signing_array.Append(tags.Clone());
  signing_array.Append(content);
  
  std::string json;
  base::JSONWriter::Write(signing_array, &json);
  
  // Compute SHA256 hash
  std::string hash = crypto::SHA256HashString(json);
  return base::HexEncode(hash);
}

// NostrFilter implementation

std::string NostrFilter::ToSqlWhereClause() const {
  std::vector<std::string> conditions;
  
  // ID filter
  if (!ids.empty()) {
    std::vector<std::string> id_conditions;
    for (const auto& id : ids) {
      id_conditions.push_back(base::StringPrintf("id = '%s'", id.c_str()));
    }
    conditions.push_back("(" + base::JoinString(id_conditions, " OR ") + ")");
  }
  
  // Author filter
  if (!authors.empty()) {
    std::vector<std::string> author_conditions;
    for (const auto& author : authors) {
      author_conditions.push_back(base::StringPrintf("pubkey = '%s'", author.c_str()));
    }
    conditions.push_back("(" + base::JoinString(author_conditions, " OR ") + ")");
  }
  
  // Kind filter
  if (!kinds.empty()) {
    std::vector<std::string> kind_conditions;
    for (int kind : kinds) {
      kind_conditions.push_back(base::StringPrintf("kind = %d", kind));
    }
    conditions.push_back("(" + base::JoinString(kind_conditions, " OR ") + ")");
  }
  
  // Time filters
  if (since.has_value()) {
    conditions.push_back(base::StringPrintf("created_at >= %lld", *since));
  }
  if (until.has_value()) {
    conditions.push_back(base::StringPrintf("created_at <= %lld", *until));
  }
  
  // Always exclude deleted events
  conditions.push_back("deleted = 0");
  
  if (conditions.empty()) {
    return "1=1";  // Match all
  }
  
  return base::JoinString(conditions, " AND ");
}

std::unique_ptr<NostrFilter> NostrFilter::FromDict(const base::Value::Dict& dict) {
  auto filter = std::make_unique<NostrFilter>();
  
  // Parse IDs
  if (const auto* ids_list = dict.FindList("ids")) {
    for (const auto& id_val : *ids_list) {
      if (const auto* id_str = id_val.GetIfString()) {
        filter->ids.push_back(*id_str);
      }
    }
  }
  
  // Parse authors
  if (const auto* authors_list = dict.FindList("authors")) {
    for (const auto& author_val : *authors_list) {
      if (const auto* author_str = author_val.GetIfString()) {
        filter->authors.push_back(*author_str);
      }
    }
  }
  
  // Parse kinds
  if (const auto* kinds_list = dict.FindList("kinds")) {
    for (const auto& kind_val : *kinds_list) {
      if (kind_val.is_int()) {
        filter->kinds.push_back(kind_val.GetInt());
      }
    }
  }
  
  // Parse time filters
  if (auto since_val = dict.FindInt("since")) {
    filter->since = *since_val;
  }
  if (auto until_val = dict.FindInt("until")) {
    filter->until = *until_val;
  }
  
  // Parse limit
  if (auto limit_val = dict.FindInt("limit")) {
    filter->limit = *limit_val;
  }
  
  // Parse tag filters
  for (const auto& [key, value] : dict) {
    if (key.length() == 2 && key[0] == '#') {
      char tag_name = key[1];
      if (value.is_list()) {
        std::vector<std::string> tag_values;
        for (const auto& tag_val : value.GetList()) {
          if (const auto* tag_str = tag_val.GetIfString()) {
            tag_values.push_back(*tag_str);
          }
        }
        filter->tags[std::string(1, tag_name)] = tag_values;
      }
    }
  }
  
  return filter;
}

// NostrDatabase implementation

NostrDatabase::NostrDatabase(const base::FilePath& db_path,
                           const Config& config)
    : db_path_(db_path),
      config_(config),
      db_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          kDatabaseTaskTraits)),
      callback_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NostrDatabase::~NostrDatabase() {
  // Ensure database is closed on the database thread
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NostrDatabase::Close, weak_factory_.GetWeakPtr()));
}

void NostrDatabase::Initialize(InitCallback callback) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NostrDatabase::InitializeInternal,
                     weak_factory_.GetWeakPtr()),
      std::move(callback));
}

void NostrDatabase::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (db_ && db_->is_open()) {
    db_->Close();
  }
  db_.reset();
  meta_table_.reset();
}

bool NostrDatabase::InitializeInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Ensure directory exists
  base::FilePath dir = db_path_.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create database directory: " << dir;
    return false;
  }
  
  // Open database
  db_ = std::make_unique<sql::Database>();
  db_->set_histogram_tag("NostrLocalRelay");
  
  if (!db_->Open(db_path_)) {
    LOG(ERROR) << "Failed to open database: " << db_path_;
    db_.reset();
    return false;
  }
  
  // Apply configuration
  if (!ApplyConfiguration()) {
    LOG(ERROR) << "Failed to apply database configuration";
    return false;
  }
  
  // Initialize schema
  if (!CreateTables() || !CreateIndexes() || !CreateMetadata()) {
    LOG(ERROR) << "Failed to initialize database schema";
    return false;
  }
  
  // Check for migrations
  if (!MigrateToLatestSchema()) {
    LOG(ERROR) << "Failed to migrate database schema";
    return false;
  }
  
  return true;
}

bool NostrDatabase::CreateTables() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }
  
  // Create all tables
  const char* create_statements[] = {
      NostrDatabaseSchema::kCreateEventsTable,
      NostrDatabaseSchema::kCreateTagsTable,
      NostrDatabaseSchema::kCreateDeletionsTable,
      NostrDatabaseSchema::kCreateReplaceableEventsTable,
      NostrDatabaseSchema::kCreateSubscriptionsTable,
      NostrDatabaseSchema::kCreateMetadataTable
  };
  
  for (const char* statement : create_statements) {
    if (!db_->Execute(statement)) {
      LOG(ERROR) << "Failed to create table: " << statement;
      return false;
    }
  }
  
  return transaction.Commit();
}

bool NostrDatabase::CreateIndexes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  const char* index_statements[] = {
      NostrDatabaseSchema::kCreateIndexEventsPubkey,
      NostrDatabaseSchema::kCreateIndexEventsKind,
      NostrDatabaseSchema::kCreateIndexEventsCreatedAt,
      NostrDatabaseSchema::kCreateIndexEventsKindCreatedAt,
      NostrDatabaseSchema::kCreateIndexEventsPubkeyKind,
      NostrDatabaseSchema::kCreateIndexEventsPubkeyCreatedAt,
      NostrDatabaseSchema::kCreateIndexTagsEventId,
      NostrDatabaseSchema::kCreateIndexTagsNameValue,
      NostrDatabaseSchema::kCreateIndexDeletionsDeletedEventId,
      NostrDatabaseSchema::kCreateIndexReplaceableCurrentEventId
  };
  
  for (const char* statement : index_statements) {
    if (!db_->Execute(statement)) {
      LOG(ERROR) << "Failed to create index: " << statement;
      return false;
    }
  }
  
  return true;
}

bool NostrDatabase::CreateMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Initialize meta table
  meta_table_ = std::make_unique<sql::MetaTable>();
  if (!meta_table_->Init(db_.get(), NostrDatabaseSchema::kCurrentVersion,
                         NostrDatabaseSchema::kCurrentVersion)) {
    LOG(ERROR) << "Failed to initialize meta table";
    return false;
  }
  
  // Insert initial metadata
  if (!db_->Execute(NostrDatabaseSchema::kInsertSchemaVersion) ||
      !db_->Execute(NostrDatabaseSchema::kInsertCreatedAt)) {
    LOG(ERROR) << "Failed to insert initial metadata";
    return false;
  }
  
  return true;
}

bool NostrDatabase::ApplyConfiguration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Set page size (must be done before any tables are created)
  std::string page_size_sql = 
      base::StringPrintf("PRAGMA page_size = %d", config_.page_size);
  db_->Execute(page_size_sql.c_str());
  
  // Set cache size
  if (config_.cache_size != -1) {
    std::string cache_size_sql = 
        base::StringPrintf("PRAGMA cache_size = %d", config_.cache_size);
    db_->Execute(cache_size_sql.c_str());
  }
  
  // Enable auto-vacuum if requested
  if (config_.auto_vacuum) {
    db_->Execute("PRAGMA auto_vacuum = INCREMENTAL");
  }
  
  // Enable foreign keys
  db_->Execute("PRAGMA foreign_keys = ON");
  
  // Set journal mode to WAL for better concurrency
  db_->Execute("PRAGMA journal_mode = WAL");
  
  // Set synchronous mode to NORMAL for better performance
  db_->Execute("PRAGMA synchronous = NORMAL");
  
  return true;
}

void NostrDatabase::StoreEvent(std::unique_ptr<NostrEvent> event,
                              StatusCallback callback) {
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NostrDatabase::StoreEventInternal,
                     weak_factory_.GetWeakPtr(), *event),
      std::move(callback));
}

bool NostrDatabase::StoreEventInternal(const NostrEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Check if event already exists
  sql::Statement check_stmt(db_->GetUniqueStatement(
      NostrDatabaseSchema::kCheckEventExists));
  check_stmt.BindString(0, event.id);
  if (check_stmt.Step()) {
    return false;  // Event already exists
  }
  
  // Check if this is a replaceable event
  if (IsReplaceableKindInternal(event.kind) || 
      IsParameterizedReplaceableKind(event.kind)) {
    return StoreReplaceableEventInternal(event);
  }
  
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }
  
  // Insert event
  sql::Statement insert_stmt(db_->GetUniqueStatement(
      "INSERT INTO events (id, pubkey, created_at, kind, content, sig, "
      "received_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
  
  insert_stmt.BindString(0, event.id);
  insert_stmt.BindString(1, event.pubkey);
  insert_stmt.BindInt64(2, event.created_at);
  insert_stmt.BindInt(3, event.kind);
  insert_stmt.BindString(4, event.content);
  insert_stmt.BindString(5, event.sig);
  insert_stmt.BindInt64(6, event.received_at.ToInternalValue());
  insert_stmt.BindInt64(7, event.received_at.ToInternalValue());
  
  if (!insert_stmt.Run()) {
    LOG(ERROR) << "Failed to insert event";
    return false;
  }
  
  // Store tags
  if (!StoreTags(event.id, event.tags)) {
    LOG(ERROR) << "Failed to store tags";
    return false;
  }
  
  // Enforce storage limits
  EnforceStorageLimits();
  
  return transaction.Commit();
}

bool NostrDatabase::StoreTags(const std::string& event_id,
                             const base::Value::List& tags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  sql::Statement stmt(db_->GetUniqueStatement(
      "INSERT INTO tags (event_id, tag_name, tag_value, tag_full) "
      "VALUES (?, ?, ?, ?)"));
  
  for (const auto& tag_value : tags) {
    if (!tag_value.is_list()) continue;
    
    const auto& tag = tag_value.GetList();
    if (tag.size() < 1) continue;
    
    const auto* tag_name = tag[0].GetIfString();
    if (!tag_name || tag_name->empty()) continue;
    
    std::string tag_value_str;
    if (tag.size() > 1 && tag[1].is_string()) {
      tag_value_str = tag[1].GetString();
    }
    
    stmt.Reset(true);
    stmt.BindString(0, event_id);
    stmt.BindString(1, *tag_name);
    stmt.BindString(2, tag_value_str);
    stmt.BindString(3, TagsToJson(base::Value::List({tag_value.Clone()})));
    
    if (!stmt.Run()) {
      return false;
    }
  }
  
  return true;
}

void NostrDatabase::EnforceStorageLimits() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  // Check event count limit
  if (config_.max_event_count > 0) {
    int64_t count = CountEventsInternal();
    if (count > config_.max_event_count) {
      // Delete oldest events
      int64_t to_delete = count - config_.max_event_count;
      sql::Statement delete_stmt(db_->GetUniqueStatement(
          "DELETE FROM events WHERE id IN "
          "(SELECT id FROM events ORDER BY created_at ASC LIMIT ?)"));
      delete_stmt.BindInt64(0, to_delete);
      delete_stmt.Run();
    }
  }
  
  // Check database size limit
  if (config_.max_size_bytes > 0) {
    int64_t size = GetDatabaseSizeInternal();
    if (size > config_.max_size_bytes) {
      // Delete oldest events until size is under limit
      // This is a simplified approach - a production system would be smarter
      sql::Statement delete_stmt(db_->GetUniqueStatement(
          "DELETE FROM events WHERE id IN "
          "(SELECT id FROM events ORDER BY created_at ASC LIMIT 1000)"));
      delete_stmt.Run();
    }
  }
}

bool NostrDatabase::MigrateToLatestSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  int current_version = meta_table_->GetVersionNumber();
  if (current_version < NostrDatabaseSchema::kCurrentVersion) {
    // Implement migration logic here when schema changes
    LOG(INFO) << "Migrating database from version " << current_version
              << " to " << NostrDatabaseSchema::kCurrentVersion;
    
    // For now, we only have version 1
    meta_table_->SetVersionNumber(NostrDatabaseSchema::kCurrentVersion);
  }
  
  return true;
}

template <typename Callback, typename... Args>
void NostrDatabase::PostCallback(Callback callback, Args&&... args) {
  callback_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::forward<Args>(args)...));
}

}  // namespace local_relay
}  // namespace nostr