# Local Relay UI Integration Guide

This document describes how to integrate the Local Relay configuration UI into Tungsten's settings page when the settings infrastructure is available.

## Current Implementation Status

### ✅ Completed Backend Components

1. **LocalRelayConfigManager** - Manages all relay configuration preferences
   - `local_relay_config.h/cc`
   - Preference registration
   - Validation logic
   - Configuration getters/setters

2. **Preference Keys** - All preferences are registered and ready:
   ```cpp
   tungsten.relay.enabled
   tungsten.relay.port
   tungsten.relay.interface
   tungsten.relay.external_access
   tungsten.relay.max_storage_gb
   tungsten.relay.max_events
   tungsten.relay.retention_days
   tungsten.relay.max_connections
   tungsten.relay.max_subs_per_conn
   tungsten.relay.max_message_size
   tungsten.relay.max_event_size
   tungsten.relay.max_events_per_minute
   tungsten.relay.max_req_per_minute
   tungsten.relay.allowed_origins
   tungsten.relay.require_auth
   tungsten.relay.blocked_pubkeys
   tungsten.relay.allowed_kinds
   ```

3. **NostrService Integration** - Methods ready for UI:
   - `StartLocalRelay(callback)`
   - `StopLocalRelay(callback)`
   - `GetLocalRelayStatus()`
   - `IsLocalRelayEnabled()`

## UI Implementation Requirements

### Settings Page Structure

Based on the mockups, the Local Relay settings should appear in the Nostr settings section:

```
Local Services ──────────────────────────────────────────
[ ] Enable Local Relay (Port: [8081])
    Cache events locally for offline access
```

### Required UI Components

1. **Basic Settings Section**
   ```html
   <settings-toggle-button 
     pref="tungsten.relay.enabled"
     label="Enable Local Relay">
   </settings-toggle-button>
   
   <settings-input
     pref="tungsten.relay.port"
     type="number"
     min="1024"
     max="65535"
     label="Port">
   </settings-input>
   ```

2. **Advanced Settings (Collapsible)**
   - Network configuration
   - Storage limits
   - Performance tuning
   - Access control

3. **Status Display**
   - Current relay status (running/stopped)
   - Active connections count
   - Storage usage
   - Event count

## Integration Steps

### Step 1: Create Settings UI Files

When settings infrastructure is available, create:
- `/chrome/browser/resources/settings/nostr_page/local_relay_section.html`
- `/chrome/browser/resources/settings/nostr_page/local_relay_section.js`

### Step 2: Add to Settings Route

Register the route in the settings router:
```javascript
routes.NOSTR_LOCAL_RELAY = {
  page: 'nostr',
  section: 'localRelay',
  path: 'nostr/localRelay'
};
```

### Step 3: Implement UI Bindings

```javascript
class SettingsLocalRelaySection extends PolymerElement {
  static get properties() {
    return {
      prefs: Object,
      relayStatus_: Object,
    };
  }

  ready() {
    super.ready();
    this.loadRelayStatus_();
  }

  async loadRelayStatus_() {
    // Call NostrService.getLocalRelayStatus()
    const status = await chrome.nostr.getLocalRelayStatus();
    this.relayStatus_ = status;
  }

  onToggleRelay_() {
    if (this.prefs.tungsten.relay.enabled.value) {
      chrome.nostr.startLocalRelay();
    } else {
      chrome.nostr.stopLocalRelay();
    }
  }
}
```

### Step 4: Add Validation

Port validation is already implemented in the backend:
```cpp
bool LocalRelayConfigManager::IsValidPort(int port) {
  return port >= 1024 && port <= 65535;
}
```

The UI should reflect these constraints.

## API Endpoints

The following APIs need to be exposed to the settings UI:

1. **chrome.nostr.localRelay.getStatus()**
   - Returns: `{running: bool, address: string, connections: number, events: number}`

2. **chrome.nostr.localRelay.start()**
   - Starts the relay with current configuration

3. **chrome.nostr.localRelay.stop()**
   - Stops the relay

4. **chrome.nostr.localRelay.getStatistics()**
   - Returns detailed statistics

## Testing the Configuration

Unit tests are already implemented in `local_relay_config_unittest.cc`. To test the configuration system:

```bash
./out/Default/unit_tests --gtest_filter="LocalRelayConfigTest.*"
```

## Next Steps

1. Wait for settings infrastructure to be available
2. Create the UI components following Chrome's settings patterns
3. Connect UI to the existing backend configuration system
4. Add integration tests for the complete flow

The backend is fully ready and waiting for UI integration.