# Deployment and Update Strategies
## dryft browser - Release, Distribution, and Update Management

### 1. Build and Release Pipeline

```yaml
# .github/workflows/release.yml
name: dryft Release Pipeline

on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
        with:
          repository: thorium-browser/thorium
          submodules: recursive
      
      - name: Setup Build Environment
        run: |
          # Install Visual Studio Build Tools
          choco install visualstudio2022buildtools
          choco install windows-sdk-10
          
      - name: Apply dryft Patches
        run: |
          git apply dryft/patches/*.patch
          python dryft/scripts/integrate_nostr.py
          
      - name: Build dryft
        run: |
          cd src
          gn gen out/Release --args="
            is_official_build=true
            is_debug=false
            enable_nacl=false
            enable_nostr=true
            enable_local_relay=true
            enable_blossom_server=true
          "
          autoninja -C out/Release chrome
          
      - name: Package Installer
        run: |
          cd src/out/Release
          makensis.exe /DVERSION=${{ github.ref_name }} 
            ../../../dryft/installer/windows/dryft.nsi
          
      - name: Sign Binary
        env:
          CERT_PASSWORD: ${{ secrets.WINDOWS_CERT_PASSWORD }}
        run: |
          signtool sign /f certificate.pfx /p $env:CERT_PASSWORD 
            /t http://timestamp.digicert.com 
            dryft-setup-${{ github.ref_name }}.exe

  build-macos:
    runs-on: macos-latest
    steps:
      - name: Build macOS App
        run: |
          cd src
          gn gen out/Release --args="
            is_official_build=true
            enable_nostr=true
            mac_deployment_target=\"10.15\"
          "
          autoninja -C out/Release chrome
          
      - name: Create DMG
        run: |
          create-dmg \
            --volname "dryft browser" \
            --volicon "dryft/assets/dryft.icns" \
            --window-pos 200 120 \
            --window-size 800 400 \
            --icon-size 100 \
            --icon "dryft.app" 200 190 \
            --hide-extension "dryft.app" \
            --app-drop-link 600 185 \
            "dryft-${{ github.ref_name }}.dmg" \
            "src/out/Release/dryft.app"
            
      - name: Notarize App
        env:
          APPLE_ID: ${{ secrets.APPLE_ID }}
          APPLE_PASSWORD: ${{ secrets.APPLE_PASSWORD }}
        run: |
          xcrun altool --notarize-app \
            --primary-bundle-id "com.dryft.browser" \
            --username "$APPLE_ID" \
            --password "$APPLE_PASSWORD" \
            --file "dryft-${{ github.ref_name }}.dmg"

  build-linux:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        arch: [x64, arm64]
    steps:
      - name: Build Linux Packages
        run: |
          cd src
          gn gen out/Release --args="
            is_official_build=true
            enable_nostr=true
            target_cpu=\"${{ matrix.arch }}\"
          "
          autoninja -C out/Release chrome
          
      - name: Create DEB Package
        run: |
          cd dryft/packaging/debian
          ./make_deb.sh ${{ github.ref_name }} ${{ matrix.arch }}
          
      - name: Create RPM Package  
        run: |
          cd dryft/packaging/rpm
          rpmbuild -bb \
            --define "_version ${{ github.ref_name }}" \
            --define "_arch ${{ matrix.arch }}" \
            dryft.spec
            
      - name: Create AppImage
        run: |
          cd dryft/packaging/appimage
          ./make_appimage.sh ${{ github.ref_name }} ${{ matrix.arch }}
```

### 2. Update Mechanism Architecture

```cpp
// chrome/browser/dryft/update/update_manager.h
class TungstenUpdateManager {
 public:
  enum UpdateChannel {
    STABLE,
    BETA,
    DEV,
    CANARY
  };
  
  struct UpdateInfo {
    base::Version version;
    std::string release_notes_url;
    std::vector<std::string> download_urls;
    std::string sha256_hash;
    int64_t size_bytes;
    base::Time release_date;
    bool is_critical;
    std::vector<std::string> nostr_features;
  };
  
  // Check for updates
  void CheckForUpdates(UpdateCheckCallback callback);
  
  // Download and stage update
  void DownloadUpdate(const UpdateInfo& info,
                     ProgressCallback progress,
                     CompletionCallback completion);
  
  // Apply staged update on next restart
  void ScheduleUpdate();
  
 private:
  // Delta updates for bandwidth efficiency
  bool CanUseDeltaUpdate(const base::Version& from,
                        const base::Version& to);
  
  // Verify update integrity
  bool VerifyUpdatePackage(const base::FilePath& package_path,
                          const std::string& expected_hash);
  
  UpdateChannel channel_;
  std::unique_ptr<UpdateDownloader> downloader_;
  std::unique_ptr<UpdateVerifier> verifier_;
};

// Update check implementation
void TungstenUpdateManager::CheckForUpdates(
    UpdateCheckCallback callback) {
  // Build update check request
  auto request = std::make_unique<UpdateCheckRequest>();
  request->current_version = GetCurrentVersion();
  request->channel = channel_;
  request->os = GetOSInfo();
  request->arch = GetArchitecture();
  request->nostr_enabled = IsNostrEnabled();
  
  // Send request to update server
  update_client_->SendRequest(
      std::move(request),
      base::BindOnce(&TungstenUpdateManager::OnUpdateCheckComplete,
                    weak_factory_.GetWeakPtr(),
                    std::move(callback)));
}

// Staged update system
class StagedUpdateManager {
 public:
  void StageUpdate(const base::FilePath& update_package) {
    // Extract to staging directory
    base::FilePath staging_dir = GetStagingDirectory();
    if (!ExtractUpdate(update_package, staging_dir)) {
      LOG(ERROR) << "Failed to extract update";
      return;
    }
    
    // Verify extracted files
    if (!VerifyExtractedUpdate(staging_dir)) {
      LOG(ERROR) << "Update verification failed";
      CleanupStagingDirectory();
      return;
    }
    
    // Mark as ready
    prefs_->SetFilePath(prefs::kStagedUpdatePath, staging_dir);
    prefs_->SetBoolean(prefs::kUpdateReadyToInstall, true);
  }
  
  void ApplyStagedUpdate() {
    // This runs early in startup before most initialization
    base::FilePath staged_path = 
        prefs_->GetFilePath(prefs::kStagedUpdatePath);
    
    if (staged_path.empty() || !base::PathExists(staged_path)) {
      return;
    }
    
    // Move current installation to backup
    if (!BackupCurrentInstallation()) {
      LOG(ERROR) << "Failed to backup current installation";
      return;
    }
    
    // Apply update
    if (!MoveFilesFromStaging(staged_path)) {
      LOG(ERROR) << "Failed to apply update, rolling back";
      RollbackFromBackup();
      return;
    }
    
    // Cleanup
    CleanupStagingDirectory();
    CleanupBackupDirectory();
    
    // Mark update as applied
    prefs_->SetString(prefs::kLastUpdateVersion, 
                     GetCurrentVersion().GetString());
  }
};
```

### 3. Distribution Channels

```cpp
// Update server configuration
{
  "channels": {
    "stable": {
      "update_url": "https://updates.dryft-browser.org/stable/",
      "update_interval_hours": 24,
      "rollout_percentage": 100
    },
    "beta": {
      "update_url": "https://updates.dryft-browser.org/beta/",
      "update_interval_hours": 12,
      "rollout_percentage": 100,
      "features": ["experimental_nips"]
    },
    "dev": {
      "update_url": "https://updates.dryft-browser.org/dev/",
      "update_interval_hours": 6,
      "rollout_percentage": 100,
      "features": ["experimental_nips", "debug_tools"]
    },
    "canary": {
      "update_url": "https://updates.dryft-browser.org/canary/",
      "update_interval_hours": 1,
      "rollout_percentage": 100,
      "features": ["all_experimental"],
      "auto_update": true
    }
  }
}

// Platform-specific distribution
class PlatformDistributor {
 public:
  // Windows: MSI/EXE installer
  void DistributeWindows() {
    // Upload to:
    // - dryft-browser.org/download/windows/
    // - Microsoft Store (MSIX package)
    // - Chocolatey package repository
    // - Winget package repository
  }
  
  // macOS: DMG/PKG installer  
  void DistributeMacOS() {
    // Upload to:
    // - dryft-browser.org/download/mac/
    // - Mac App Store (if approved)
    // - Homebrew cask
  }
  
  // Linux: Multiple package formats
  void DistributeLinux() {
    // Upload to:
    // - dryft-browser.org/download/linux/
    // - Flathub (Flatpak)
    // - Snap Store (Snap)
    // - AUR (Arch Linux)
    // - PPA (Ubuntu/Debian)
    // - COPR (Fedora)
    // - AppImage releases
  }
};
```

### 4. Rollout Strategy

```cpp
class RolloutController {
 public:
  struct RolloutConfig {
    double percentage = 0.0;
    std::vector<std::string> target_countries;
    std::vector<std::string> target_languages;
    base::Time start_time;
    base::Time end_time;
    bool pause_on_crash_spike = true;
    double max_crash_rate = 0.01;  // 1%
  };
  
  bool ShouldReceiveUpdate(const std::string& client_id,
                          const RolloutConfig& config) {
    // Check if in rollout time window
    base::Time now = base::Time::Now();
    if (now < config.start_time || now > config.end_time) {
      return false;
    }
    
    // Check rollout percentage
    uint32_t hash = base::PersistentHash(client_id);
    double position = static_cast<double>(hash) / UINT32_MAX;
    if (position > config.percentage / 100.0) {
      return false;
    }
    
    // Check geographic targeting
    if (!config.target_countries.empty()) {
      std::string country = GetClientCountry();
      if (!base::Contains(config.target_countries, country)) {
        return false;
      }
    }
    
    // Check crash rate threshold
    if (config.pause_on_crash_spike) {
      double crash_rate = GetCurrentCrashRate();
      if (crash_rate > config.max_crash_rate) {
        LOG(WARNING) << "Pausing rollout due to high crash rate: " 
                     << crash_rate;
        return false;
      }
    }
    
    return true;
  }
  
  // Gradual rollout schedule
  void ConfigureGradualRollout(const base::Version& version) {
    std::vector<RolloutStage> stages = {
      {.day = 1, .percentage = 1},     // 1% on day 1
      {.day = 2, .percentage = 5},     // 5% on day 2
      {.day = 3, .percentage = 10},    // 10% on day 3
      {.day = 4, .percentage = 25},    // 25% on day 4
      {.day = 5, .percentage = 50},    // 50% on day 5
      {.day = 7, .percentage = 100}    // 100% on day 7
    };
    
    ScheduleRolloutStages(version, stages);
  }
};
```

### 5. Enterprise Deployment

```xml
<!-- Enterprise policy template -->
<policyDefinitions>
  <policy name="TungstenUpdatePolicy">
    <parentCategory ref="dryft"/>
    <supportedOn ref="SUPPORTED_DRYFT_1_0"/>
    <elements>
      <enum id="UpdateChannel" valueName="UpdateChannel">
        <item displayName="Stable" value="stable"/>
        <item displayName="Beta" value="beta"/>
        <item displayName="Dev" value="dev"/>
      </enum>
      <boolean id="AutoUpdateEnabled" valueName="AutoUpdate"/>
      <decimal id="UpdateIntervalHours" valueName="UpdateInterval" 
               minValue="1" maxValue="168"/>
      <boolean id="NostrFeaturesEnabled" valueName="EnableNostr"/>
      <list id="AllowedRelays" key="Software\Policies\dryft\NostrRelays"/>
    </elements>
  </policy>
</policyDefinitions>

<!-- Group Policy ADMX -->
<policy name="TungstenNostrConfiguration" 
        displayName="Configure Nostr Features"
        key="Software\Policies\dryft\Nostr"
        valueName="Configuration">
  <parentCategory ref="dryft-nostr"/>
  <supportedOn ref="SUPPORTED_DRYFT_1_0"/>
  <enabledValue>
    <decimal value="1"/>
  </enabledValue>
  <disabledValue>
    <decimal value="0"/>
  </disabledValue>
  <elements>
    <boolean id="EnableLocalRelay" valueName="LocalRelay">
      <trueValue><decimal value="1"/></trueValue>
      <falseValue><decimal value="0"/></falseValue>
    </boolean>
    <decimal id="LocalRelayPort" valueName="LocalRelayPort" 
             minValue="1024" maxValue="65535"/>
  </elements>
</policy>
```

### 6. Update Server Infrastructure

```python
# update_server.py
from flask import Flask, request, jsonify
import hashlib
import json

app = Flask(__name__)

class UpdateServer:
    def __init__(self):
        self.releases = self.load_releases()
        self.rollout_config = self.load_rollout_config()
    
    @app.route('/update/check', methods=['POST'])
    def check_update(self):
        data = request.json
        current_version = data['current_version']
        channel = data['channel']
        client_id = data['client_id']
        
        # Find latest version for channel
        latest = self.get_latest_version(channel)
        
        if not latest or latest['version'] <= current_version:
            return jsonify({'update_available': False})
        
        # Check rollout eligibility
        if not self.is_eligible_for_update(client_id, latest):
            return jsonify({'update_available': False})
        
        # Return update info
        return jsonify({
            'update_available': True,
            'version': latest['version'],
            'download_url': self.get_download_url(latest, data['os'], data['arch']),
            'size': latest['size'],
            'hash': latest['hash'],
            'release_notes': latest['release_notes_url'],
            'critical': latest.get('critical', False),
            'nostr_features': latest.get('nostr_features', [])
        })
    
    def get_download_url(self, release, os, arch):
        """Generate platform-specific download URL"""
        base_url = f"https://cdn.dryft-browser.org/releases/{release['version']}/"
        
        if os == 'windows':
            return f"{base_url}dryft-{release['version']}-win-{arch}.exe"
        elif os == 'macos':
            return f"{base_url}dryft-{release['version']}-mac-universal.dmg"
        elif os == 'linux':
            return f"{base_url}dryft-{release['version']}-linux-{arch}.AppImage"
    
    def is_eligible_for_update(self, client_id, release):
        """Check if client should receive this update"""
        rollout = self.rollout_config.get(release['version'], {})
        
        # Check rollout percentage
        if 'percentage' in rollout:
            client_hash = int(hashlib.sha256(client_id.encode()).hexdigest(), 16)
            if (client_hash % 100) >= rollout['percentage']:
                return False
        
        # Check feature flags
        if 'required_features' in rollout:
            # Would check against client capabilities
            pass
        
        return True

# CDN configuration for binary distribution
class CDNConfig:
    """
    Global CDN setup for fast downloads:
    - Primary: CloudFlare
    - Fallback: AWS CloudFront
    - Mirror: Self-hosted servers
    """
    
    cdn_endpoints = {
        'primary': 'https://cdn.dryft-browser.org',
        'fallback': 'https://d1234567890.cloudfront.net',
        'mirrors': [
            'https://mirror1.dryft-browser.org',
            'https://mirror2.dryft-browser.org'
        ]
    }
```

### 7. Crash Reporting and Telemetry

```cpp
// Crash reporting integration
class TungstenCrashReporter {
 public:
  void ConfigureCrashReporting() {
    // Use Chromium's crash reporting with custom endpoint
    crash_reporter::SetCrashServerURL(
        "https://crashes.dryft-browser.org/submit");
    
    // Add Nostr-specific metadata
    crash_reporter::AddCrashMetadata("nostr_enabled", 
                                    IsNostrEnabled() ? "true" : "false");
    crash_reporter::AddCrashMetadata("local_relay_active",
                                    IsLocalRelayActive() ? "true" : "false");
    crash_reporter::AddCrashMetadata("connected_relays",
                                    GetConnectedRelayCount());
  }
  
  // Monitor crash rates for rollout decisions
  void MonitorCrashRates() {
    // Query crash server for rates
    crash_client_->GetCrashStats(
        base::BindOnce(&TungstenCrashReporter::OnCrashStatsReceived,
                      weak_factory_.GetWeakPtr()));
  }
  
 private:
  void OnCrashStatsReceived(const CrashStats& stats) {
    if (stats.crash_rate > kMaxAcceptableCrashRate) {
      // Notify rollout controller to pause
      rollout_controller_->PauseRollout(
          "High crash rate detected: " + 
          base::NumberToString(stats.crash_rate));
    }
  }
};
```

### 8. A/B Testing Framework

```cpp
class FeatureRolloutManager {
 public:
  // A/B test Nostr features
  void ConfigureNostrExperiments() {
    // Test different relay connection strategies
    experiment_manager_->RegisterExperiment(
        "NostrRelayStrategy",
        {
          {"control", 0.5, {}},
          {"aggressive_reconnect", 0.25, {{"reconnect_delay_ms", "100"}}},
          {"conservative_reconnect", 0.25, {{"reconnect_delay_ms", "5000"}}}
        });
    
    // Test NIP-07 permission models
    experiment_manager_->RegisterExperiment(
        "NIP07Permissions",
        {
          {"per_site", 0.33, {}},
          {"per_event_type", 0.33, {{"granular", "true"}}},
          {"global", 0.34, {{"require_password", "true"}}}
        });
  }
  
  // Measure experiment outcomes
  void CollectExperimentMetrics() {
    // User engagement metrics
    UMA_HISTOGRAM_COUNTS_100("Nostr.Experiment.EventsSignedPerDay",
                            GetDailySignedEvents());
    
    // Performance metrics
    UMA_HISTOGRAM_TIMES("Nostr.Experiment.RelayConnectionTime",
                       GetAverageConnectionTime());
    
    // Error rates
    UMA_HISTOGRAM_PERCENTAGE("Nostr.Experiment.ConnectionFailureRate",
                           GetConnectionFailureRate());
  }
};
```

### 9. Security Update Process

```cpp
class SecurityUpdateManager {
 public:
  struct SecurityUpdate {
    base::Version version;
    std::vector<CVE> fixed_cves;
    Severity severity;
    bool requires_immediate_update;
  };
  
  void HandleSecurityUpdate(const SecurityUpdate& update) {
    if (update.severity == Severity::CRITICAL) {
      // Force update for critical security issues
      ShowSecurityUpdateDialog(update);
      
      // Disable vulnerable features until updated
      if (update.requires_immediate_update) {
        DisableVulnerableFeatures(update.fixed_cves);
      }
    }
    
    // Fast-track security updates
    update_manager_->SetPriority(UpdatePriority::URGENT);
    update_manager_->CheckForUpdates(
        base::BindOnce(&SecurityUpdateManager::OnUpdateCheckComplete,
                      weak_factory_.GetWeakPtr()));
  }
  
 private:
  void DisableVulnerableFeatures(const std::vector<CVE>& cves) {
    for (const auto& cve : cves) {
      if (cve.component == "nostr") {
        // Temporarily disable Nostr features
        prefs_->SetBoolean(prefs::kNostrEnabled, false);
        ShowSecurityWarning(
            "Nostr features temporarily disabled due to security update. "
            "Please update dryft to re-enable.");
      }
    }
  }
};
```

### 10. Migration Support

```cpp
class MigrationManager {
 public:
  // Migrate from other browsers
  void MigrateFromBrowser(BrowserType source) {
    switch (source) {
      case BrowserType::CHROME:
      case BrowserType::BRAVE:
      case BrowserType::EDGE:
        MigrateChromiumBrowser(source);
        break;
      case BrowserType::FIREFOX:
        MigrateFirefox();
        break;
    }
    
    // Migrate Nostr extensions data if present
    MigrateNostrExtensions(source);
  }
  
 private:
  void MigrateNostrExtensions(BrowserType source) {
    // Check for Alby extension
    base::FilePath alby_data = GetExtensionDataPath(source, "alby");
    if (base::PathExists(alby_data)) {
      ImportAlbyKeys(alby_data);
    }
    
    // Check for nos2x extension
    base::FilePath nos2x_data = GetExtensionDataPath(source, "nos2x");
    if (base::PathExists(nos2x_data)) {
      ImportNos2xKeys(nos2x_data);
    }
  }
  
  void ImportAlbyKeys(const base::FilePath& alby_path) {
    // Read Alby's LevelDB storage
    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status status = leveldb::DB::Open(options, 
                                              alby_path.value(), 
                                              &db);
    
    if (status.ok()) {
      // Extract Nostr keys
      std::string key_data;
      status = db->Get(leveldb::ReadOptions(), "nostr_keys", &key_data);
      
      if (status.ok()) {
        // Import into dryft's key manager
        key_manager_->ImportKeys(key_data, KeyFormat::ALBY);
      }
    }
  }
};
```

### 11. Deployment Verification

```bash
#!/bin/bash
# verify_deployment.sh

VERSION=$1
PLATFORMS=("windows" "macos" "linux")
ARCHITECTURES=("x64" "arm64")

echo "Verifying dryft $VERSION deployment..."

# Check all platform binaries
for platform in "${PLATFORMS[@]}"; do
  for arch in "${ARCHITECTURES[@]}"; do
    URL="https://cdn.dryft-browser.org/releases/$VERSION/dryft-$VERSION-$platform-$arch"
    
    # Download and verify hash
    echo "Checking $platform-$arch..."
    curl -s "$URL.sha256" -o expected_hash
    curl -s "$URL" | sha256sum | cut -d' ' -f1 > actual_hash
    
    if ! diff -q expected_hash actual_hash > /dev/null; then
      echo "ERROR: Hash mismatch for $platform-$arch"
      exit 1
    fi
  done
done

# Test update server
echo "Testing update server..."
curl -X POST https://updates.dryft-browser.org/check \
  -H "Content-Type: application/json" \
  -d '{
    "current_version": "1.0.0",
    "channel": "stable",
    "os": "linux",
    "arch": "x64"
  }'

# Verify CDN propagation
echo "Checking CDN propagation..."
for endpoint in $(dig +short cdn.dryft-browser.org); do
  response=$(curl -s -o /dev/null -w "%{http_code}" \
    "http://$endpoint/releases/$VERSION/manifest.json")
  if [ "$response" != "200" ]; then
    echo "ERROR: CDN endpoint $endpoint not serving version $VERSION"
    exit 1
  fi
done

echo "Deployment verification complete!"
```

### 12. Rollback Procedures

```cpp
class RollbackManager {
 public:
  void InitiateRollback(const base::Version& target_version,
                       RollbackReason reason) {
    LOG(WARNING) << "Initiating rollback to " << target_version 
                 << " due to: " << ReasonToString(reason);
    
    // Check if we have the target version cached
    base::FilePath rollback_path = GetRollbackPath(target_version);
    if (!base::PathExists(rollback_path)) {
      // Download previous version
      DownloadVersion(target_version,
          base::BindOnce(&RollbackManager::OnVersionDownloaded,
                        weak_factory_.GetWeakPtr()));
      return;
    }
    
    // Perform rollback
    ExecuteRollback(rollback_path);
  }
  
 private:
  void ExecuteRollback(const base::FilePath& version_path) {
    // Backup current installation
    BackupCurrentInstallation("rollback_backup");
    
    // Apply previous version
    if (!ApplyVersion(version_path)) {
      LOG(ERROR) << "Rollback failed, attempting recovery";
      RecoverFromFailedRollback();
      return;
    }
    
    // Update preferences
    prefs_->SetString(prefs::kRollbackFromVersion, 
                     GetCurrentVersion().GetString());
    prefs_->SetBoolean(prefs::kIsRolledBack, true);
    
    // Restart browser
    chrome::AttemptRestart();
  }
  
  void RecoverFromFailedRollback() {
    // Last resort: download fresh stable version
    LOG(ERROR) << "Attempting fresh installation recovery";
    
    FreshInstallManager installer;
    installer.DownloadAndInstallStable(
        base::BindOnce(&RollbackManager::OnFreshInstallComplete,
                      weak_factory_.GetWeakPtr()));
  }
};
```

This deployment and update strategy ensures:
- Reliable multi-platform distribution
- Gradual rollouts with monitoring
- Enterprise management capabilities  
- Security update fast-tracking
- Rollback capabilities for stability
- Migration support from other browsers
- A/B testing for new features
- Global CDN distribution
- Comprehensive verification procedures