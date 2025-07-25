# Telemetry and Analytics Approach
## dryft browser - Privacy-Focused Metrics and Analytics

### 1. Privacy-First Telemetry Philosophy

```cpp
namespace dryft {
namespace telemetry {

// Core principles for Tungsten telemetry
class TelemetryPolicy {
 public:
  static constexpr bool kDefaultOptIn = false;  // Opt-in by default
  static constexpr bool kRequireExplicitConsent = true;
  static constexpr bool kAllowPersonallyIdentifiableInfo = false;
  static constexpr bool kUseLocalAggregation = true;
  static constexpr bool kUseDifferentialPrivacy = true;
  
  // No telemetry for Nostr-specific data that could compromise privacy
  static constexpr bool kCollectNostrPublicKeys = false;
  static constexpr bool kCollectRelayURLs = false;
  static constexpr bool kCollectEventContent = false;
  static constexpr bool kCollectNIP05Identifiers = false;
};

}  // namespace telemetry
}  // namespace dryft
```

### 2. Telemetry Architecture

```cpp
// Telemetry collection system with privacy safeguards
class TungstenTelemetryService {
 public:
  struct TelemetryConfig {
    bool enabled = false;
    bool crash_reporting_enabled = false;
    bool usage_stats_enabled = false;
    bool performance_monitoring_enabled = false;
    int upload_interval_minutes = 1440;  // Daily
    std::string upload_url = "https://telemetry.tungsten-browser.org/v1/";
  };
  
  // Initialize with user consent
  void Initialize(bool user_consent_given) {
    if (!user_consent_given) {
      LOG(INFO) << "Telemetry disabled - no user consent";
      return;
    }
    
    config_.enabled = true;
    InitializeCollectors();
    StartLocalAggregation();
  }
  
 private:
  // Local aggregation before upload
  class LocalAggregator {
   public:
    void AddMetric(const std::string& metric_name,
                  int64_t value) {
      // Apply differential privacy
      int64_t noisy_value = AddLaplaceNoise(value);
      
      // Aggregate locally
      local_metrics_[metric_name].push_back(noisy_value);
      
      // Compute statistics
      if (local_metrics_[metric_name].size() >= kMinSampleSize) {
        AggregateAndClear(metric_name);
      }
    }
    
   private:
    static constexpr int kMinSampleSize = 100;
    static constexpr double kPrivacyBudget = 1.0;
    
    int64_t AddLaplaceNoise(int64_t value) {
      // Add Laplace noise for differential privacy
      std::random_device rd;
      std::mt19937 gen(rd());
      std::exponential_distribution<> exp_dist(kPrivacyBudget);
      
      double noise = exp_dist(gen) - exp_dist(gen);
      return value + static_cast<int64_t>(noise);
    }
  };
};
```

### 3. Metrics Collection Categories

```cpp
// General browser metrics (non-Nostr specific)
namespace browser_metrics {

// Performance metrics
void RecordStartupTime(base::TimeDelta startup_time) {
  UMA_HISTOGRAM_TIMES("Tungsten.Startup.ColdStart", startup_time);
}

void RecordMemoryUsage() {
  base::ProcessMetrics* metrics = 
      base::ProcessMetrics::CreateCurrentProcessMetrics();
  
  UMA_HISTOGRAM_MEMORY_MB("Tungsten.Memory.Browser",
                         metrics->GetWorkingSetSize() / 1024 / 1024);
}

// Stability metrics
void RecordCrash(CrashType type) {
  UMA_HISTOGRAM_ENUMERATION("Tungsten.Stability.CrashType",
                           type,
                           CrashType::kMaxValue);
}

// Feature usage (anonymous)
void RecordFeatureUsage(Feature feature) {
  UMA_HISTOGRAM_ENUMERATION("Tungsten.Features.Used",
                           feature,
                           Feature::kMaxValue);
}

}  // namespace browser_metrics

// Nostr metrics (privacy-preserving)
namespace nostr_metrics {

// Only collect aggregate, non-identifying metrics
void RecordNostrFeatureEnabled(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("Tungsten.Nostr.Enabled", enabled);
}

void RecordRelayConnectionCount(int count) {
  // Bucket into ranges to prevent fingerprinting
  int bucketed_count = 0;
  if (count == 0) bucketed_count = 0;
  else if (count <= 3) bucketed_count = 3;
  else if (count <= 5) bucketed_count = 5;
  else if (count <= 10) bucketed_count = 10;
  else bucketed_count = 11;  // 11+
  
  UMA_HISTOGRAM_SPARSE_SLOWLY("Tungsten.Nostr.RelayCount", 
                             bucketed_count);
}

void RecordNIP07Usage(NIP07Method method) {
  // Only track method types, not content
  UMA_HISTOGRAM_ENUMERATION("Tungsten.Nostr.NIP07.MethodUsed",
                           method,
                           NIP07Method::kMaxValue);
}

void RecordLocalServiceUsage() {
  UMA_HISTOGRAM_BOOLEAN("Tungsten.Nostr.LocalRelay.Enabled",
                       prefs::GetBoolean(prefs::kNostrLocalRelayEnabled));
  
  UMA_HISTOGRAM_BOOLEAN("Tungsten.Nostr.Blossom.Enabled",
                       prefs::GetBoolean(prefs::kBlossomServerEnabled));
}

// Performance without identifying info
void RecordEventProcessingTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Tungsten.Nostr.EventProcessing.Time", time);
}

void RecordBandwidthUsage(size_t bytes, Direction direction) {
  // Log in buckets to prevent precise tracking
  size_t kb = bytes / 1024;
  if (direction == Direction::SENT) {
    UMA_HISTOGRAM_COUNTS_1M("Tungsten.Nostr.Bandwidth.Sent.KB", kb);
  } else {
    UMA_HISTOGRAM_COUNTS_1M("Tungsten.Nostr.Bandwidth.Received.KB", kb);
  }
}

}  // namespace nostr_metrics
```

### 4. User Consent UI

```cpp
// Clear consent dialog for telemetry
class TelemetryConsentDialog {
 public:
  void Show() {
    // Create consent dialog with clear explanation
    auto dialog = std::make_unique<views::DialogDelegate>();
    
    dialog->SetTitle("Help Improve Tungsten");
    dialog->SetMessage(
        "Tungsten can collect anonymous usage statistics to help "
        "improve the browser. This data:\n\n"
        "• Never includes personal information\n"
        "• Never includes your Nostr keys or identity\n"
        "• Never includes message content or relay URLs\n"
        "• Is processed using differential privacy\n"
        "• Can be disabled at any time in settings\n\n"
        "Would you like to help improve Tungsten?");
    
    dialog->SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
    dialog->SetButtonLabel(ui::DIALOG_BUTTON_OK, "Yes, help improve");
    dialog->SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, "No thanks");
    
    dialog->SetAcceptCallback(
        base::BindOnce(&TelemetryConsentDialog::OnAccept,
                      weak_factory_.GetWeakPtr()));
    
    dialog->SetCancelCallback(
        base::BindOnce(&TelemetryConsentDialog::OnDecline,
                      weak_factory_.GetWeakPtr()));
    
    dialog->Show();
  }
  
 private:
  void OnAccept() {
    prefs_->SetBoolean(prefs::kTelemetryEnabled, true);
    prefs_->SetTime(prefs::kTelemetryConsentTime, base::Time::Now());
    
    // Initialize telemetry
    TelemetryService::GetInstance()->Initialize(true);
  }
  
  void OnDecline() {
    prefs_->SetBoolean(prefs::kTelemetryEnabled, false);
    prefs_->SetTime(prefs::kTelemetryConsentTime, base::Time::Now());
  }
};
```

### 5. Local Analytics Dashboard

```cpp
// Local analytics for users to see their own usage
class LocalAnalyticsDashboard : public content::WebUIController {
 public:
  explicit LocalAnalyticsDashboard(content::WebUI* web_ui)
      : WebUIController(web_ui) {
    // Set up the tungsten://analytics page
    content::WebUIDataSource* source =
        content::WebUIDataSource::Create("analytics");
    
    source->AddResourcePath("analytics.html", IDR_ANALYTICS_HTML);
    source->AddResourcePath("analytics.js", IDR_ANALYTICS_JS);
    source->AddResourcePath("analytics.css", IDR_ANALYTICS_CSS);
    
    web_ui->AddMessageHandler(
        std::make_unique<AnalyticsHandler>());
  }
};

class AnalyticsHandler : public content::WebUIMessageHandler {
 public:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getLocalStats",
        base::BindRepeating(&AnalyticsHandler::GetLocalStats,
                          base::Unretained(this)));
  }
  
 private:
  void GetLocalStats(const base::ListValue* args) {
    base::DictionaryValue stats;
    
    // Browser stats
    stats.SetInteger("totalSessions", 
                    local_db_->GetSessionCount());
    stats.SetDouble("averageSessionMinutes",
                   local_db_->GetAverageSessionLength().InMinutes());
    
    // Nostr stats (local only)
    stats.SetInteger("eventsSignedToday",
                    local_db_->GetDailyEventCount());
    stats.SetInteger("relaysConnected",
                    relay_manager_->GetConnectedCount());
    stats.SetInteger("localEventsCached",
                    local_relay_->GetCachedEventCount());
    
    // Performance stats
    stats.SetDouble("averageStartupMs",
                   local_db_->GetAverageStartupTime().InMilliseconds());
    stats.SetInteger("currentMemoryMB",
                    GetCurrentMemoryUsage() / 1024 / 1024);
    
    AllowJavascript();
    CallJavascriptFunction("analytics.updateStats", stats);
  }
};
```

### 6. Crash Reporting

```cpp
// Privacy-preserving crash reporting
class TungstenCrashReporter {
 public:
  void ConfigureCrashReporting() {
    if (!prefs_->GetBoolean(prefs::kCrashReportingEnabled)) {
      return;
    }
    
    crash_reporter::InitializeCrashpad(true, "Tungsten");
    
    // Set crash keys that don't identify users
    static crash_reporter::CrashKeyString<32> version_key("version");
    version_key.Set(version_info::GetVersionNumber());
    
    static crash_reporter::CrashKeyString<16> channel_key("channel");
    channel_key.Set(chrome::GetChannelName());
    
    // Nostr feature flags (no identifying info)
    static crash_reporter::CrashKeyString<8> nostr_enabled("nostr");
    nostr_enabled.Set(IsNostrEnabled() ? "1" : "0");
    
    static crash_reporter::CrashKeyString<8> local_relay("local_relay");
    local_relay.Set(IsLocalRelayEnabled() ? "1" : "0");
  }
  
  // Strip sensitive data from crash dumps
  void OnCrashDumpCreated(const base::FilePath& crash_dump) {
    // Remove any potential Nostr keys or sensitive data
    SanitizeCrashDump(crash_dump);
  }
  
 private:
  void SanitizeCrashDump(const base::FilePath& dump_path) {
    // Patterns to remove from memory dumps
    std::vector<std::regex> sensitive_patterns = {
      std::regex("nsec1[a-z0-9]{58}"),  // Private keys
      std::regex("\"privkey\"\\s*:\\s*\"[^\"]+\""),  // JSON private keys
      std::regex("wss://[^\\s]+"),  // Relay URLs
    };
    
    // Process dump file to remove sensitive patterns
    // Implementation details omitted for brevity
  }
};
```

### 7. A/B Testing Telemetry

```cpp
// A/B testing framework with privacy
class ExperimentTelemetry {
 public:
  struct ExperimentConfig {
    std::string name;
    std::vector<std::string> groups;
    double sample_rate = 1.0;
    bool requires_consent = true;
  };
  
  void RecordExperimentExposure(const std::string& experiment_name,
                               const std::string& group) {
    if (!IsExperimentEnabled(experiment_name)) {
      return;
    }
    
    // Hash experiment + group to prevent correlation
    uint64_t hash = base::PersistentHash(experiment_name + "|" + group);
    
    UMA_HISTOGRAM_SPARSE_SLOWLY("Tungsten.Experiments.Exposure",
                               hash % 1000000);  // Truncate for privacy
  }
  
  void RecordExperimentMetric(const std::string& experiment_name,
                             const std::string& metric_name,
                             double value) {
    // Add noise for privacy
    double noisy_value = AddGaussianNoise(value, kNoiseScale);
    
    // Record with experiment context
    base::StringPiece histogram_name = base::StringPrintf(
        "Tungsten.Experiments.%s.%s",
        SanitizeExperimentName(experiment_name).c_str(),
        SanitizeMetricName(metric_name).c_str());
    
    base::UmaHistogramCustomCounts(
        histogram_name, noisy_value, 1, 1000000, 50);
  }
  
 private:
  static constexpr double kNoiseScale = 0.1;
  
  double AddGaussianNoise(double value, double scale) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0, scale * value);
    return value + noise(gen);
  }
};
```

### 8. Performance Monitoring

```cpp
// Lightweight performance monitoring
class PerformanceMonitor {
 public:
  void StartMonitoring() {
    // Sample performance periodically
    timer_.Start(FROM_HERE,
                base::Minutes(5),
                this,
                &PerformanceMonitor::CollectSample);
  }
  
 private:
  void CollectSample() {
    PerformanceSample sample;
    
    // CPU usage (bucketed)
    sample.cpu_usage_percent = GetBucketedCPUUsage();
    
    // Memory usage (bucketed)  
    sample.memory_usage_mb = GetBucketedMemoryUsage();
    
    // Renderer process count
    sample.renderer_count = 
        content::RenderProcessHost::GetCurrentRenderProcessCountForTesting();
    
    // Nostr connection health (anonymous)
    sample.relay_connection_success_rate = 
        relay_manager_->GetConnectionSuccessRate();
    
    // Store locally
    local_storage_->AddPerformanceSample(sample);
    
    // Upload aggregated data daily
    MaybeUploadAggregatedData();
  }
  
  int GetBucketedCPUUsage() {
    double cpu = process_metrics_->GetPlatformIndependentCPUUsage();
    
    // Bucket into ranges
    if (cpu < 5) return 5;
    if (cpu < 10) return 10;
    if (cpu < 25) return 25;
    if (cpu < 50) return 50;
    return 100;  // 50+
  }
  
  int GetBucketedMemoryUsage() {
    size_t memory_mb = process_metrics_->GetWorkingSetSize() / 1024 / 1024;
    
    // Bucket into ranges
    if (memory_mb < 256) return 256;
    if (memory_mb < 512) return 512;
    if (memory_mb < 1024) return 1024;
    if (memory_mb < 2048) return 2048;
    return 4096;  // 2GB+
  }
};
```

### 9. Custom Event Tracking

```cpp
// User behavior analytics (privacy-preserving)
class BehaviorAnalytics {
 public:
  // Track feature discovery
  void RecordFeatureDiscovery(Feature feature) {
    // Only track that feature was discovered, not how
    UMA_HISTOGRAM_ENUMERATION("Tungsten.Discovery.Feature",
                             feature,
                             Feature::kMaxValue);
  }
  
  // Track user journeys (anonymous)
  void RecordUserJourney(JourneyType journey, bool completed) {
    UMA_HISTOGRAM_BOOLEAN(
        base::StringPrintf("Tungsten.Journey.%s.Completed",
                          JourneyToString(journey)),
        completed);
  }
  
  // Session analytics
  void RecordSessionMetrics() {
    SessionMetrics metrics;
    metrics.duration = base::Time::Now() - session_start_time_;
    metrics.page_views = page_view_count_;
    metrics.nostr_interactions = nostr_interaction_count_;
    
    // Only record bucketed values
    UMA_HISTOGRAM_CUSTOM_TIMES("Tungsten.Session.Duration",
                              metrics.duration,
                              base::Seconds(1),
                              base::Hours(24),
                              50);
    
    UMA_HISTOGRAM_COUNTS_100("Tungsten.Session.PageViews",
                            std::min(metrics.page_views, 100));
  }
};
```

### 10. Error Tracking

```cpp
// Error tracking without exposing sensitive data
class ErrorTelemetry {
 public:
  void RecordError(ErrorType type, ErrorCode code) {
    // Generic error tracking
    UMA_HISTOGRAM_ENUMERATION("Tungsten.Errors.Type",
                             type,
                             ErrorType::kMaxValue);
    
    // Specific error codes (anonymized)
    UMA_HISTOGRAM_SPARSE_SLOWLY("Tungsten.Errors.Code",
                               static_cast<int>(code));
  }
  
  void RecordNostrError(NostrErrorType type) {
    // Only track error types, never content
    UMA_HISTOGRAM_ENUMERATION("Tungsten.Nostr.Errors",
                             type,
                             NostrErrorType::kMaxValue);
  }
  
  // Rate limiting to prevent error spam
  void RecordRateLimitedError(const std::string& error_key) {
    auto& count = error_counts_[error_key];
    count++;
    
    // Only record periodically to prevent spam
    if (count == 1 || count == 10 || count == 100 || count % 1000 == 0) {
      UMA_HISTOGRAM_COUNTS_1M("Tungsten.Errors.Frequency",
                             count);
    }
  }
};
```

### 11. Data Retention and Deletion

```cpp
class TelemetryDataManager {
 public:
  struct RetentionPolicy {
    base::TimeDelta local_retention = base::Days(90);
    base::TimeDelta server_retention = base::Days(180);
    bool auto_delete_on_uninstall = true;
  };
  
  void EnforceRetentionPolicy() {
    // Delete old local data
    base::Time cutoff = base::Time::Now() - policy_.local_retention;
    local_db_->DeleteDataBefore(cutoff);
    
    // Request server deletion for old data
    if (prefs_->GetBoolean(prefs::kTelemetryEnabled)) {
      RequestServerDataDeletion(cutoff);
    }
  }
  
  void DeleteAllTelemetryData() {
    // Local deletion
    local_db_->DeleteAllData();
    
    // Clear preferences
    prefs_->ClearPref(prefs::kTelemetryClientId);
    prefs_->SetBoolean(prefs::kTelemetryEnabled, false);
    
    // Request server deletion
    RequestCompleteDataDeletion();
  }
  
 private:
  void RequestCompleteDataDeletion() {
    auto request = std::make_unique<network::SimpleURLLoader>(
        network::SimpleURLLoader::Create(
            CreateDeletionRequest(),
            TRAFFIC_ANNOTATION));
    
    request->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&TelemetryDataManager::OnDeletionComplete,
                      weak_factory_.GetWeakPtr()));
  }
};
```

### 12. Telemetry Settings UI

```typescript
// Settings page for telemetry control
interface TelemetrySettings {
  enabled: boolean;
  crashReportingEnabled: boolean;
  usageStatsEnabled: boolean;
  performanceMonitoringEnabled: boolean;
  experimentParticipation: boolean;
}

class TelemetrySettingsPage extends React.Component {
  render() {
    return (
      <div className="settings-section">
        <h2>Privacy & Analytics</h2>
        
        <div className="setting-item">
          <label>
            <input
              type="checkbox"
              checked={this.props.settings.enabled}
              onChange={e => this.updateSetting('enabled', e.target.checked)}
            />
            Help improve Tungsten with anonymous usage data
          </label>
          <p className="description">
            Collects anonymous statistics about feature usage and performance.
            Never includes personal information or Nostr identity.
          </p>
        </div>
        
        <div className="setting-item">
          <label>
            <input
              type="checkbox"
              checked={this.props.settings.crashReportingEnabled}
              onChange={e => this.updateSetting('crashReportingEnabled', e.target.checked)}
            />
            Send crash reports
          </label>
          <p className="description">
            Helps us fix crashes and improve stability. Crash reports are 
            sanitized to remove any Nostr keys or personal data.
          </p>
        </div>
        
        <div className="advanced-settings">
          <h3>Advanced</h3>
          
          <div className="setting-item">
            <button onClick={this.viewLocalAnalytics}>
              View Your Local Analytics
            </button>
          </div>
          
          <div className="setting-item">
            <button onClick={this.downloadMyData}>
              Download My Data
            </button>
          </div>
          
          <div className="setting-item danger">
            <button onClick={this.deleteAllData}>
              Delete All Analytics Data
            </button>
          </div>
        </div>
      </div>
    );
  }
}
```

### 13. Server-Side Analytics

```python
# Analytics server implementation
from flask import Flask, request, jsonify
import hashlib
from datetime import datetime, timedelta

app = Flask(__name__)

class PrivacyPreservingAnalytics:
    def __init__(self):
        self.noise_generator = DifferentialPrivacy(epsilon=1.0)
        
    @app.route('/v1/telemetry', methods=['POST'])
    def receive_telemetry(self):
        data = request.json
        
        # Validate data doesn't contain PII
        if self.contains_pii(data):
            return jsonify({'error': 'PII detected'}), 400
        
        # Add server-side noise
        noisy_data = self.add_noise(data)
        
        # Store in time-series database
        self.store_metrics(noisy_data)
        
        return jsonify({'status': 'accepted'}), 200
    
    def contains_pii(self, data):
        """Check for potential PII patterns"""
        pii_patterns = [
            r'nsec1[a-z0-9]{58}',  # Nostr private keys
            r'npub1[a-z0-9]{58}',  # Nostr public keys  
            r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b',  # Email
            r'wss?://[^\s]+',  # Relay URLs
        ]
        
        data_str = json.dumps(data)
        for pattern in pii_patterns:
            if re.search(pattern, data_str):
                return True
        return False
    
    def aggregate_metrics(self, time_window):
        """Aggregate metrics with k-anonymity"""
        metrics = self.db.query(f"""
            SELECT 
                metric_name,
                AVG(value) as avg_value,
                COUNT(*) as sample_count,
                PERCENTILE_CONT(0.5) as median_value,
                PERCENTILE_CONT(0.95) as p95_value
            FROM metrics
            WHERE timestamp > NOW() - INTERVAL '{time_window}'
            GROUP BY metric_name
            HAVING COUNT(*) >= 100  -- k-anonymity threshold
        """)
        
        return metrics

class DifferentialPrivacy:
    def __init__(self, epsilon=1.0):
        self.epsilon = epsilon
    
    def add_laplace_noise(self, value, sensitivity=1.0):
        """Add Laplace noise for differential privacy"""
        scale = sensitivity / self.epsilon
        noise = np.random.laplace(0, scale)
        return value + noise
```

This telemetry and analytics approach ensures:
- User privacy is paramount with opt-in consent
- No personally identifiable information is collected
- Differential privacy techniques protect individual users
- Local analytics give users visibility into their own data
- Clear user controls and data deletion options
- Nostr-specific data is never transmitted
- Crash reports are sanitized
- Server-side validation prevents PII leakage