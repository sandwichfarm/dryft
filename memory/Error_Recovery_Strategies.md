# Error Recovery Strategies
## Tungsten Browser - Comprehensive Error Handling and Recovery

### 1. Error Classification and Hierarchy

```cpp
namespace nostr {
namespace errors {

enum class ErrorCategory {
  NETWORK,          // Connection failures, timeouts
  CRYPTOGRAPHIC,    // Key operations, signatures
  STORAGE,          // Database, file system
  PROTOCOL,         // Invalid Nostr messages
  AUTHENTICATION,   // Permission, authorization
  RESOURCE,         // Memory, disk space
  USER_INPUT,       // Invalid user data
  SYSTEM            // OS-level failures
};

enum class ErrorSeverity {
  CRITICAL,   // Unrecoverable, requires restart
  SEVERE,     // Major functionality broken
  MODERATE,   // Degraded functionality
  MINOR,      // Cosmetic or minor inconvenience
  INFO        // Informational only
};

class NostrError {
 public:
  NostrError(ErrorCategory category,
            ErrorSeverity severity,
            const std::string& message,
            std::unique_ptr<ErrorContext> context = nullptr)
      : category_(category),
        severity_(severity),
        message_(message),
        context_(std::move(context)),
        timestamp_(base::Time::Now()) {}
  
  // Recovery strategies based on error type
  virtual RecoveryStrategy GetRecoveryStrategy() const = 0;
  virtual bool IsRetryable() const = 0;
  virtual base::TimeDelta GetRetryDelay() const;
  
 protected:
  ErrorCategory category_;
  ErrorSeverity severity_;
  std::string message_;
  std::unique_ptr<ErrorContext> context_;
  base::Time timestamp_;
};

}  // namespace errors
}  // namespace nostr
```

### 2. Network Error Recovery

```cpp
class NetworkErrorRecovery {
 public:
  enum class NetworkErrorType {
    CONNECTION_FAILED,
    CONNECTION_TIMEOUT,
    CONNECTION_LOST,
    DNS_RESOLUTION_FAILED,
    SSL_HANDSHAKE_FAILED,
    WEBSOCKET_PROTOCOL_ERROR,
    RELAY_UNAVAILABLE,
    RATE_LIMITED
  };
  
  struct RecoveryConfig {
    int max_retry_attempts = 5;
    base::TimeDelta initial_retry_delay = base::Seconds(1);
    double backoff_multiplier = 2.0;
    base::TimeDelta max_retry_delay = base::Minutes(5);
    bool use_exponential_backoff = true;
    bool use_jitter = true;
  };
  
  class NetworkError : public NostrError {
   public:
    RecoveryStrategy GetRecoveryStrategy() const override {
      switch (error_type_) {
        case NetworkErrorType::CONNECTION_FAILED:
        case NetworkErrorType::CONNECTION_TIMEOUT:
          return RecoveryStrategy{
            .action = RecoveryAction::RETRY_WITH_BACKOFF,
            .fallback = RecoveryAction::USE_ALTERNATIVE_RELAY,
            .notify_user = severity_ >= ErrorSeverity::MODERATE
          };
          
        case NetworkErrorType::CONNECTION_LOST:
          return RecoveryStrategy{
            .action = RecoveryAction::RECONNECT_IMMEDIATELY,
            .fallback = RecoveryAction::QUEUE_MESSAGES,
            .notify_user = false  // Silent reconnection
          };
          
        case NetworkErrorType::RELAY_UNAVAILABLE:
          return RecoveryStrategy{
            .action = RecoveryAction::USE_ALTERNATIVE_RELAY,
            .fallback = RecoveryAction::OPERATE_OFFLINE,
            .notify_user = true
          };
          
        case NetworkErrorType::RATE_LIMITED:
          return RecoveryStrategy{
            .action = RecoveryAction::THROTTLE_REQUESTS,
            .fallback = RecoveryAction::WAIT_AND_RETRY,
            .notify_user = false,
            .retry_after = ParseRetryAfterHeader()
          };
          
        default:
          return RecoveryStrategy{
            .action = RecoveryAction::LOG_AND_CONTINUE,
            .notify_user = false
          };
      }
    }
    
    bool IsRetryable() const override {
      return error_type_ != NetworkErrorType::SSL_HANDSHAKE_FAILED &&
             retry_count_ < config_.max_retry_attempts;
    }
  };
  
  // Automatic reconnection with circuit breaker
  void HandleConnectionLost(const std::string& relay_url) {
    auto& state = connection_states_[relay_url];
    
    if (state.circuit_breaker.IsOpen()) {
      LOG(WARNING) << "Circuit breaker open for " << relay_url;
      // Try alternative relay
      auto alternative = relay_selector_->GetAlternative(relay_url);
      if (!alternative.empty()) {
        SwitchToRelay(alternative);
      }
      return;
    }
    
    // Attempt immediate reconnection
    state.reconnect_timer = std::make_unique<base::OneShotTimer>();
    state.reconnect_timer->Start(
        FROM_HERE,
        base::Milliseconds(100),  // Small delay to prevent tight loop
        base::BindOnce(&NetworkErrorRecovery::AttemptReconnection,
                      weak_factory_.GetWeakPtr(),
                      relay_url));
  }
  
 private:
  void AttemptReconnection(const std::string& relay_url) {
    auto& state = connection_states_[relay_url];
    
    connection_manager_->Connect(
        relay_url,
        base::BindOnce(&NetworkErrorRecovery::OnReconnectionResult,
                      weak_factory_.GetWeakPtr(),
                      relay_url));
    
    // Set timeout for reconnection attempt
    state.reconnect_timeout_timer = std::make_unique<base::OneShotTimer>();
    state.reconnect_timeout_timer->Start(
        FROM_HERE,
        config_.connection_timeout,
        base::BindOnce(&NetworkErrorRecovery::OnReconnectionTimeout,
                      weak_factory_.GetWeakPtr(),
                      relay_url));
  }
};
```

### 3. Cryptographic Error Recovery

```cpp
class CryptoErrorRecovery {
 public:
  enum class CryptoErrorType {
    KEY_NOT_FOUND,
    KEY_CORRUPTED,
    SIGNATURE_VERIFICATION_FAILED,
    DECRYPTION_FAILED,
    KEY_DERIVATION_FAILED,
    HARDWARE_KEY_UNAVAILABLE,
    KEYCHAIN_ACCESS_DENIED
  };
  
  class CryptoError : public NostrError {
   public:
    RecoveryStrategy GetRecoveryStrategy() const override {
      switch (error_type_) {
        case CryptoErrorType::KEY_NOT_FOUND:
          return RecoveryStrategy{
            .action = RecoveryAction::PROMPT_KEY_IMPORT,
            .fallback = RecoveryAction::CREATE_NEW_KEY,
            .notify_user = true,
            .user_action_required = true
          };
          
        case CryptoErrorType::KEY_CORRUPTED:
          return RecoveryStrategy{
            .action = RecoveryAction::RESTORE_FROM_BACKUP,
            .fallback = RecoveryAction::PROMPT_KEY_RECOVERY,
            .notify_user = true,
            .severity = ErrorSeverity::SEVERE
          };
          
        case CryptoErrorType::KEYCHAIN_ACCESS_DENIED:
          return RecoveryStrategy{
            .action = RecoveryAction::REQUEST_PERMISSION,
            .fallback = RecoveryAction::USE_ALTERNATIVE_STORAGE,
            .notify_user = true,
            .platform_specific = true
          };
          
        case CryptoErrorType::HARDWARE_KEY_UNAVAILABLE:
          return RecoveryStrategy{
            .action = RecoveryAction::WAIT_FOR_DEVICE,
            .fallback = RecoveryAction::USE_SOFTWARE_KEY,
            .notify_user = true,
            .retry_interval = base::Seconds(2)
          };
          
        default:
          return RecoveryStrategy{
            .action = RecoveryAction::LOG_ERROR,
            .notify_user = severity_ >= ErrorSeverity::MODERATE
          };
      }
    }
  };
  
  // Key recovery flow
  void RecoverCorruptedKey(const std::string& account_id) {
    // Step 1: Check for local backup
    auto backup = backup_manager_->GetLatestBackup(account_id);
    if (backup && backup->IsValid()) {
      if (RestoreFromBackup(backup)) {
        NotifyUser("Key restored from backup");
        return;
      }
    }
    
    // Step 2: Try to recover from seed phrase
    ShowKeyRecoveryDialog(
        base::BindOnce(&CryptoErrorRecovery::OnSeedPhraseProvided,
                      weak_factory_.GetWeakPtr(),
                      account_id));
  }
  
  void OnSeedPhraseProvided(const std::string& account_id,
                           const std::vector<std::string>& seed_words) {
    if (seed_words.empty()) {
      // User cancelled
      HandleKeyRecoveryFailed(account_id);
      return;
    }
    
    // Derive key from seed
    auto key = key_derivation_->DeriveFromSeed(seed_words, account_id);
    if (!key) {
      ShowError("Invalid seed phrase");
      return;
    }
    
    // Restore key
    if (key_storage_->StoreKey(account_id, *key)) {
      NotifyUser("Key successfully recovered");
      
      // Re-sync account data
      sync_manager_->ResyncAccount(account_id);
    }
  }
};
```

### 4. Storage Error Recovery

```cpp
class StorageErrorRecovery {
 public:
  enum class StorageErrorType {
    DATABASE_CORRUPTED,
    DISK_FULL,
    PERMISSION_DENIED,
    IO_ERROR,
    QUOTA_EXCEEDED,
    MIGRATION_FAILED
  };
  
  void HandleDatabaseCorruption() {
    LOG(ERROR) << "Database corruption detected";
    
    // Step 1: Attempt automatic repair
    if (database_->Repair()) {
      LOG(INFO) << "Database repaired successfully";
      return;
    }
    
    // Step 2: Restore from backup
    auto backup_path = GetLatestBackupPath();
    if (!backup_path.empty() && RestoreDatabase(backup_path)) {
      LOG(INFO) << "Database restored from backup";
      return;
    }
    
    // Step 3: Rebuild from scratch
    LOG(WARNING) << "Rebuilding database from scratch";
    
    // Save critical data to temporary storage
    auto critical_data = ExtractCriticalData();
    
    // Delete corrupted database
    database_->Close();
    base::DeleteFile(database_path_);
    
    // Create new database
    database_->Open(database_path_);
    
    // Restore critical data
    RestoreCriticalData(critical_data);
    
    // Re-sync from relays
    sync_manager_->FullResync();
  }
  
  void HandleDiskFull() {
    // Step 1: Clean up cache
    size_t freed = cache_manager_->EmergencyCleanup();
    LOG(INFO) << "Freed " << freed << " bytes from cache";
    
    if (GetAvailableSpace() > min_required_space_) {
      return;
    }
    
    // Step 2: Delete old events
    int deleted = event_storage_->DeleteOldEvents(
        base::Time::Now() - base::Days(7));
    LOG(INFO) << "Deleted " << deleted << " old events";
    
    if (GetAvailableSpace() > min_required_space_) {
      return;
    }
    
    // Step 3: Notify user
    ShowStorageFullDialog(
        base::BindOnce(&StorageErrorRecovery::OnUserStorageChoice,
                      weak_factory_.GetWeakPtr()));
  }
  
  // Automatic migration recovery
  void RecoverFromFailedMigration(int from_version, int to_version) {
    LOG(ERROR) << "Migration failed: v" << from_version 
               << " -> v" << to_version;
    
    // Create backup of current state
    BackupDatabase("pre_migration_recovery");
    
    // Rollback to previous version
    if (migration_manager_->Rollback(from_version)) {
      LOG(INFO) << "Successfully rolled back to v" << from_version;
      
      // Disable feature requiring new version
      feature_flags_->DisableIncompatibleFeatures(from_version);
      
      // Schedule retry for next browser restart
      prefs_->SetInteger(prefs::kPendingMigrationVersion, to_version);
    } else {
      // Rollback failed, try forward recovery
      AttemptForwardRecovery(from_version, to_version);
    }
  }
};
```

### 5. Protocol Error Recovery

```cpp
class ProtocolErrorRecovery {
 public:
  enum class ProtocolErrorType {
    INVALID_EVENT_FORMAT,
    SIGNATURE_MISMATCH,
    INVALID_FILTER,
    UNSUPPORTED_NIP,
    RELAY_PROTOCOL_VIOLATION,
    EVENT_TOO_LARGE
  };
  
  void HandleInvalidEvent(const std::string& event_json,
                         const std::string& relay_url) {
    // Log for debugging
    LOG(WARNING) << "Invalid event from " << relay_url 
                 << ": " << event_json.substr(0, 100);
    
    // Try to parse partially
    auto partial_event = TryPartialParse(event_json);
    if (partial_event) {
      // Check if it's a newer NIP we don't support
      if (IsUnsupportedNIP(partial_event->kind)) {
        HandleUnsupportedNIP(partial_event->kind, relay_url);
        return;
      }
    }
    
    // Track relay behavior
    relay_monitor_->RecordProtocolViolation(relay_url);
    
    // If relay is consistently sending bad data, disconnect
    if (relay_monitor_->GetViolationCount(relay_url) > 
        config_.max_protocol_violations) {
      DisconnectFromRelay(relay_url, "Too many protocol violations");
    }
  }
  
  void HandleUnsupportedNIP(int nip_number, const std::string& source) {
    // Check if update available
    update_checker_->CheckForUpdate(
        base::BindOnce(&ProtocolErrorRecovery::OnUpdateCheckComplete,
                      weak_factory_.GetWeakPtr(),
                      nip_number));
    
    // Log for telemetry
    UMA_HISTOGRAM_SPARSE_SLOWLY("Nostr.UnsupportedNIP", nip_number);
    
    // Notify user if it's becoming common
    unsupported_nip_counter_[nip_number]++;
    if (unsupported_nip_counter_[nip_number] > 10) {
      NotifyUserAboutUnsupportedFeature(nip_number);
    }
  }
};
```

### 6. Offline Mode and Graceful Degradation

```cpp
class OfflineModeManager {
 public:
  void EnterOfflineMode(OfflineReason reason) {
    LOG(INFO) << "Entering offline mode: " << ToString(reason);
    
    is_offline_ = true;
    offline_start_time_ = base::Time::Now();
    
    // Switch to local-only operation
    relay_manager_->DisconnectAll();
    
    // Enable offline features
    EnableOfflineFeatures();
    
    // Queue outgoing events
    event_queue_->StartQueueing();
    
    // Notify UI
    NotifyOfflineStateChange(true);
    
    // Start monitoring for connectivity
    connectivity_monitor_->Start(
        base::BindRepeating(&OfflineModeManager::OnConnectivityChanged,
                          weak_factory_.GetWeakPtr()));
  }
  
  void EnableOfflineFeatures() {
    // Use local relay exclusively
    local_relay_->SetExclusiveMode(true);
    
    // Enable aggressive caching
    cache_manager_->SetOfflineMode(true);
    
    // Preload essential data
    PreloadOfflineData();
    
    // Show offline indicator
    ui_controller_->ShowOfflineIndicator();
  }
  
  void OnConnectivityRestored() {
    LOG(INFO) << "Connectivity restored after " 
              << (base::Time::Now() - offline_start_time_).InMinutes() 
              << " minutes";
    
    is_offline_ = false;
    
    // Gradually restore online features
    RestoreOnlineServices();
    
    // Sync queued events
    SyncQueuedEvents();
    
    // Update UI
    NotifyOfflineStateChange(false);
  }
  
 private:
  void RestoreOnlineServices() {
    // Phase 1: Test connectivity with primary relays
    TestRelayConnectivity(
        base::BindOnce(&OfflineModeManager::OnConnectivityTestComplete,
                      weak_factory_.GetWeakPtr()));
  }
  
  void SyncQueuedEvents() {
    auto queued_events = event_queue_->GetQueuedEvents();
    LOG(INFO) << "Syncing " << queued_events.size() << " queued events";
    
    // Send in batches to avoid overwhelming relays
    const int batch_size = 50;
    for (size_t i = 0; i < queued_events.size(); i += batch_size) {
      auto batch_end = std::min(i + batch_size, queued_events.size());
      std::vector<NostrEvent> batch(
          queued_events.begin() + i,
          queued_events.begin() + batch_end);
      
      relay_manager_->BroadcastEvents(
          batch,
          base::BindOnce(&OfflineModeManager::OnBatchSyncComplete,
                        weak_factory_.GetWeakPtr(),
                        i / batch_size));
      
      // Delay between batches
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::DoNothing(),
          base::Milliseconds(500));
    }
  }
};
```

### 7. User-Facing Error Messages

```cpp
class ErrorMessageProvider {
 public:
  struct ErrorMessage {
    std::string title;
    std::string description;
    std::string action_button_text;
    std::string help_link;
    ErrorIcon icon;
  };
  
  ErrorMessage GetUserMessage(const NostrError& error) {
    // Translate technical errors to user-friendly messages
    switch (error.GetCategory()) {
      case ErrorCategory::NETWORK:
        return GetNetworkErrorMessage(error);
        
      case ErrorCategory::CRYPTOGRAPHIC:
        return GetCryptoErrorMessage(error);
        
      case ErrorCategory::STORAGE:
        return GetStorageErrorMessage(error);
        
      default:
        return GetGenericErrorMessage(error);
    }
  }
  
 private:
  ErrorMessage GetNetworkErrorMessage(const NostrError& error) {
    auto network_error = static_cast<const NetworkError&>(error);
    
    switch (network_error.GetType()) {
      case NetworkErrorType::RELAY_UNAVAILABLE:
        return {
          .title = "Connection Problem",
          .description = "Unable to connect to Nostr relay. "
                        "Your messages will be queued and sent when "
                        "the connection is restored.",
          .action_button_text = "Retry",
          .icon = ErrorIcon::WARNING
        };
        
      case NetworkErrorType::RATE_LIMITED:
        return {
          .title = "Slow Down",
          .description = "You're sending messages too quickly. "
                        "Please wait a moment before trying again.",
          .action_button_text = "OK",
          .icon = ErrorIcon::INFO
        };
        
      default:
        return {
          .title = "Network Error",
          .description = "There was a problem with the network connection. "
                        "Please check your internet connection.",
          .action_button_text = "Retry",
          .help_link = "tungsten://help/network-errors",
          .icon = ErrorIcon::ERROR
        };
    }
  }
};
```

### 8. Automatic Recovery Orchestration

```cpp
class RecoveryOrchestrator {
 public:
  void HandleError(std::unique_ptr<NostrError> error) {
    // Add to error history
    error_history_.push_back(error->Clone());
    
    // Get recovery strategy
    auto strategy = error->GetRecoveryStrategy();
    
    // Check if we're in a recovery loop
    if (IsRecoveryLoop(error.get())) {
      LOG(WARNING) << "Recovery loop detected for " << error->GetMessage();
      strategy = GetFallbackStrategy(error.get());
    }
    
    // Execute recovery
    ExecuteRecoveryStrategy(std::move(error), strategy);
  }
  
 private:
  void ExecuteRecoveryStrategy(std::unique_ptr<NostrError> error,
                              const RecoveryStrategy& strategy) {
    active_recoveries_[error->GetId()] = {
      .error = std::move(error),
      .strategy = strategy,
      .start_time = base::Time::Now(),
      .attempt_count = 0
    };
    
    switch (strategy.action) {
      case RecoveryAction::RETRY_WITH_BACKOFF:
        ScheduleRetry(error->GetId(), strategy);
        break;
        
      case RecoveryAction::USE_ALTERNATIVE:
        UseAlternative(error->GetId(), strategy);
        break;
        
      case RecoveryAction::RESTORE_FROM_BACKUP:
        RestoreFromBackup(error->GetId(), strategy);
        break;
        
      case RecoveryAction::PROMPT_USER:
        PromptUser(error->GetId(), strategy);
        break;
        
      case RecoveryAction::OPERATE_DEGRADED:
        EnableDegradedMode(error->GetId(), strategy);
        break;
        
      default:
        LOG(WARNING) << "Unknown recovery action: " 
                     << static_cast<int>(strategy.action);
    }
  }
  
  bool IsRecoveryLoop(const NostrError* error) {
    // Check if we've seen similar errors recently
    int similar_count = 0;
    auto cutoff_time = base::Time::Now() - base::Minutes(5);
    
    for (auto it = error_history_.rbegin(); 
         it != error_history_.rend(); ++it) {
      if ((*it)->GetTimestamp() < cutoff_time) break;
      
      if ((*it)->IsSimilarTo(*error)) {
        similar_count++;
      }
    }
    
    return similar_count > config_.max_similar_errors;
  }
};
```

### 9. Crash Recovery

```cpp
class CrashRecoveryManager {
 public:
  void OnBrowserStart() {
    // Check for previous crash
    if (DetectPreviousCrash()) {
      HandleCrashRecovery();
    }
    
    // Set up crash detection for this session
    SetupCrashDetection();
  }
  
 private:
  void HandleCrashRecovery() {
    LOG(WARNING) << "Recovering from previous crash";
    
    // Step 1: Verify data integrity
    RunIntegrityChecks();
    
    // Step 2: Restore unsaved data
    RestoreSessionData();
    
    // Step 3: Recover incomplete operations
    RecoverIncompleteOperations();
    
    // Step 4: Notify user if necessary
    if (HasDataLoss()) {
      NotifyUserOfRecovery();
    }
  }
  
  void RunIntegrityChecks() {
    // Check key storage
    key_integrity_checker_->Verify(
        base::BindOnce(&CrashRecoveryManager::OnKeyIntegrityChecked,
                      weak_factory_.GetWeakPtr()));
    
    // Check database
    database_integrity_checker_->Verify(
        base::BindOnce(&CrashRecoveryManager::OnDatabaseIntegrityChecked,
                      weak_factory_.GetWeakPtr()));
    
    // Check cached files
    cache_integrity_checker_->Verify(
        base::BindOnce(&CrashRecoveryManager::OnCacheIntegrityChecked,
                      weak_factory_.GetWeakPtr()));
  }
  
  void RecoverIncompleteOperations() {
    // Check for pending broadcasts
    auto pending_events = event_storage_->GetPendingBroadcasts();
    if (!pending_events.empty()) {
      LOG(INFO) << "Found " << pending_events.size() 
                << " events pending broadcast";
      relay_manager_->BroadcastEvents(pending_events);
    }
    
    // Check for incomplete downloads
    auto incomplete_downloads = download_manager_->GetIncomplete();
    for (const auto& download : incomplete_downloads) {
      download_manager_->Resume(download.id);
    }
  }
};
```

### 10. Error Analytics and Learning

```cpp
class ErrorAnalytics {
 public:
  void AnalyzeErrorPatterns() {
    // Identify common error patterns
    auto patterns = pattern_analyzer_->Analyze(error_history_);
    
    for (const auto& pattern : patterns) {
      if (pattern.frequency > threshold_) {
        // Adjust recovery strategies based on patterns
        strategy_optimizer_->Optimize(pattern);
        
        // Report to telemetry
        ReportErrorPattern(pattern);
      }
    }
  }
  
  void LearnFromRecoverySuccess(const std::string& error_id,
                               const RecoveryStrategy& strategy) {
    // Record successful recovery
    success_database_->Record(error_id, strategy);
    
    // Update strategy weights
    strategy_ml_model_->UpdateWeights(error_id, strategy, 1.0);
    
    // Share learning with other instances (privacy-preserving)
    if (telemetry_enabled_) {
      ShareRecoverySuccess(error_id, strategy);
    }
  }
};
```