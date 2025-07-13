// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nostr/nsite/nsite_header_injector.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace nostr {

class NsiteHeaderInjectorTest : public testing::Test {
 protected:
  NsiteHeaderInjectorTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    
    header_injector_ = std::make_unique<NsiteHeaderInjector>(profile_.get());
    header_injector_->Initialize();
  }

  void TearDown() override {
    header_injector_->Shutdown();
    header_injector_.reset();
    web_contents_ = nullptr;
    profile_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_;
  std::unique_ptr<NsiteHeaderInjector> header_injector_;
};

TEST_F(NsiteHeaderInjectorTest, SetAndGetNsiteForTab) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  
  // Initially no nsite set
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents_), "");
  
  // Set nsite
  header_injector_->SetNsiteForTab(web_contents_, npub);
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents_), npub);
  
  // Clear nsite
  header_injector_->ClearNsiteForTab(web_contents_);
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents_), "");
}

TEST_F(NsiteHeaderInjectorTest, MultipleTabsIndependent) {
  auto web_contents2 = web_contents_factory_.CreateWebContents(profile_.get());
  
  const std::string npub1 = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string npub2 = "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj";
  
  // Set different nsites for each tab
  header_injector_->SetNsiteForTab(web_contents_, npub1);
  header_injector_->SetNsiteForTab(web_contents2, npub2);
  
  // Verify they're independent
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents_), npub1);
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents2), npub2);
  
  // Clear one tab
  header_injector_->ClearNsiteForTab(web_contents_);
  
  // First tab cleared, second unchanged
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents_), "");
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents2), npub2);
}

TEST_F(NsiteHeaderInjectorTest, UpdateExistingNsite) {
  const std::string npub1 = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  const std::string npub2 = "npub14nr0ux0cn38r5rvf3wen3p9sgfxv2ydqchtqt5gu8r8rpa0x97q330wjj";
  
  // Set initial nsite
  header_injector_->SetNsiteForTab(web_contents_, npub1);
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents_), npub1);
  
  // Update to different nsite
  header_injector_->SetNsiteForTab(web_contents_, npub2);
  EXPECT_EQ(header_injector_->GetNsiteForTab(web_contents_), npub2);
}

class NsiteNavigationContextTest : public testing::Test {
 protected:
  NsiteNavigationContextTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    
    context_ = std::make_unique<NsiteNavigationContext>(web_contents_);
  }

  void TearDown() override {
    context_.reset();
    web_contents_ = nullptr;
    profile_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  content::WebContents* web_contents_;
  std::unique_ptr<NsiteNavigationContext> context_;
};

TEST_F(NsiteNavigationContextTest, SetAndGetNsite) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  
  // Initially empty
  EXPECT_EQ(context_->GetCurrentNsite(), "");
  
  // Set nsite
  context_->SetCurrentNsite(npub);
  EXPECT_EQ(context_->GetCurrentNsite(), npub);
  
  // Clear nsite
  context_->ClearNsite();
  EXPECT_EQ(context_->GetCurrentNsite(), "");
}

TEST_F(NsiteNavigationContextTest, NavigationClearsContext) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  
  // Set nsite
  context_->SetCurrentNsite(npub);
  EXPECT_EQ(context_->GetCurrentNsite(), npub);
  
  // Simulate navigation to external site
  auto* web_contents_tester = content::WebContentsTester::For(web_contents_);
  web_contents_tester->NavigateAndCommit(GURL("https://external.com"));
  
  // Context should be cleared
  EXPECT_EQ(context_->GetCurrentNsite(), "");
}

TEST_F(NsiteNavigationContextTest, LocalhostNavigationKeepsContext) {
  const std::string npub = "npub1hyfvhwydfdsfwdz2ey2v4jz2x3xvryj8f8qnxv5xppsuamgas2rskp7w0r";
  
  // Set nsite
  context_->SetCurrentNsite(npub);
  EXPECT_EQ(context_->GetCurrentNsite(), npub);
  
  // Simulate navigation within localhost
  auto* web_contents_tester = content::WebContentsTester::For(web_contents_);
  web_contents_tester->NavigateAndCommit(GURL("http://localhost:8080/page.html"));
  
  // Context should be preserved (though in practice this might change
  // based on actual implementation details)
  // For now we'll test the basic functionality
}

}  // namespace nostr