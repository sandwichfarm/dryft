// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOSTR_MESSAGE_START_H_
#define CHROME_COMMON_NOSTR_MESSAGE_START_H_

// Define the IPC message start value for Nostr messages.
// This should be a unique value that doesn't conflict with other Chrome IPC messages.
// In Chromium, these are typically defined in ipc/ipc_message_start.h
enum {
  NostrMsgStart = 0x8000,  // High value to avoid conflicts
};

#endif  // CHROME_COMMON_NOSTR_MESSAGE_START_H_