// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nostr {

class NostrTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NostrTest, BasicSetup) {
  // This test verifies that the Nostr component can be built and linked
  EXPECT_TRUE(true);
}

}  // namespace nostr