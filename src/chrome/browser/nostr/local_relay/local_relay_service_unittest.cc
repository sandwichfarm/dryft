// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/local_relay/local_relay_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {
namespace local_relay {

namespace {

// Test WebSocket client for connecting to the local relay
class TestWebSocketClient {
 public:
  TestWebSocketClient() = default;
  ~TestWebSocketClient() = default;

  bool Connect(const net::IPEndPoint& endpoint) {
    socket_ = std::make_unique<net::TCPClientSocket>(
        net::AddressList(endpoint), nullptr, nullptr, nullptr,
        net::NetLogSource());
    
    net::TestCompletionCallback callback;
    int rv = socket_->Connect(callback.callback());
    if (rv != net::ERR_IO_PENDING) {
      return rv == net::OK;
    }
    
    return callback.WaitForResult() == net::OK;
  }
  
  bool SendWebSocketUpgrade() {
    std::string request = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "\r\n";
    
    scoped_refptr<net::StringIOBuffer> buffer = 
        base::MakeRefCounted<net::StringIOBuffer>(request);
    
    net::TestCompletionCallback callback;
    int rv = socket_->Write(buffer.get(), request.size(), 
                           callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    
    if (rv == net::ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    
    return rv == static_cast<int>(request.size());
  }
  
  bool ReadResponse(std::string* response) {
    const int kBufferSize = 4096;
    scoped_refptr<net::IOBuffer> buffer = 
        base::MakeRefCounted<net::IOBuffer>(kBufferSize);
    
    net::TestCompletionCallback callback;
    int rv = socket_->Read(buffer.get(), kBufferSize, callback.callback());
    
    if (rv == net::ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    
    if (rv > 0) {
      *response = std::string(buffer->data(), rv);
      return true;
    }
    
    return false;
  }
  
  void Disconnect() {
    socket_.reset();
  }

 private:
  std::unique_ptr<net::TCPClientSocket> socket_;
};

}  // namespace

class LocalRelayServiceTest : public testing::Test {
 public:
  LocalRelayServiceTest() = default;
  ~LocalRelayServiceTest() override = default;

  void SetUp() override {
    service_ = std::make_unique<LocalRelayService>();
  }

  void TearDown() override {
    if (service_->IsRunning()) {
      base::RunLoop run_loop;
      service_->Stop(run_loop.QuitClosure());
      run_loop.Run();
    }
    service_.reset();
  }

 protected:
  void StartService(const LocalRelayConfig& config = LocalRelayConfig()) {
    base::RunLoop run_loop;
    bool success = false;
    
    service_->Start(config, base::BindOnce(
        [](bool* out_success, base::OnceClosure quit, bool result) {
          *out_success = result;
          std::move(quit).Run();
        },
        &success, run_loop.QuitClosure()));
    
    run_loop.Run();
    ASSERT_TRUE(success);
  }
  
  // Helper to create a test event
  base::Value CreateTestEvent(const std::string& content) {
    base::Value::Dict event;
    event.Set("id", std::string(64, 'a'));
    event.Set("pubkey", std::string(64, 'b'));
    event.Set("created_at", 1234567890.0);
    event.Set("kind", 1);
    event.Set("tags", base::Value::List());
    event.Set("content", content);
    event.Set("sig", std::string(128, 'c'));
    return base::Value(std::move(event));
  }
  
  // Helper to create EVENT message
  std::string CreateEventMessage(const base::Value& event) {
    base::Value::List message;
    message.Append("EVENT");
    message.Append(event.Clone());
    
    std::string json;
    base::JSONWriter::Write(message, &json);
    return json;
  }
  
  // Helper to create REQ message
  std::string CreateReqMessage(const std::string& subscription_id) {
    base::Value::List message;
    message.Append("REQ");
    message.Append(subscription_id);
    
    base::Value::Dict filter;
    filter.Set("limit", 10);
    message.Append(base::Value(std::move(filter)));
    
    std::string json;
    base::JSONWriter::Write(message, &json);
    return json;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<LocalRelayService> service_;
};

// Test basic service lifecycle
TEST_F(LocalRelayServiceTest, StartStop) {
  EXPECT_FALSE(service_->IsRunning());
  
  StartService();
  EXPECT_TRUE(service_->IsRunning());
  
  auto address = service_->GetLocalAddress();
  EXPECT_EQ("127.0.0.1", address.address().ToString());
  EXPECT_GT(address.port(), 0);
  
  base::RunLoop run_loop;
  service_->Stop(run_loop.QuitClosure());
  run_loop.Run();
  
  EXPECT_FALSE(service_->IsRunning());
}

// Test custom configuration
TEST_F(LocalRelayServiceTest, CustomConfig) {
  LocalRelayConfig config;
  config.bind_address = "127.0.0.1";
  config.port = 0;  // Let OS assign port
  config.max_connections = 50;
  
  StartService(config);
  
  auto address = service_->GetLocalAddress();
  EXPECT_EQ("127.0.0.1", address.address().ToString());
  EXPECT_GT(address.port(), 0);
}

// Test WebSocket connection
TEST_F(LocalRelayServiceTest, WebSocketConnection) {
  StartService();
  
  TestWebSocketClient client;
  ASSERT_TRUE(client.Connect(service_->GetLocalAddress()));
  ASSERT_TRUE(client.SendWebSocketUpgrade());
  
  std::string response;
  ASSERT_TRUE(client.ReadResponse(&response));
  
  // Check for WebSocket upgrade response
  EXPECT_NE(std::string::npos, response.find("HTTP/1.1 101"));
  EXPECT_NE(std::string::npos, response.find("Upgrade: websocket"));
  
  EXPECT_EQ(1, service_->GetConnectionCount());
  
  client.Disconnect();
  
  // Give time for disconnect to process
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, service_->GetConnectionCount());
}

// Test statistics
TEST_F(LocalRelayServiceTest, Statistics) {
  StartService();
  
  auto stats = service_->GetStatistics();
  EXPECT_TRUE(stats.FindBool("running").value_or(false));
  EXPECT_EQ(service_->GetLocalAddress().ToString(), 
            *stats.FindString("address"));
  
  auto* connections = stats.FindDict("connections");
  ASSERT_TRUE(connections);
  EXPECT_EQ(0, *connections->FindInt("connection_count"));
  EXPECT_EQ(0, *connections->FindInt("total_subscriptions"));
}

// Test multiple connections
TEST_F(LocalRelayServiceTest, MultipleConnections) {
  LocalRelayConfig config;
  config.max_connections = 2;
  StartService(config);
  
  TestWebSocketClient client1, client2, client3;
  
  // First two connections should succeed
  ASSERT_TRUE(client1.Connect(service_->GetLocalAddress()));
  ASSERT_TRUE(client1.SendWebSocketUpgrade());
  
  ASSERT_TRUE(client2.Connect(service_->GetLocalAddress()));
  ASSERT_TRUE(client2.SendWebSocketUpgrade());
  
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, service_->GetConnectionCount());
  
  // Third connection should be rejected
  ASSERT_TRUE(client3.Connect(service_->GetLocalAddress()));
  ASSERT_TRUE(client3.SendWebSocketUpgrade());
  
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, service_->GetConnectionCount());  // Still only 2
}

}  // namespace local_relay
}  // namespace nostr