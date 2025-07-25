// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blossom/blossom_server.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "crypto/sha2.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blossom {

class BlossomServerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    
    ServerConfig config;
    config.bind_address = "127.0.0.1";
    config.port = 0;  // Let OS pick port
    config.storage_config.root_path = temp_dir_.GetPath();
    config.storage_config.max_total_size = 10 * 1024 * 1024;
    config.storage_config.max_blob_size = 1 * 1024 * 1024;
    config.storage_config.shard_depth = 2;
    
    server_ = std::make_unique<BlossomServer>(config);
    
    // Start server
    base::RunLoop run_loop;
    server_->Start(base::BindLambdaForTesting([&](bool success, 
                                                  const std::string& error) {
      ASSERT_TRUE(success) << error;
      run_loop.Quit();
    }));
    run_loop.Run();
    
    // Get server address
    server_address_ = server_->GetServerAddress();
    ASSERT_NE(0, server_address_.port());
  }

  void TearDown() override {
    if (server_) {
      base::RunLoop run_loop;
      server_->Stop(run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  std::string CalculateHash(const std::string& data) {
    std::string hash = crypto::SHA256HashString(data);
    return base::HexEncode(hash.data(), hash.size());
  }

  GURL GetServerURL(const std::string& path) {
    return GURL(base::StringPrintf("http://127.0.0.1:%d%s",
                                  server_address_.port(),
                                  path.c_str()));
  }

  // Simple HTTP client for testing
  std::unique_ptr<net::HttpResponseHeaders> SendRequest(
      const std::string& method,
      const GURL& url,
      const std::string& body,
      int* response_code,
      std::string* response_body) {
    net::TestDelegate delegate;
    auto request_context = base::MakeRefCounted<net::TestURLRequestContext>();
    
    auto request = request_context->CreateRequest(
        url, net::DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    
    request->set_method(method);
    if (!body.empty()) {
      auto upload_data = base::MakeRefCounted<net::UploadBytesElementReader>(
          body.data(), body.size());
      request->set_upload(net::ElementsUploadDataStream::CreateWithReader(
          std::move(upload_data), 0));
    }
    
    request->Start();
    delegate.RunUntilComplete();
    
    *response_code = request->GetResponseCode();
    *response_body = delegate.data_received();
    
    return request->response_headers() ? 
        request->response_headers()->Clone() : nullptr;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<BlossomServer> server_;
  net::IPEndPoint server_address_;
};

TEST_F(BlossomServerTest, GetNonExistentBlob) {
  int response_code;
  std::string response_body;
  auto headers = SendRequest("GET", 
      GetServerURL("/0000000000000000000000000000000000000000000000000000000000000000"),
      "", &response_code, &response_body);
  
  EXPECT_EQ(404, response_code);
  
  // Check CORS header
  std::string cors_header;
  ASSERT_TRUE(headers);
  EXPECT_TRUE(headers->GetNormalizedHeader("Access-Control-Allow-Origin", 
                                          &cors_header));
  EXPECT_EQ("*", cors_header);
}

TEST_F(BlossomServerTest, StoreAndRetrieveBlob) {
  std::string content = "Hello, Blossom!";
  std::string hash = CalculateHash(content);
  
  // Store blob via PUT
  int put_response_code;
  std::string put_response_body;
  auto put_headers = SendRequest("PUT",
      GetServerURL("/" + hash),
      content, &put_response_code, &put_response_body);
  
  EXPECT_EQ(201, put_response_code);  // Created
  
  // Retrieve blob via GET
  int get_response_code;
  std::string get_response_body;
  auto get_headers = SendRequest("GET",
      GetServerURL("/" + hash),
      "", &get_response_code, &get_response_body);
  
  EXPECT_EQ(200, get_response_code);
  EXPECT_EQ(content, get_response_body);
  
  // Check headers
  std::string content_type;
  EXPECT_TRUE(get_headers->GetNormalizedHeader("Content-Type", &content_type));
  EXPECT_EQ("application/octet-stream", content_type);
  
  std::string cache_control;
  EXPECT_TRUE(get_headers->GetNormalizedHeader("Cache-Control", 
                                               &cache_control));
  EXPECT_EQ("public, max-age=31536000, immutable", cache_control);
  
  std::string accept_ranges;
  EXPECT_TRUE(get_headers->GetNormalizedHeader("Accept-Ranges", 
                                               &accept_ranges));
  EXPECT_EQ("bytes", accept_ranges);
}

TEST_F(BlossomServerTest, HeadRequest) {
  std::string content = "Test content for HEAD";
  std::string hash = CalculateHash(content);
  
  // Store blob first
  int put_response_code;
  std::string put_response_body;
  SendRequest("PUT", GetServerURL("/" + hash),
              content, &put_response_code, &put_response_body);
  ASSERT_EQ(201, put_response_code);
  
  // Send HEAD request
  int head_response_code;
  std::string head_response_body;
  auto head_headers = SendRequest("HEAD",
      GetServerURL("/" + hash),
      "", &head_response_code, &head_response_body);
  
  EXPECT_EQ(200, head_response_code);
  EXPECT_TRUE(head_response_body.empty());  // No body for HEAD
  
  // Check headers
  std::string content_length;
  EXPECT_TRUE(head_headers->GetNormalizedHeader("Content-Length", 
                                                &content_length));
  EXPECT_EQ(base::NumberToString(content.size()), content_length);
}

TEST_F(BlossomServerTest, OptionsRequest) {
  int response_code;
  std::string response_body;
  auto headers = SendRequest("OPTIONS",
      GetServerURL("/"),
      "", &response_code, &response_body);
  
  EXPECT_EQ(200, response_code);
  
  // Check CORS headers
  std::string allow_methods;
  EXPECT_TRUE(headers->GetNormalizedHeader("Access-Control-Allow-Methods",
                                          &allow_methods));
  EXPECT_TRUE(allow_methods.find("GET") != std::string::npos);
  EXPECT_TRUE(allow_methods.find("PUT") != std::string::npos);
  EXPECT_TRUE(allow_methods.find("DELETE") != std::string::npos);
  
  std::string allow_headers;
  EXPECT_TRUE(headers->GetNormalizedHeader("Access-Control-Allow-Headers",
                                          &allow_headers));
  EXPECT_TRUE(allow_headers.find("Content-Type") != std::string::npos);
}

TEST_F(BlossomServerTest, GetWithFileExtension) {
  std::string content = "\x89PNG\r\n\x1a\n";  // PNG header
  std::string hash = CalculateHash(content);
  
  // Store blob
  int put_response_code;
  std::string put_response_body;
  SendRequest("PUT", GetServerURL("/" + hash),
              content, &put_response_code, &put_response_body);
  ASSERT_EQ(201, put_response_code);
  
  // Get with .png extension
  int get_response_code;
  std::string get_response_body;
  auto get_headers = SendRequest("GET",
      GetServerURL("/" + hash + ".png"),
      "", &get_response_code, &get_response_body);
  
  EXPECT_EQ(200, get_response_code);
  EXPECT_EQ(content, get_response_body);
}

TEST_F(BlossomServerTest, InvalidHashFormat) {
  // Too short
  int response_code;
  std::string response_body;
  SendRequest("GET", GetServerURL("/abc123"),
              "", &response_code, &response_body);
  EXPECT_EQ(404, response_code);
  
  // Invalid characters
  SendRequest("GET", GetServerURL("/zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"),
              "", &response_code, &response_body);
  EXPECT_EQ(404, response_code);
}

TEST_F(BlossomServerTest, DeleteBlob) {
  std::string content = "To be deleted";
  std::string hash = CalculateHash(content);
  
  // Store blob
  int put_response_code;
  std::string put_response_body;
  SendRequest("PUT", GetServerURL("/" + hash),
              content, &put_response_code, &put_response_body);
  ASSERT_EQ(201, put_response_code);
  
  // Delete blob
  int delete_response_code;
  std::string delete_response_body;
  SendRequest("DELETE", GetServerURL("/" + hash),
              "", &delete_response_code, &delete_response_body);
  EXPECT_EQ(204, delete_response_code);  // No Content
  
  // Try to get deleted blob
  int get_response_code;
  std::string get_response_body;
  SendRequest("GET", GetServerURL("/" + hash),
              "", &get_response_code, &get_response_body);
  EXPECT_EQ(404, get_response_code);
}

TEST_F(BlossomServerTest, ServerStats) {
  // Store some blobs
  for (int i = 0; i < 3; i++) {
    std::string content = base::StringPrintf("Blob %d", i);
    std::string hash = CalculateHash(content);
    
    int response_code;
    std::string response_body;
    SendRequest("PUT", GetServerURL("/" + hash),
                content, &response_code, &response_body);
    ASSERT_EQ(201, response_code);
  }
  
  // Get server stats
  base::RunLoop run_loop;
  server_->GetStats(base::BindLambdaForTesting([&](base::Value::Dict stats) {
    EXPECT_TRUE(stats.FindBool("running").value_or(false));
    EXPECT_EQ(3.0, stats.FindDouble("blob_count").value_or(0));
    EXPECT_GT(stats.FindDouble("storage_size").value_or(0), 0);
    run_loop.Quit();
  }));
  run_loop.Run();
}

// TODO: Add tests for:
// - Range requests (partial content)
// - Authorization (when implemented)
// - Hash mismatch rejection
// - Storage limits
// - Concurrent requests

}  // namespace blossom