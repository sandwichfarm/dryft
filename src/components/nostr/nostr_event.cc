// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nostr/nostr_event.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"

namespace nostr {

NostrEvent::NostrEvent() : created_at(0), kind(0) {}

NostrEvent::~NostrEvent() = default;

base::Value NostrEvent::ToValue() const {
  base::Value::Dict event;
  event.Set("id", id);
  event.Set("pubkey", pubkey);
  event.Set("created_at", static_cast<double>(created_at));
  event.Set("kind", kind);
  event.Set("tags", tags.Clone());
  event.Set("content", content);
  event.Set("sig", sig);
  return base::Value(std::move(event));
}

std::unique_ptr<NostrEvent> NostrEvent::FromValue(const base::Value::Dict& value) {
  auto event = std::make_unique<NostrEvent>();
  
  // Parse required fields
  const auto* id_str = value.FindString("id");
  if (!id_str) return nullptr;
  event->id = *id_str;
  
  const auto* pubkey_str = value.FindString("pubkey");
  if (!pubkey_str) return nullptr;
  event->pubkey = *pubkey_str;
  
  const auto created_at_val = value.FindDouble("created_at");
  if (!created_at_val) return nullptr;
  event->created_at = static_cast<int64_t>(*created_at_val);
  
  const auto kind_val = value.FindInt("kind");
  if (!kind_val) return nullptr;
  event->kind = *kind_val;
  
  const auto* tags_list = value.FindList("tags");
  if (!tags_list) return nullptr;
  event->tags = tags_list->Clone();
  
  const auto* content_str = value.FindString("content");
  if (!content_str) return nullptr;
  event->content = *content_str;
  
  const auto* sig_str = value.FindString("sig");
  if (!sig_str) return nullptr;
  event->sig = *sig_str;
  
  // Set received time to now
  event->received_at = base::Time::Now();
  
  return event;
}

bool NostrEvent::IsValid() const {
  return !id.empty() && 
         id.length() == 64 &&
         !pubkey.empty() && 
         pubkey.length() == 64 &&
         created_at > 0 &&
         !sig.empty() && 
         sig.length() == 128;
}

std::string NostrEvent::GetDTagValue() const {
  for (const auto& tag : tags) {
    const auto* tag_list = tag.GetIfList();
    if (!tag_list || tag_list->size() < 2) {
      continue;
    }
    
    const auto* tag_name = (*tag_list)[0].GetIfString();
    if (tag_name && *tag_name == "d") {
      const auto* tag_value = (*tag_list)[1].GetIfString();
      if (tag_value) {
        return *tag_value;
      }
    }
  }
  return std::string();
}

bool NostrEvent::HasTag(const std::string& tag_name) const {
  for (const auto& tag : tags) {
    const auto* tag_list = tag.GetIfList();
    if (!tag_list || tag_list->empty()) {
      continue;
    }
    
    const auto* name = (*tag_list)[0].GetIfString();
    if (name && *name == tag_name) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> NostrEvent::GetTagValues(const std::string& tag_name) const {
  std::vector<std::string> values;
  
  for (const auto& tag : tags) {
    const auto* tag_list = tag.GetIfList();
    if (!tag_list || tag_list->size() < 2) {
      continue;
    }
    
    const auto* name = (*tag_list)[0].GetIfString();
    if (name && *name == tag_name) {
      for (size_t i = 1; i < tag_list->size(); ++i) {
        const auto* value = (*tag_list)[i].GetIfString();
        if (value) {
          values.push_back(*value);
        }
      }
    }
  }
  
  return values;
}

}  // namespace nostr