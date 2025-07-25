// Copyright 2024 The dryft Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Enhanced relay settings management for Nostr settings.
 */

const RelaySettingsManager = {
  config: null,
  status: null,
  updateInterval: null,
  
  // Common Nostr event kinds
  eventKinds: [
    { kind: 0, name: 'Profile', category: 'social' },
    { kind: 1, name: 'Text Note', category: 'social' },
    { kind: 3, name: 'Contacts', category: 'social' },
    { kind: 4, name: 'DM', category: 'messaging' },
    { kind: 5, name: 'Delete', category: 'social' },
    { kind: 6, name: 'Repost', category: 'social' },
    { kind: 7, name: 'Reaction', category: 'social' },
    { kind: 40, name: 'Channel Create', category: 'messaging' },
    { kind: 41, name: 'Channel Metadata', category: 'messaging' },
    { kind: 42, name: 'Channel Message', category: 'messaging' },
    { kind: 43, name: 'Channel Hide', category: 'messaging' },
    { kind: 44, name: 'Channel Mute', category: 'messaging' },
    { kind: 1984, name: 'Report', category: 'moderation' },
    { kind: 9735, name: 'Zap', category: 'payment' },
    { kind: 10002, name: 'Relay List', category: 'metadata' },
    { kind: 30023, name: 'Long Form', category: 'content' },
    { kind: 30078, name: 'Curated Lists', category: 'metadata' },
    { kind: 31989, name: 'App Metadata', category: 'metadata' },
    { kind: 31990, name: 'App Data', category: 'metadata' }
  ],
  
  /**
   * Initialize relay settings manager
   */
  initialize() {
    this.setupEventListeners();
    this.setupSliders();
    this.setupCollapsibleSections();
    this.populateEventKindsGrid();
    this.loadConfig();
    this.startStatusUpdates();
  },
  
  /**
   * Set up all event listeners
   */
  setupEventListeners() {
    // Main enable toggle
    document.getElementById('relay-enabled').addEventListener('change', (e) => {
      this.toggleRelayEnabled(e.target.checked);
    });
    
    // Copy relay address
    document.getElementById('copy-relay-address').addEventListener('click', () => {
      this.copyRelayAddress();
    });
    
    // Form inputs
    document.getElementById('relay-port').addEventListener('change', () => {
      this.validatePort();
      this.saveConfig();
    });
    
    document.getElementById('relay-interface').addEventListener('change', () => {
      this.validateInterface();
      this.saveConfig();
    });
    
    document.getElementById('relay-external').addEventListener('change', () => {
      this.saveConfig();
    });
    
    // Event kind presets
    document.querySelectorAll('.preset-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        this.selectEventKindPreset(btn.dataset.preset);
      });
    });
    
    // Configuration actions
    document.getElementById('relay-restart').addEventListener('click', () => {
      this.restartRelay();
    });
    
    document.getElementById('relay-reset-config').addEventListener('click', () => {
      this.resetToDefaults();
    });
    
    document.getElementById('relay-export-config').addEventListener('click', () => {
      this.exportConfig();
    });
    
    document.getElementById('relay-import-config').addEventListener('click', () => {
      this.importConfig();
    });
  },
  
  /**
   * Set up slider controls with two-way binding
   */
  setupSliders() {
    const sliders = [
      { slider: 'relay-max-connections', value: 'relay-max-connections-value' },
      { slider: 'relay-storage', value: 'relay-storage-value' },
      { slider: 'relay-events', value: 'relay-events-value' },
      { slider: 'relay-retention', value: 'relay-retention-value' },
      { slider: 'relay-max-subs', value: 'relay-max-subs-value' },
      { slider: 'relay-message-size', value: 'relay-message-size-value' },
      { slider: 'relay-event-size', value: 'relay-event-size-value' },
      { slider: 'relay-events-per-min', value: 'relay-events-per-min-value' },
      { slider: 'relay-req-per-min', value: 'relay-req-per-min-value' }
    ];
    
    sliders.forEach(({ slider, value }) => {
      const sliderEl = document.getElementById(slider);
      const valueEl = document.getElementById(value);
      
      if (!sliderEl || !valueEl) return;
      
      // Sync slider to input
      sliderEl.addEventListener('input', () => {
        valueEl.value = sliderEl.value;
        if (slider === 'relay-storage') {
          this.updateStorageProgress();
        }
        this.saveConfig();
      });
      
      // Sync input to slider
      valueEl.addEventListener('input', () => {
        sliderEl.value = valueEl.value;
        if (slider === 'relay-storage') {
          this.updateStorageProgress();
        }
        this.saveConfig();
      });
      
      // Validate on blur
      valueEl.addEventListener('blur', () => {
        this.validateSliderValue(slider, value);
      });
    });
  },
  
  /**
   * Set up collapsible sections
   */
  setupCollapsibleSections() {
    document.querySelectorAll('.collapsible-header').forEach(header => {
      header.addEventListener('click', () => {
        const group = header.closest('.settings-group');
        group.classList.toggle('collapsed');
      });
    });
  },
  
  /**
   * Populate event kinds grid
   */
  populateEventKindsGrid() {
    const grid = document.getElementById('event-kinds-grid');
    grid.innerHTML = '';
    
    this.eventKinds.forEach(kind => {
      const checkbox = document.createElement('div');
      checkbox.className = 'kind-checkbox';
      checkbox.innerHTML = `
        <input type="checkbox" id="kind-${kind.kind}" value="${kind.kind}">
        <label for="kind-${kind.kind}" class="kind-label">${kind.name} (${kind.kind})</label>
      `;
      
      checkbox.addEventListener('change', () => {
        this.updateEventKindPreset();
        this.saveConfig();
      });
      
      grid.appendChild(checkbox);
    });
  },
  
  /**
   * Load current configuration
   */
  async loadConfig() {
    try {
      this.config = await NostrSettings.sendMessage('getLocalRelayConfig');
      this.displayConfig(this.config);
    } catch (error) {
      console.error('Failed to load relay config:', error);
      NostrSettings.showError('Failed to load relay configuration');
    }
  },
  
  /**
   * Display configuration in UI
   */
  displayConfig(config) {
    if (!config) return;
    
    // Basic settings
    document.getElementById('relay-enabled').checked = config.enabled || false;
    document.getElementById('relay-port').value = config.port || 8081;
    document.getElementById('relay-interface').value = config.interface || '127.0.0.1';
    document.getElementById('relay-external').checked = config.externalAccess || false;
    
    // Storage settings
    document.getElementById('relay-storage').value = config.maxStorageGB || 1;
    document.getElementById('relay-storage-value').value = config.maxStorageGB || 1;
    document.getElementById('relay-events').value = config.maxEvents || 100000;
    document.getElementById('relay-events-value').value = config.maxEvents || 100000;
    document.getElementById('relay-retention').value = config.retentionDays || 30;
    document.getElementById('relay-retention-value').value = config.retentionDays || 30;
    
    // Performance settings
    document.getElementById('relay-max-connections').value = config.maxConnections || 100;
    document.getElementById('relay-max-connections-value').value = config.maxConnections || 100;
    document.getElementById('relay-max-subs').value = config.maxSubsPerConn || 20;
    document.getElementById('relay-max-subs-value').value = config.maxSubsPerConn || 20;
    document.getElementById('relay-message-size').value = (config.maxMessageSize || 512) / 1024;
    document.getElementById('relay-message-size-value').value = (config.maxMessageSize || 512) / 1024;
    document.getElementById('relay-event-size').value = (config.maxEventSize || 256) / 1024;
    document.getElementById('relay-event-size-value').value = (config.maxEventSize || 256) / 1024;
    
    // Rate limiting
    document.getElementById('relay-events-per-min').value = config.maxEventsPerMinute || 100;
    document.getElementById('relay-events-per-min-value').value = config.maxEventsPerMinute || 100;
    document.getElementById('relay-req-per-min').value = config.maxReqPerMinute || 60;
    document.getElementById('relay-req-per-min-value').value = config.maxReqPerMinute || 60;
    
    // Access control
    document.getElementById('relay-require-auth').checked = config.requireAuth || false;
    document.getElementById('relay-allowed-origins').value = (config.allowedOrigins || []).join('\n');
    document.getElementById('relay-blocked-pubkeys').value = (config.blockedPubkeys || []).join('\n');
    
    // Event kinds
    const allowedKinds = config.allowedKinds || [];
    this.eventKinds.forEach(kind => {
      const checkbox = document.getElementById(`kind-${kind.kind}`);
      if (checkbox) {
        checkbox.checked = allowedKinds.length === 0 || allowedKinds.includes(kind.kind);
      }
    });
    
    // Update UI state
    this.updateRelaySettingsVisibility();
    this.updateRelayAddress();
    this.updateStorageProgress();
    this.updateEventKindPreset();
  },
  
  /**
   * Start status updates
   */
  startStatusUpdates() {
    this.updateStatus();
    this.updateInterval = setInterval(() => {
      this.updateStatus();
    }, 5000); // Update every 5 seconds
  },
  
  /**
   * Update relay status
   */
  async updateStatus() {
    try {
      this.status = await NostrSettings.sendMessage('getLocalRelayStatus');
      this.displayStatus(this.status);
    } catch (error) {
      console.error('Failed to get relay status:', error);
    }
  },
  
  /**
   * Display relay status
   */
  displayStatus(status) {
    if (!status) return;
    
    const indicator = document.getElementById('relay-status-indicator');
    const text = document.getElementById('relay-status-text');
    const statsContainer = document.getElementById('relay-stats');
    const addressContainer = document.getElementById('relay-address');
    
    // Update status indicator
    indicator.className = `status-indicator ${status.state || 'stopped'}`;
    
    switch (status.state) {
      case 'running':
        text.textContent = 'Running';
        statsContainer.hidden = false;
        addressContainer.hidden = false;
        break;
      case 'starting':
        text.textContent = 'Starting...';
        statsContainer.hidden = true;
        addressContainer.hidden = true;
        break;
      case 'stopping':
        text.textContent = 'Stopping...';
        statsContainer.hidden = true;
        addressContainer.hidden = true;
        break;
      case 'error':
        text.textContent = `Error: ${status.error || 'Unknown error'}`;
        statsContainer.hidden = true;
        addressContainer.hidden = true;
        break;
      default:
        text.textContent = 'Stopped';
        statsContainer.hidden = true;
        addressContainer.hidden = true;
        break;
    }
    
    // Update statistics if running
    if (status.state === 'running' && status.stats) {
      document.getElementById('stat-connections').textContent = status.stats.connections || 0;
      document.getElementById('stat-events').textContent = status.stats.totalEvents || 0;
      document.getElementById('stat-storage').textContent = this.formatBytes(status.stats.storageUsed || 0);
      document.getElementById('stat-uptime').textContent = this.formatUptime(status.stats.uptime || 0);
      
      // Update storage progress
      this.updateStorageProgressWithUsage(status.stats.storageUsed || 0);
    }
  },
  
  /**
   * Toggle relay enabled state
   */
  async toggleRelayEnabled(enabled) {
    try {
      if (enabled) {
        await NostrSettings.sendMessage('startLocalRelay');
        NostrSettings.showSuccess('Local relay starting...');
      } else {
        await NostrSettings.sendMessage('stopLocalRelay');
        NostrSettings.showSuccess('Local relay stopping...');
      }
      
      this.updateRelaySettingsVisibility();
      this.updateRelayAddress();
      
      // Update status immediately
      setTimeout(() => this.updateStatus(), 1000);
    } catch (error) {
      console.error('Failed to toggle relay:', error);
      NostrSettings.showError('Failed to control relay service');
      
      // Revert checkbox state
      document.getElementById('relay-enabled').checked = !enabled;
    }
  },
  
  /**
   * Update relay settings visibility
   */
  updateRelaySettingsVisibility() {
    const enabled = document.getElementById('relay-enabled').checked;
    const settingsDiv = document.getElementById('relay-settings');
    settingsDiv.style.display = enabled ? 'block' : 'none';
  },
  
  /**
   * Update relay address display
   */
  updateRelayAddress() {
    const port = document.getElementById('relay-port').value;
    const interface_ = document.getElementById('relay-interface').value;
    const addressValue = document.getElementById('relay-address-value');
    
    const address = `ws://${interface_}:${port}`;
    addressValue.textContent = address;
  },
  
  /**
   * Validate port input
   */
  validatePort() {
    const portInput = document.getElementById('relay-port');
    const validation = document.getElementById('port-validation');
    const port = parseInt(portInput.value);
    
    if (port < 1024 || port > 65535) {
      validation.textContent = 'Port must be between 1024 and 65535';
      validation.className = 'validation-message error';
      return false;
    } else if (port < 8000) {
      validation.textContent = 'Ports below 8000 may require admin privileges';
      validation.className = 'validation-message warning';
    } else {
      validation.textContent = 'Port is available';
      validation.className = 'validation-message success';
    }
    
    this.updateRelayAddress();
    return true;
  },
  
  /**
   * Validate interface input
   */
  validateInterface() {
    const interfaceSelect = document.getElementById('relay-interface');
    const validation = document.getElementById('interface-validation');
    const interface_ = interfaceSelect.value;
    
    if (interface_ === '0.0.0.0') {
      validation.textContent = 'Warning: Accessible from all network interfaces';
      validation.className = 'validation-message warning';
    } else {
      validation.textContent = 'Localhost only - secure';
      validation.className = 'validation-message success';
    }
    
    this.updateRelayAddress();
    return true;
  },
  
  /**
   * Validate slider value
   */
  validateSliderValue(sliderId, valueId) {
    const slider = document.getElementById(sliderId);
    const valueInput = document.getElementById(valueId);
    const min = parseFloat(slider.min);
    const max = parseFloat(slider.max);
    const value = parseFloat(valueInput.value);
    
    if (value < min) {
      valueInput.value = min;
      slider.value = min;
    } else if (value > max) {
      valueInput.value = max;
      slider.value = max;
    }
  },
  
  /**
   * Update storage progress bar
   */
  updateStorageProgress() {
    const maxGB = parseFloat(document.getElementById('relay-storage').value);
    const maxBytes = maxGB * 1024 * 1024 * 1024;
    
    // This will be updated with actual usage when status is available
    this.updateStorageProgressWithUsage(0);
  },
  
  /**
   * Update storage progress with actual usage
   */
  updateStorageProgressWithUsage(usedBytes) {
    const maxGB = parseFloat(document.getElementById('relay-storage').value);
    const maxBytes = maxGB * 1024 * 1024 * 1024;
    const progressFill = document.getElementById('storage-progress-fill');
    const progressText = document.getElementById('storage-progress-text');
    
    const percentage = maxBytes > 0 ? (usedBytes / maxBytes) * 100 : 0;
    progressFill.style.width = `${Math.min(percentage, 100)}%`;
    progressText.textContent = `${this.formatBytes(usedBytes)} used of ${this.formatBytes(maxBytes)}`;
  },
  
  /**
   * Select event kind preset
   */
  selectEventKindPreset(preset) {
    // Update preset button states
    document.querySelectorAll('.preset-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.preset === preset);
    });
    
    // Update checkboxes based on preset
    this.eventKinds.forEach(kind => {
      const checkbox = document.getElementById(`kind-${kind.kind}`);
      if (!checkbox) return;
      
      switch (preset) {
        case 'standard':
          checkbox.checked = [0, 1, 3, 5, 6, 7].includes(kind.kind);
          break;
        case 'social':
          checkbox.checked = kind.category === 'social';
          break;
        case 'messaging':
          checkbox.checked = kind.category === 'messaging';
          break;
        case 'all':
          checkbox.checked = true;
          break;
        case 'custom':
          // Don't change anything for custom
          break;
      }
    });
    
    this.saveConfig();
  },
  
  /**
   * Update event kind preset based on current selection
   */
  updateEventKindPreset() {
    const checkedKinds = [];
    this.eventKinds.forEach(kind => {
      const checkbox = document.getElementById(`kind-${kind.kind}`);
      if (checkbox && checkbox.checked) {
        checkedKinds.push(kind.kind);
      }
    });
    
    // Determine which preset matches
    let matchingPreset = 'custom';
    
    if (checkedKinds.length === this.eventKinds.length) {
      matchingPreset = 'all';
    } else if (checkedKinds.length === 6 && [0, 1, 3, 5, 6, 7].every(k => checkedKinds.includes(k))) {
      matchingPreset = 'standard';
    } else if (this.eventKinds.filter(k => k.category === 'social').every(k => checkedKinds.includes(k.kind))) {
      const socialKinds = this.eventKinds.filter(k => k.category === 'social').map(k => k.kind);
      if (checkedKinds.filter(k => socialKinds.includes(k)).length === socialKinds.length) {
        matchingPreset = 'social';
      }
    } else if (this.eventKinds.filter(k => k.category === 'messaging').every(k => checkedKinds.includes(k.kind))) {
      const messagingKinds = this.eventKinds.filter(k => k.category === 'messaging').map(k => k.kind);
      if (checkedKinds.filter(k => messagingKinds.includes(k)).length === messagingKinds.length) {
        matchingPreset = 'messaging';
      }
    }
    
    // Update preset button states
    document.querySelectorAll('.preset-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.preset === matchingPreset);
    });
  },
  
  /**
   * Save configuration
   */
  async saveConfig() {
    const config = this.gatherConfig();
    
    try {
      await NostrSettings.sendMessage('setLocalRelayConfig', config);
      this.config = config;
    } catch (error) {
      console.error('Failed to save relay config:', error);
      NostrSettings.showError('Failed to save configuration');
    }
  },
  
  /**
   * Gather configuration from UI
   */
  gatherConfig() {
    // Get allowed event kinds
    const allowedKinds = [];
    this.eventKinds.forEach(kind => {
      const checkbox = document.getElementById(`kind-${kind.kind}`);
      if (checkbox && checkbox.checked) {
        allowedKinds.push(kind.kind);
      }
    });
    
    // Parse origins and pubkeys
    const allowedOrigins = document.getElementById('relay-allowed-origins').value
      .split('\n')
      .map(line => line.trim())
      .filter(line => line.length > 0);
      
    const blockedPubkeys = document.getElementById('relay-blocked-pubkeys').value
      .split('\n')
      .map(line => line.trim())
      .filter(line => line.length > 0);
    
    return {
      enabled: document.getElementById('relay-enabled').checked,
      port: parseInt(document.getElementById('relay-port').value),
      interface: document.getElementById('relay-interface').value,
      externalAccess: document.getElementById('relay-external').checked,
      maxStorageGB: parseFloat(document.getElementById('relay-storage-value').value),
      maxEvents: parseInt(document.getElementById('relay-events-value').value),
      retentionDays: parseInt(document.getElementById('relay-retention-value').value),
      maxConnections: parseInt(document.getElementById('relay-max-connections-value').value),
      maxSubsPerConn: parseInt(document.getElementById('relay-max-subs-value').value),
      maxMessageSize: Math.round(parseFloat(document.getElementById('relay-message-size-value').value) * 1024),
      maxEventSize: Math.round(parseFloat(document.getElementById('relay-event-size-value').value) * 1024),
      maxEventsPerMinute: parseInt(document.getElementById('relay-events-per-min-value').value),
      maxReqPerMinute: parseInt(document.getElementById('relay-req-per-min-value').value),
      requireAuth: document.getElementById('relay-require-auth').checked,
      allowedOrigins,
      blockedPubkeys,
      allowedKinds
    };
  },
  
  /**
   * Copy relay address to clipboard
   */
  async copyRelayAddress() {
    const address = document.getElementById('relay-address-value').textContent;
    
    try {
      await navigator.clipboard.writeText(address);
      NostrSettings.showSuccess('Relay address copied to clipboard');
    } catch (error) {
      console.error('Failed to copy address:', error);
      NostrSettings.showError('Failed to copy address');
    }
  },
  
  /**
   * Restart relay service
   */
  async restartRelay() {
    if (!confirm('Restart the local relay? This will disconnect all clients.')) {
      return;
    }
    
    try {
      await NostrSettings.sendMessage('stopLocalRelay');
      setTimeout(async () => {
        await NostrSettings.sendMessage('startLocalRelay');
        NostrSettings.showSuccess('Relay restarted successfully');
      }, 2000);
    } catch (error) {
      console.error('Failed to restart relay:', error);
      NostrSettings.showError('Failed to restart relay');
    }
  },
  
  /**
   * Reset configuration to defaults
   */
  async resetToDefaults() {
    if (!confirm('Reset all relay settings to defaults? This cannot be undone.')) {
      return;
    }
    
    try {
      await NostrSettings.sendMessage('resetLocalRelayConfig');
      await this.loadConfig();
      NostrSettings.showSuccess('Configuration reset to defaults');
    } catch (error) {
      console.error('Failed to reset config:', error);
      NostrSettings.showError('Failed to reset configuration');
    }
  },
  
  /**
   * Export configuration
   */
  exportConfig() {
    const config = this.gatherConfig();
    const blob = new Blob([JSON.stringify(config, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = 'dryft-relay-config.json';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    NostrSettings.showSuccess('Configuration exported');
  },
  
  /**
   * Import configuration
   */
  importConfig() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    
    input.addEventListener('change', async (e) => {
      const file = e.target.files[0];
      if (!file) return;
      
      try {
        const text = await file.text();
        const config = JSON.parse(text);
        
        // Validate config structure
        if (typeof config.port !== 'number' || config.port < 1024 || config.port > 65535) {
          throw new Error('Invalid port in configuration');
        }
        
        // Apply configuration
        await NostrSettings.sendMessage('setLocalRelayConfig', config);
        await this.loadConfig();
        
        NostrSettings.showSuccess('Configuration imported successfully');
      } catch (error) {
        console.error('Failed to import config:', error);
        NostrSettings.showError('Failed to import configuration: ' + error.message);
      }
    });
    
    input.click();
  },
  
  /**
   * Format bytes for display
   */
  formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  },
  
  /**
   * Format uptime for display
   */
  formatUptime(seconds) {
    if (seconds < 60) return `${seconds}s`;
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}h`;
    return `${Math.floor(seconds / 86400)}d`;
  },
  
  /**
   * Cleanup
   */
  destroy() {
    if (this.updateInterval) {
      clearInterval(this.updateInterval);
      this.updateInterval = null;
    }
  }
};

// Export for use in main settings file
window.RelaySettingsManager = RelaySettingsManager;