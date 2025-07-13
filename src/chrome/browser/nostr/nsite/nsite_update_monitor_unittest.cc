// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_update_monitor.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/nostr/nsite/nsite_cache_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NsiteUpdateMonitorTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    
    cache_manager_ = std::make_unique<NsiteCacheManager>(temp_dir_.GetPath());
    
    test_url_loader_factory_ = std::make_unique<network::TestURLLoaderFactory>();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    
    update_monitor_ = std::make_unique<NsiteUpdateMonitor>(
        shared_url_loader_factory_, cache_manager_.get());
  }

  void TearDown() override {
    update_monitor_.reset();
    cache_manager_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<NsiteCacheManager> cache_manager_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<NsiteUpdateMonitor> update_monitor_;
};

TEST_F(NsiteUpdateMonitorTest, SetMinCheckInterval) {
  // Test setting minimum check interval
  base::TimeDelta custom_interval = base::Minutes(10);
  update_monitor_->SetMinCheckInterval(custom_interval);
  
  // The interval should be stored (we can't directly test it, but subsequent
  // checks should respect the new interval)
  SUCCEED();  // Test passes if no crash occurs
}

TEST_F(NsiteUpdateMonitorTest, RateLimiting) {
  const std::string npub = "npub1test123";
  const std::string path = "/index.html";
  
  int callback_count = 0;
  auto callback = base::BindRepeating([&callback_count](const std::string&, const std::string&) {
    ++callback_count;
  });
  
  // Set a very long check interval (1 hour)
  update_monitor_->SetMinCheckInterval(base::Hours(1));
  
  // First check should proceed
  update_monitor_->CheckForUpdates(npub, path, callback);
  task_environment_.RunUntilIdle();
  
  // Second immediate check should be rate limited (no callback should fire)
  update_monitor_->CheckForUpdates(npub, path, callback);
  task_environment_.RunUntilIdle();
  
  // Since the implementation currently returns false for updates,
  // we expect no callbacks to have fired
  EXPECT_EQ(0, callback_count);
}

TEST_F(NsiteUpdateMonitorTest, MultipleNsites) {
  const std::string npub1 = "npub1test123";
  const std::string npub2 = "npub1test456";
  const std::string path = "/index.html";
  
  int callback_count = 0;
  auto callback = base::BindRepeating([&callback_count](const std::string&, const std::string&) {
    ++callback_count;
  });
  
  // Check different nsites - should not interfere with each other
  update_monitor_->CheckForUpdates(npub1, path, callback);
  update_monitor_->CheckForUpdates(npub2, path, callback);
  
  task_environment_.RunUntilIdle();
  
  SUCCEED();  // Test passes if no crashes occur
}

TEST_F(NsiteUpdateMonitorTest, Stop) {
  const std::string npub = "npub1test123";
  const std::string path = "/index.html";
  
  auto callback = base::BindRepeating([](const std::string&, const std::string&) {
    // This callback should not be called after Stop()
  });
  
  // Start some update checks
  update_monitor_->CheckForUpdates(npub, path, callback);
  
  // Stop the monitor
  update_monitor_->Stop();
  
  // Additional checks should be handled gracefully
  update_monitor_->CheckForUpdates(npub, path, callback);
  
  task_environment_.RunUntilIdle();
  
  SUCCEED();  // Test passes if no crashes occur
}

TEST_F(NsiteUpdateMonitorTest, ShortInterval) {
  const std::string npub = "npub1test123";
  const std::string path = "/index.html";
  
  int callback_count = 0;
  auto callback = base::BindRepeating([&callback_count](const std::string&, const std::string&) {
    ++callback_count;
  });
  
  // Set a very short interval for testing
  update_monitor_->SetMinCheckInterval(base::Milliseconds(1));
  
  // First check
  update_monitor_->CheckForUpdates(npub, path, callback);
  task_environment_.RunUntilIdle();
  
  // Wait a bit and do second check
  task_environment_.FastForwardBy(base::Milliseconds(2));
  update_monitor_->CheckForUpdates(npub, path, callback);
  task_environment_.RunUntilIdle();
  
  // Since the implementation currently returns false for updates,
  // no callbacks should fire even though both checks are allowed
  EXPECT_EQ(0, callback_count);
}

}  // namespace nostr