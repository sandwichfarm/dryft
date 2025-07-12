// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/nostr/mac_keychain_manager.h"

#import <Security/Security.h>

@implementation MacKeychainManager

- (BOOL)storeData:(NSData*)data
       forService:(NSString*)service
          account:(NSString*)account
            error:(NSError**)error {
  if (!data || !service || !account) {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:errSecParam
                               userInfo:@{NSLocalizedDescriptionKey: @"Invalid parameters"}];
    }
    return NO;
  }
  
  // Delete existing item first (update doesn't work if item doesn't exist)
  [self deleteItemForService:service account:account error:nil];
  
  // Create keychain item attributes
  NSDictionary* attributes = @{
    (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrService: service,
    (__bridge id)kSecAttrAccount: account,
    (__bridge id)kSecValueData: data,
    (__bridge id)kSecAttrAccessible: (__bridge id)kSecAttrAccessibleWhenUnlocked,
    (__bridge id)kSecAttrLabel: @"Tungsten Nostr Key",
    (__bridge id)kSecAttrDescription: @"Encrypted Nostr private key",
    (__bridge id)kSecAttrSynchronizable: @NO  // Don't sync to iCloud
  };
  
  OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attributes, NULL);
  
  if (status != errSecSuccess) {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:status
                               userInfo:@{NSLocalizedDescriptionKey: 
                                         [NSString stringWithFormat:@"Failed to store item: %d", (int)status]}];
    }
    return NO;
  }
  
  return YES;
}

- (NSData*)retrieveDataForService:(NSString*)service
                          account:(NSString*)account
                            error:(NSError**)error {
  if (!service || !account) {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:errSecParam
                               userInfo:@{NSLocalizedDescriptionKey: @"Invalid parameters"}];
    }
    return nil;
  }
  
  NSDictionary* query = @{
    (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrService: service,
    (__bridge id)kSecAttrAccount: account,
    (__bridge id)kSecReturnData: @YES,
    (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitOne
  };
  
  CFTypeRef result = NULL;
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
  
  if (status == errSecSuccess && result) {
    NSData* data = (__bridge_transfer NSData*)result;
    return data;
  } else if (status == errSecItemNotFound) {
    // Not an error, just not found
    return nil;
  } else {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:status
                               userInfo:@{NSLocalizedDescriptionKey: 
                                         [NSString stringWithFormat:@"Failed to retrieve item: %d", (int)status]}];
    }
    return nil;
  }
}

- (BOOL)deleteItemForService:(NSString*)service
                     account:(NSString*)account
                       error:(NSError**)error {
  if (!service || !account) {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:errSecParam
                               userInfo:@{NSLocalizedDescriptionKey: @"Invalid parameters"}];
    }
    return NO;
  }
  
  NSDictionary* query = @{
    (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrService: service,
    (__bridge id)kSecAttrAccount: account
  };
  
  OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
  
  if (status == errSecSuccess || status == errSecItemNotFound) {
    return YES;
  } else {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:status
                               userInfo:@{NSLocalizedDescriptionKey: 
                                         [NSString stringWithFormat:@"Failed to delete item: %d", (int)status]}];
    }
    return NO;
  }
}

- (NSArray<NSString*>*)listAccountsForService:(NSString*)service
                                         error:(NSError**)error {
  if (!service) {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:errSecParam
                               userInfo:@{NSLocalizedDescriptionKey: @"Invalid parameters"}];
    }
    return nil;
  }
  
  NSDictionary* query = @{
    (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrService: service,
    (__bridge id)kSecReturnAttributes: @YES,
    (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitAll
  };
  
  CFTypeRef result = NULL;
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
  
  if (status == errSecSuccess && result) {
    NSArray* items = (__bridge_transfer NSArray*)result;
    NSMutableArray* accounts = [NSMutableArray array];
    
    for (NSDictionary* item in items) {
      NSString* account = item[(__bridge id)kSecAttrAccount];
      if (account) {
        [accounts addObject:account];
      }
    }
    
    return accounts;
  } else if (status == errSecItemNotFound) {
    return @[];  // Empty array, not an error
  } else {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:status
                               userInfo:@{NSLocalizedDescriptionKey: 
                                         [NSString stringWithFormat:@"Failed to list items: %d", (int)status]}];
    }
    return nil;
  }
}

- (BOOL)updateData:(NSData*)data
        forService:(NSString*)service
           account:(NSString*)account
             error:(NSError**)error {
  if (!data || !service || !account) {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:errSecParam
                               userInfo:@{NSLocalizedDescriptionKey: @"Invalid parameters"}];
    }
    return NO;
  }
  
  NSDictionary* query = @{
    (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrService: service,
    (__bridge id)kSecAttrAccount: account
  };
  
  NSDictionary* updateAttributes = @{
    (__bridge id)kSecValueData: data
  };
  
  OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query,
                                  (__bridge CFDictionaryRef)updateAttributes);
  
  if (status == errSecSuccess) {
    return YES;
  } else if (status == errSecItemNotFound) {
    // Item doesn't exist, create it
    return [self storeData:data forService:service account:account error:error];
  } else {
    if (error) {
      *error = [NSError errorWithDomain:@"TungstenKeychain"
                                   code:status
                               userInfo:@{NSLocalizedDescriptionKey: 
                                         [NSString stringWithFormat:@"Failed to update item: %d", (int)status]}];
    }
    return NO;
  }
}

- (BOOL)hasItemForService:(NSString*)service
                  account:(NSString*)account {
  if (!service || !account) {
    return NO;
  }
  
  NSDictionary* query = @{
    (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrService: service,
    (__bridge id)kSecAttrAccount: account,
    (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitOne
  };
  
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
  return status == errSecSuccess;
}

@end