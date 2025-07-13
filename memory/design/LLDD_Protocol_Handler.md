# Low Level Design Document: Protocol Handler
## Tungsten Browser - nostr:// URL Scheme Implementation

### 1. Component Overview

The Protocol Handler component enables Tungsten to natively handle nostr:// URLs, providing seamless navigation to Nostr content. It parses various Nostr identifier formats, resolves content from relays, and renders appropriate views.

### 2. Detailed Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  URL Input Layer                         │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Address Bar │  │ Link Click   │  │ JavaScript   │  │
│  │             │  │              │  │ Navigation   │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│              Protocol Registration Layer                 │
│  ┌─────────────────────────────────────────────────┐   │
│  │           Chrome Protocol Handler                 │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Scheme       │  │ URL Interceptor   │       │   │
│  │  │ Registry     │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                 Nostr URL Parser                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │            Identifier Parser                      │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │   │
│  │  │  Bech32  │  │  TLV     │  │  Validation  │  │   │
│  │  │  Decoder │  │  Parser  │  │              │  │   │
│  │  └──────────┘  └──────────┘  └──────────────┘  │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                Content Resolution Layer                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │            Content Resolver                       │   │
│  │  ┌──────────────┐  ┌───────────────────┐       │   │
│  │  │ Relay Query  │  │ Cache Lookup      │       │   │
│  │  │ Engine       │  │                   │       │   │
│  │  └──────────────┘  └───────────────────┘       │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
┌─────────────────────────────────────────────────────────┐
│                  Rendering Layer                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │            Content Type Handlers                  │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │   │
│  │  │ Profile  │  │  Note    │  │    Nsite     │  │   │
│  │  │ Viewer   │  │  Viewer  │  │   Renderer   │  │   │
│  │  └──────────┘  └──────────┘  └──────────────┘  │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3. URL Format Specifications

```cpp
// Supported Nostr URL formats
enum class NostrIdentifierType {
  NPUB,      // Public key
  NSEC,      // Private key (handle with care)
  NOTE,      // Event ID
  NEVENT,    // Event with relay hints
  NADDR,     // Parameterized replaceable event
  NPROFILE,  // Profile with relay hints
  NSITE,     // Static website reference
  NRELAY,    // Relay recommendation
};

struct NostrIdentifier {
  NostrIdentifierType type;
  std::string raw_identifier;
  std::vector<uint8_t> decoded_data;
  
  // TLV fields for extended identifiers
  struct TLVField {
    uint8_t type;
    std::vector<uint8_t> value;
  };
  std::vector<TLVField> tlv_fields;
};
```

### 4. Class Definitions

```cpp
// browser/nostr_protocol_handler.cc
class NostrProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  NostrProtocolHandler();
  
  // URLRequestJobFactory::ProtocolHandler implementation
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;
  
 private:
  std::unique_ptr<NostrURLParser> parser_;
  std::unique_ptr<NostrContentResolver> resolver_;
};

// browser/nostr_url_parser.cc
class NostrURLParser {
 public:
  struct ParseResult {
    bool success;
    NostrIdentifier identifier;
    std::string error_message;
    
    // Extracted data based on type
    std::string public_key;      // For npub, nprofile
    std::string event_id;        // For note, nevent
    std::string d_tag;           // For naddr
    int kind;                    // For naddr
    std::vector<std::string> relay_hints;
  };
  
  ParseResult Parse(const GURL& url);
  
 private:
  bool DecodeBech32(const std::string& bech32_string,
                   std::string* hrp,
                   std::vector<uint8_t>* data);
  bool ParseTLV(const std::vector<uint8_t>& data,
               std::vector<NostrIdentifier::TLVField>* fields);
};

// browser/nostr_content_resolver.cc
class NostrContentResolver {
 public:
  using ResolveCallback = base::OnceCallback<void(
      bool success,
      std::unique_ptr<NostrContent> content)>;
  
  void ResolveContent(const NostrIdentifier& identifier,
                     ResolveCallback callback);
  
 private:
  void QueryRelays(const std::vector<std::string>& relay_urls,
                  const NostrFilter& filter,
                  ResolveCallback callback);
  void CheckCache(const std::string& identifier_key,
                 ResolveCallback callback);
  
  std::unique_ptr<RelayConnectionManager> relay_manager_;
  std::unique_ptr<NostrCache> cache_;
};
```

### 5. Bech32 Decoding Implementation

```cpp
class Bech32Decoder {
 public:
  static constexpr char kCharset[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
  
  struct DecodeResult {
    bool success;
    std::string hrp;
    std::vector<uint8_t> data;
    std::string error;
  };
  
  static DecodeResult Decode(const std::string& bech32) {
    DecodeResult result;
    
    // 1. Find separator
    size_t sep_pos = bech32.rfind('1');
    if (sep_pos == std::string::npos || sep_pos == 0) {
      result.error = "No separator found";
      return result;
    }
    
    // 2. Extract HRP
    result.hrp = bech32.substr(0, sep_pos);
    
    // 3. Decode data part
    std::vector<uint8_t> values;
    for (size_t i = sep_pos + 1; i < bech32.length(); ++i) {
      const char* pos = std::strchr(kCharset, bech32[i]);
      if (!pos) {
        result.error = "Invalid character";
        return result;
      }
      values.push_back(pos - kCharset);
    }
    
    // 4. Verify checksum
    if (!VerifyChecksum(result.hrp, values)) {
      result.error = "Invalid checksum";
      return result;
    }
    
    // 5. Convert from 5-bit to 8-bit
    values.resize(values.size() - 6); // Remove checksum
    result.data = ConvertBits(values, 5, 8, false);
    result.success = true;
    
    return result;
  }
  
 private:
  static bool VerifyChecksum(const std::string& hrp,
                            const std::vector<uint8_t>& values) {
    std::vector<uint8_t> enc;
    enc.reserve(hrp.size() + 1 + values.size());
    
    // Expand HRP
    for (char c : hrp) {
      enc.push_back(c >> 5);
    }
    enc.push_back(0);
    for (char c : hrp) {
      enc.push_back(c & 31);
    }
    
    // Add values
    enc.insert(enc.end(), values.begin(), values.end());
    
    return Polymod(enc) == 1;
  }
  
  static std::vector<uint8_t> ConvertBits(
      const std::vector<uint8_t>& data,
      int from_bits, int to_bits, bool pad) {
    int acc = 0;
    int bits = 0;
    std::vector<uint8_t> ret;
    int maxv = (1 << to_bits) - 1;
    int max_acc = (1 << (from_bits + to_bits - 1)) - 1;
    
    for (uint8_t value : data) {
      acc = ((acc << from_bits) | value) & max_acc;
      bits += from_bits;
      while (bits >= to_bits) {
        bits -= to_bits;
        ret.push_back((acc >> bits) & maxv);
      }
    }
    
    if (pad && bits) {
      ret.push_back((acc << (to_bits - bits)) & maxv);
    }
    
    return ret;
  }
};
```

### 6. TLV Parsing Implementation

```cpp
class TLVParser {
 public:
  enum TLVType {
    TLV_SPECIAL = 0,
    TLV_RELAY = 1,
    TLV_AUTHOR = 2,
    TLV_KIND = 3,
  };
  
  static std::vector<NostrIdentifier::TLVField> Parse(
      const std::vector<uint8_t>& data) {
    std::vector<NostrIdentifier::TLVField> fields;
    size_t pos = 0;
    
    while (pos < data.size()) {
      if (pos + 2 > data.size()) break;
      
      NostrIdentifier::TLVField field;
      field.type = data[pos++];
      uint8_t length = data[pos++];
      
      if (pos + length > data.size()) break;
      
      field.value.assign(data.begin() + pos, 
                        data.begin() + pos + length);
      pos += length;
      
      fields.push_back(field);
    }
    
    return fields;
  }
  
  static std::vector<std::string> ExtractRelays(
      const std::vector<NostrIdentifier::TLVField>& fields) {
    std::vector<std::string> relays;
    
    for (const auto& field : fields) {
      if (field.type == TLV_RELAY) {
        relays.push_back(std::string(field.value.begin(), 
                                    field.value.end()));
      }
    }
    
    return relays;
  }
  
  static std::string ExtractAuthor(
      const std::vector<NostrIdentifier::TLVField>& fields) {
    for (const auto& field : fields) {
      if (field.type == TLV_AUTHOR && field.value.size() == 32) {
        return HexEncode(field.value);
      }
    }
    return "";
  }
  
  static int ExtractKind(
      const std::vector<NostrIdentifier::TLVField>& fields) {
    for (const auto& field : fields) {
      if (field.type == TLV_KIND && field.value.size() == 4) {
        return (field.value[0] << 24) | (field.value[1] << 16) |
               (field.value[2] << 8) | field.value[3];
      }
    }
    return -1;
  }
};
```

### 7. Content Resolution Implementation

```cpp
class NostrContentResolver {
 public:
  void ResolveContent(const NostrIdentifier& identifier,
                     ResolveCallback callback) {
    // 1. Check cache first
    std::string cache_key = GetCacheKey(identifier);
    auto cached = cache_->Get(cache_key);
    if (cached) {
      std::move(callback).Run(true, std::move(cached));
      return;
    }
    
    // 2. Build filter based on identifier type
    NostrFilter filter = BuildFilter(identifier);
    
    // 3. Determine relays to query
    std::vector<std::string> relays = GetRelaysForQuery(identifier);
    
    // 4. Query relays
    QueryRelays(relays, filter, std::move(callback));
  }
  
 private:
  NostrFilter BuildFilter(const NostrIdentifier& identifier) {
    NostrFilter filter;
    
    switch (identifier.type) {
      case NostrIdentifierType::NOTE:
      case NostrIdentifierType::NEVENT:
        filter.ids.push_back(HexEncode(identifier.decoded_data));
        break;
        
      case NostrIdentifierType::NPUB:
      case NostrIdentifierType::NPROFILE:
        filter.authors.push_back(HexEncode(identifier.decoded_data));
        filter.kinds = {0, 3}; // Metadata and contacts
        break;
        
      case NostrIdentifierType::NADDR:
        filter.authors.push_back(TLVParser::ExtractAuthor(
            identifier.tlv_fields));
        filter.kinds.push_back(TLVParser::ExtractKind(
            identifier.tlv_fields));
        filter.tags["d"] = {TLVParser::ExtractDTag(
            identifier.tlv_fields)};
        break;
    }
    
    return filter;
  }
  
  void QueryRelays(const std::vector<std::string>& relay_urls,
                  const NostrFilter& filter,
                  ResolveCallback callback) {
    auto query = std::make_unique<RelayQuery>();
    query->filter = filter;
    query->callback = std::move(callback);
    query->pending_relays = relay_urls.size();
    
    for (const auto& relay_url : relay_urls) {
      relay_manager_->Query(
          relay_url, 
          filter,
          base::BindOnce(&NostrContentResolver::OnRelayResponse,
                        weak_factory_.GetWeakPtr(),
                        query.get()));
    }
    
    pending_queries_[query.get()] = std::move(query);
  }
  
  void OnRelayResponse(RelayQuery* query,
                      bool success,
                      std::vector<NostrEvent> events) {
    if (success && !events.empty()) {
      // Found content, cancel other queries
      auto content = std::make_unique<NostrContent>();
      content->events = std::move(events);
      
      // Cache the result
      cache_->Put(GetCacheKey(query->identifier), content);
      
      // Return result
      std::move(query->callback).Run(true, std::move(content));
      pending_queries_.erase(query);
      return;
    }
    
    // Check if all relays have responded
    if (--query->pending_relays == 0) {
      std::move(query->callback).Run(false, nullptr);
      pending_queries_.erase(query);
    }
  }
};
```

### 8. Content Rendering Implementation

```cpp
// browser/nostr_content_renderer.cc
class NostrContentRenderer {
 public:
  std::unique_ptr<content::WebContents> RenderContent(
      const NostrIdentifier& identifier,
      const NostrContent& content) {
    switch (identifier.type) {
      case NostrIdentifierType::NPUB:
      case NostrIdentifierType::NPROFILE:
        return RenderProfile(content);
        
      case NostrIdentifierType::NOTE:
      case NostrIdentifierType::NEVENT:
        return RenderNote(content);
        
      case NostrIdentifierType::NADDR:
        return RenderParameterizedEvent(content);
        
      case NostrIdentifierType::NSITE:
        return RenderNsite(identifier, content);
        
      default:
        return RenderError("Unsupported content type");
    }
  }
  
 private:
  std::unique_ptr<content::WebContents> RenderProfile(
      const NostrContent& content) {
    // Extract profile metadata
    NostrProfileData profile;
    for (const auto& event : content.events) {
      if (event.kind == 0) {
        profile = ParseProfileMetadata(event.content);
        break;
      }
    }
    
    // Generate HTML
    std::string html = GenerateProfileHTML(profile);
    
    // Create WebContents with generated HTML
    return CreateWebContentsFromHTML(html);
  }
  
  std::unique_ptr<content::WebContents> RenderNote(
      const NostrContent& content) {
    if (content.events.empty()) {
      return RenderError("Note not found");
    }
    
    const NostrEvent& note = content.events[0];
    std::string html = GenerateNoteHTML(note);
    
    return CreateWebContentsFromHTML(html);
  }
  
  std::string GenerateProfileHTML(const NostrProfileData& profile) {
    return base::StringPrintf(R"(
      <!DOCTYPE html>
      <html>
      <head>
        <meta charset="utf-8">
        <title>%s - Nostr Profile</title>
        <style>
          body { font-family: system-ui; max-width: 600px; margin: 0 auto; padding: 20px; }
          .avatar { width: 128px; height: 128px; border-radius: 50%%; }
          .name { font-size: 24px; font-weight: bold; }
          .about { margin-top: 10px; color: #666; }
        </style>
      </head>
      <body>
        <img class="avatar" src="%s" alt="Avatar">
        <h1 class="name">%s</h1>
        <p class="about">%s</p>
        <div class="details">
          <p><strong>NIP-05:</strong> %s</p>
          <p><strong>Website:</strong> <a href="%s">%s</a></p>
        </div>
      </body>
      </html>
    )", 
    profile.name.c_str(),
    profile.picture.c_str(),
    profile.name.c_str(),
    profile.about.c_str(),
    profile.nip05.c_str(),
    profile.website.c_str(),
    profile.website.c_str());
  }
};
```

### 9. URL Interception Implementation

```cpp
// chrome/browser/chrome_content_browser_client.cc
void ChromeContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  // Register Nostr URL rewriter
  handler->AddHandlerPair(&NostrURLRewriter::Rewrite,
                         &NostrURLRewriter::Reverse);
}

// browser/nostr_url_rewriter.cc
class NostrURLRewriter {
 public:
  static bool Rewrite(GURL* url, content::BrowserContext* context) {
    if (!url->SchemeIs("nostr")) {
      return false;
    }
    
    // Rewrite to internal chrome:// URL for handling
    std::string internal_url = base::StringPrintf(
        "chrome://nostr/%s", url->host().c_str());
    *url = GURL(internal_url);
    
    return true;
  }
  
  static bool Reverse(GURL* url, content::BrowserContext* context) {
    if (!url->SchemeIs("chrome") || url->host() != "nostr") {
      return false;
    }
    
    // Reverse the rewrite for display
    std::string nostr_url = base::StringPrintf(
        "nostr:%s", url->path().substr(1).c_str());
    *url = GURL(nostr_url);
    
    return true;
  }
};
```

### 10. Error Handling

```cpp
class NostrProtocolError {
 public:
  enum ErrorCode {
    INVALID_IDENTIFIER,
    NETWORK_ERROR,
    CONTENT_NOT_FOUND,
    RELAY_TIMEOUT,
    UNSUPPORTED_TYPE,
  };
  
  static std::unique_ptr<content::WebContents> CreateErrorPage(
      ErrorCode code,
      const std::string& details) {
    std::string html = base::StringPrintf(R"(
      <!DOCTYPE html>
      <html>
      <head>
        <title>Nostr Error</title>
        <style>
          body { font-family: system-ui; text-align: center; padding: 50px; }
          .error-code { font-size: 72px; color: #e74c3c; }
          .error-message { font-size: 24px; margin: 20px 0; }
          .details { color: #666; }
        </style>
      </head>
      <body>
        <div class="error-code">%s</div>
        <div class="error-message">%s</div>
        <div class="details">%s</div>
      </body>
      </html>
    )",
    GetErrorCodeString(code).c_str(),
    GetErrorMessage(code).c_str(),
    details.c_str());
    
    return CreateWebContentsFromHTML(html);
  }
  
 private:
  static std::string GetErrorCodeString(ErrorCode code) {
    switch (code) {
      case INVALID_IDENTIFIER: return "400";
      case NETWORK_ERROR: return "502";
      case CONTENT_NOT_FOUND: return "404";
      case RELAY_TIMEOUT: return "504";
      case UNSUPPORTED_TYPE: return "501";
    }
  }
};
```

### 11. Testing Strategy

```cpp
// Unit tests
TEST(NostrURLParserTest, ParsesNpubCorrectly) {
  NostrURLParser parser;
  GURL url("nostr:npub1sn0wdenkukak0d9dfczzeacvhkrgz92azqgxfemjvdlkqvlld2qspjdkge");
  
  auto result = parser.Parse(url);
  
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.identifier.type, NostrIdentifierType::NPUB);
  EXPECT_EQ(result.public_key, 
            "84de66e66dc5fb675caaC90592dc328dc8822954a0203");
}

TEST(Bech32DecoderTest, DecodesValidBech32) {
  auto result = Bech32Decoder::Decode(
      "npub1sn0wdenkukak0d9dfczzeacvhkrgz92azqgxfemjvdlkqvlld2qspjdkge");
  
  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.hrp, "npub");
  EXPECT_EQ(result.data.size(), 32);
}

// Integration tests
TEST(NostrProtocolHandlerTest, HandlesNostrURLs) {
  TestBrowserContext context;
  NostrProtocolHandler handler;
  
  GURL url("nostr:note1qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq3x4w0f");
  auto request = context.CreateRequest(url);
  
  auto job = handler.MaybeCreateJob(request.get(), nullptr);
  EXPECT_NE(job, nullptr);
}
```

### 12. Performance Optimizations

- **Parallel Relay Queries**: Query multiple relays simultaneously
- **Result Caching**: Cache resolved content with TTL
- **Early Termination**: Stop queries once content is found
- **Connection Pooling**: Reuse WebSocket connections
- **Preemptive Resolution**: Resolve visible links before click

### 13. Security Considerations

```cpp
// Content Security Policy for rendered Nostr content
const char kNostrContentCSP[] = 
    "default-src 'none'; "
    "img-src https: data: blob:; "
    "style-src 'unsafe-inline'; "
    "script-src 'none'; "
    "frame-src 'none';";

// Sanitize user-generated content
std::string SanitizeNostrContent(const std::string& content) {
  // Remove script tags
  // Escape HTML entities
  // Validate URLs
  return sanitized_content;
}
```