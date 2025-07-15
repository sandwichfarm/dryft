// Copyright 2024 The Chromium Authors and Alex313031
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Manages Blossom storage settings including server configuration,
 * storage quotas, MIME type restrictions, and content mirroring.
 */

class BlossomSettingsManager {
  constructor() {
    this.elements = {};
    this.serverTestResults = new Map();
    this.initializeElements();
    this.attachEventListeners();
    this.loadSettings();
  }

  initializeElements() {
    // Enable/disable toggle
    this.elements.enabled = document.getElementById('blossom-enabled');
    this.elements.settingsContainer = document.getElementById('blossom-settings');
    
    // Local server settings
    this.elements.port = document.getElementById('blossom-port');
    this.elements.autoStart = document.getElementById('blossom-auto-start');
    this.elements.testLocalButton = document.getElementById('test-local-blossom');
    
    // Storage configuration
    this.elements.storageSlider = document.getElementById('blossom-storage-slider');
    this.elements.storageQuota = document.getElementById('blossom-storage-quota');
    this.elements.usageBar = document.getElementById('blossom-usage-bar');
    this.elements.usageText = document.getElementById('blossom-usage-text');
    this.elements.clearCacheButton = document.getElementById('clear-blossom-cache');
    this.elements.exportDataButton = document.getElementById('export-blossom-data');
    
    // Upload restrictions
    this.elements.maxFileSize = document.getElementById('blossom-max-file-size');
    this.elements.mimeTypeCheckboxes = document.querySelectorAll('input[name="mime-type"]');
    this.elements.customMimeTypes = document.getElementById('custom-mime-types');
    this.elements.compressImages = document.getElementById('blossom-compress-images');
    
    // Server management
    this.elements.serverList = document.getElementById('blossom-servers');
    this.elements.testServersButton = document.getElementById('test-blossom-servers');
    this.elements.importServerListButton = document.getElementById('import-server-list');
    this.elements.serverStatus = document.getElementById('blossom-server-status');
    
    // Mirroring configuration
    this.elements.autoMirror = document.getElementById('blossom-auto-mirror');
    this.elements.mirroringSettings = document.getElementById('mirroring-settings');
    this.elements.mirrorRedundancy = document.getElementById('mirror-redundancy');
    this.elements.mirrorVerify = document.getElementById('mirror-verify');
    this.elements.mirrorRetry = document.getElementById('mirror-retry');
  }

  attachEventListeners() {
    // Enable/disable toggle
    this.elements.enabled.addEventListener('change', () => this.toggleBlossom());
    
    // Local server
    this.elements.port.addEventListener('change', () => this.saveSettings());
    this.elements.autoStart.addEventListener('change', () => this.saveSettings());
    this.elements.testLocalButton.addEventListener('click', () => this.testLocalServer());
    
    // Storage configuration
    this.elements.storageSlider.addEventListener('input', (e) => {
      this.elements.storageQuota.value = e.target.value;
      this.saveSettings();
    });
    
    this.elements.storageQuota.addEventListener('input', (e) => {
      this.elements.storageSlider.value = e.target.value;
      this.saveSettings();
    });
    
    this.elements.clearCacheButton.addEventListener('click', () => this.clearCache());
    this.elements.exportDataButton.addEventListener('click', () => this.exportData());
    
    // Upload restrictions
    this.elements.maxFileSize.addEventListener('change', () => this.saveSettings());
    this.elements.mimeTypeCheckboxes.forEach(checkbox => {
      checkbox.addEventListener('change', () => this.saveSettings());
    });
    this.elements.customMimeTypes.addEventListener('change', () => this.saveSettings());
    this.elements.compressImages.addEventListener('change', () => this.saveSettings());
    
    // Server management
    this.elements.serverList.addEventListener('change', () => this.saveSettings());
    this.elements.testServersButton.addEventListener('click', () => this.testAllServers());
    this.elements.importServerListButton.addEventListener('click', () => this.importServerList());
    
    // Mirroring
    this.elements.autoMirror.addEventListener('change', () => {
      this.toggleMirroringSettings();
      this.saveSettings();
    });
    this.elements.mirrorRedundancy.addEventListener('change', () => this.saveSettings());
    this.elements.mirrorVerify.addEventListener('change', () => this.saveSettings());
    this.elements.mirrorRetry.addEventListener('change', () => this.saveSettings());
  }

  async loadSettings() {
    const settings = await this.sendMessage('getBlossomConfig');
    
    if (settings) {
      // Enable/disable
      this.elements.enabled.checked = settings.enabled || false;
      this.toggleBlossomSettings();
      
      // Local server
      this.elements.port.value = settings.port || 8088;
      this.elements.autoStart.checked = settings.autoStart || false;
      
      // Storage
      const quotaGB = Math.round((settings.storageQuota || 10737418240) / (1024 * 1024 * 1024));
      this.elements.storageSlider.value = quotaGB;
      this.elements.storageQuota.value = quotaGB;
      
      // Upload restrictions
      this.elements.maxFileSize.value = settings.maxFileSize || 100;
      this.loadMimeTypes(settings.allowedMimeTypes || ['image/*', 'video/*', 'audio/*']);
      this.elements.customMimeTypes.value = (settings.customMimeTypes || []).join('\n');
      this.elements.compressImages.checked = settings.compressImages || false;
      
      // Server list
      this.elements.serverList.value = (settings.servers || []).join('\n');
      
      // Mirroring
      this.elements.autoMirror.checked = settings.autoMirror || false;
      this.elements.mirrorRedundancy.value = settings.mirrorRedundancy || '2';
      this.elements.mirrorVerify.checked = settings.mirrorVerify || false;
      this.elements.mirrorRetry.value = settings.mirrorRetry || '3';
      this.toggleMirroringSettings();
      
      // Update usage display
      this.updateUsageDisplay();
    }
  }

  async saveSettings() {
    const settings = {
      enabled: this.elements.enabled.checked,
      port: parseInt(this.elements.port.value, 10),
      autoStart: this.elements.autoStart.checked,
      storageQuota: parseInt(this.elements.storageQuota.value, 10) * 1024 * 1024 * 1024,
      maxFileSize: parseInt(this.elements.maxFileSize.value, 10),
      allowedMimeTypes: this.getSelectedMimeTypes(),
      customMimeTypes: this.elements.customMimeTypes.value.split('\n').filter(Boolean),
      compressImages: this.elements.compressImages.checked,
      servers: this.elements.serverList.value.split('\n').filter(Boolean),
      autoMirror: this.elements.autoMirror.checked,
      mirrorRedundancy: this.elements.mirrorRedundancy.value,
      mirrorVerify: this.elements.mirrorVerify.checked,
      mirrorRetry: parseInt(this.elements.mirrorRetry.value, 10)
    };
    
    await this.sendMessage('setBlossomConfig', settings);
    showToast('Blossom settings saved');
  }

  toggleBlossom() {
    this.toggleBlossomSettings();
    this.saveSettings();
  }

  toggleBlossomSettings() {
    const enabled = this.elements.enabled.checked;
    this.elements.settingsContainer.style.display = enabled ? 'block' : 'none';
  }

  toggleMirroringSettings() {
    const enabled = this.elements.autoMirror.checked;
    this.elements.mirroringSettings.style.display = enabled ? 'block' : 'none';
  }

  getSelectedMimeTypes() {
    const selected = [];
    this.elements.mimeTypeCheckboxes.forEach(checkbox => {
      if (checkbox.checked) {
        selected.push(checkbox.value);
      }
    });
    return selected;
  }

  loadMimeTypes(mimeTypes) {
    this.elements.mimeTypeCheckboxes.forEach(checkbox => {
      checkbox.checked = mimeTypes.includes(checkbox.value);
    });
  }

  async testLocalServer() {
    const button = this.elements.testLocalButton;
    button.disabled = true;
    button.textContent = 'Testing...';
    
    try {
      const result = await this.sendMessage('testLocalBlossom');
      if (result.success) {
        showToast('Local Blossom server is running', 'success');
      } else {
        showToast(`Local server test failed: ${result.error}`, 'error');
      }
    } catch (error) {
      showToast('Failed to test local server', 'error');
    } finally {
      button.disabled = false;
      button.textContent = 'Test Connection';
    }
  }

  async testAllServers() {
    const button = this.elements.testServersButton;
    button.disabled = true;
    button.textContent = 'Testing...';
    
    const servers = this.elements.serverList.value.split('\n').filter(Boolean);
    this.elements.serverStatus.innerHTML = '';
    
    for (const server of servers) {
      const statusDiv = document.createElement('div');
      statusDiv.className = 'server-status-item';
      statusDiv.innerHTML = `
        <span class="server-url">${server}</span>
        <span class="server-status testing">Testing...</span>
      `;
      this.elements.serverStatus.appendChild(statusDiv);
      
      try {
        const result = await this.sendMessage('testBlossomServer', server);
        const statusSpan = statusDiv.querySelector('.server-status');
        if (result.success) {
          statusSpan.className = 'server-status online';
          statusSpan.textContent = 'Online';
          this.serverTestResults.set(server, true);
        } else {
          statusSpan.className = 'server-status offline';
          statusSpan.textContent = 'Offline';
          this.serverTestResults.set(server, false);
        }
      } catch (error) {
        const statusSpan = statusDiv.querySelector('.server-status');
        statusSpan.className = 'server-status error';
        statusSpan.textContent = 'Error';
        this.serverTestResults.set(server, false);
      }
    }
    
    button.disabled = false;
    button.textContent = 'Test All Servers';
  }

  async importServerList() {
    const button = this.elements.importServerListButton;
    button.disabled = true;
    button.textContent = 'Importing...';
    
    try {
      const result = await this.sendMessage('importBlossomServers');
      if (result.servers && result.servers.length > 0) {
        const currentServers = this.elements.serverList.value.split('\n').filter(Boolean);
        const newServers = result.servers.filter(s => !currentServers.includes(s));
        
        if (newServers.length > 0) {
          this.elements.serverList.value = [...currentServers, ...newServers].join('\n');
          this.saveSettings();
          showToast(`Imported ${newServers.length} new servers`, 'success');
        } else {
          showToast('No new servers found', 'info');
        }
      } else {
        showToast('No server list found in Nostr events', 'warning');
      }
    } catch (error) {
      showToast('Failed to import server list', 'error');
    } finally {
      button.disabled = false;
      button.textContent = 'Import from Nostr';
    }
  }

  async updateUsageDisplay() {
    try {
      const stats = await this.sendMessage('getBlossomStats');
      if (stats) {
        const usageGB = stats.usage / (1024 * 1024 * 1024);
        const quotaGB = stats.quota / (1024 * 1024 * 1024);
        const percentage = (stats.usage / stats.quota) * 100;
        
        this.elements.usageBar.style.width = `${percentage}%`;
        this.elements.usageText.textContent = `${usageGB.toFixed(2)} GB / ${quotaGB.toFixed(0)} GB`;
        
        // Color code based on usage
        if (percentage > 90) {
          this.elements.usageBar.style.backgroundColor = '#ff4444';
        } else if (percentage > 75) {
          this.elements.usageBar.style.backgroundColor = '#ff9944';
        } else {
          this.elements.usageBar.style.backgroundColor = '#44ff44';
        }
      }
    } catch (error) {
      console.error('Failed to get Blossom stats:', error);
    }
  }

  async clearCache() {
    if (confirm('Are you sure you want to clear the Blossom cache? This will remove all locally stored content.')) {
      try {
        await this.sendMessage('clearBlossomCache');
        showToast('Blossom cache cleared', 'success');
        this.updateUsageDisplay();
      } catch (error) {
        showToast('Failed to clear cache', 'error');
      }
    }
  }

  async exportData() {
    try {
      const result = await this.sendMessage('exportBlossomData');
      if (result.success) {
        showToast('Blossom data exported successfully', 'success');
      } else {
        showToast('Failed to export data', 'error');
      }
    } catch (error) {
      showToast('Export failed', 'error');
    }
  }

  sendMessage(method, params) {
    return new Promise((resolve, reject) => {
      chrome.send(method, params ? [params] : []);
      
      // Set up response handler
      const responseHandler = (response) => {
        resolve(response);
      };
      
      // Register temporary handler for the response
      const callbackName = `${method}Response`;
      window[callbackName] = responseHandler;
      
      // Timeout after 10 seconds
      setTimeout(() => {
        delete window[callbackName];
        reject(new Error('Request timeout'));
      }, 10000);
    });
  }
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    window.blossomSettingsManager = new BlossomSettingsManager();
  });
} else {
  window.blossomSettingsManager = new BlossomSettingsManager();
}