// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOSTR_MAC_KEYCHAIN_MANAGER_H_
#define CHROME_BROWSER_NOSTR_MAC_KEYCHAIN_MANAGER_H_

#import <Foundation/Foundation.h>

// Objective-C interface for managing macOS Keychain operations
@interface MacKeychainManager : NSObject

// Store data in keychain
- (BOOL)storeData:(NSData*)data
       forService:(NSString*)service
          account:(NSString*)account
            error:(NSError**)error;

// Retrieve data from keychain
- (NSData*)retrieveDataForService:(NSString*)service
                          account:(NSString*)account
                            error:(NSError**)error;

// Delete item from keychain
- (BOOL)deleteItemForService:(NSString*)service
                     account:(NSString*)account
                       error:(NSError**)error;

// List all accounts for a service
- (NSArray<NSString*>*)listAccountsForService:(NSString*)service
                                         error:(NSError**)error;

// Update existing keychain item
- (BOOL)updateData:(NSData*)data
        forService:(NSString*)service
           account:(NSString*)account
             error:(NSError**)error;

// Check if item exists
- (BOOL)hasItemForService:(NSString*)service
                  account:(NSString*)account;

@end

#endif  // CHROME_BROWSER_NOSTR_MAC_KEYCHAIN_MANAGER_H_