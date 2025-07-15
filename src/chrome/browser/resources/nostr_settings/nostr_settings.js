// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Main JavaScript for Nostr settings page.
 */

// Namespace for Nostr settings
const NostrSettings = {
  /**
   * Initialize the settings page
   */
  initialize() {
    // Set up navigation
    document.querySelectorAll('.nav-button').forEach(button => {
      button.addEventListener('click', () => {
        this.showSection(button.dataset.section);
      });
    });
    
    // Set up event listeners
    this.setupAccountsSection();
    this.setupPermissionsSection();
    this.setupLocalRelaySection();
    this.setupBlossomSection();
    this.setupSecuritySection();
    
    // Load initial data
    this.loadSettings();
    
    // Show first section
    this.showSection('accounts');
  },
  
  /**
   * Show a specific settings section
   * @param {string} sectionId
   */
  showSection(sectionId) {
    // Hide all sections
    document.querySelectorAll('.settings-section').forEach(section => {
      section.hidden = true;
    });
    
    // Show selected section
    const section = document.getElementById(sectionId);
    if (section) {
      section.hidden = false;
    }
    
    // Update nav buttons
    document.querySelectorAll('.nav-button').forEach(button => {
      button.classList.toggle('active', button.dataset.section === sectionId);
    });
  },
  
  /**
   * Load all settings from backend
   */
  async loadSettings() {
    // Load accounts
    const accounts = await this.sendMessage('getAccounts');
    this.displayAccounts(accounts);
    
    // Load permissions
    const permissions = await this.sendMessage('getPermissions');
    this.displayPermissions(permissions);
    
    // Load local relay config
    const relayConfig = await this.sendMessage('getLocalRelayConfig');
    this.displayRelayConfig(relayConfig);
    
    // TODO: Load other settings
  },
  
  /**
   * Set up accounts section
   */
  setupAccountsSection() {
    // Initialize AccountManager
    if (window.AccountManager) {
      AccountManager.initialize();
    }
  },
  
  /**
   * Display accounts list
   * @param {Array} accounts
   */
  displayAccounts(accounts) {
    // Delegate to AccountManager for enhanced UI
    if (window.AccountManager) {
      AccountManager.displayAccounts(accounts);
    }
  },
  
  /**
   * Set up permissions section
   */
  setupPermissionsSection() {
    // Permission controls will be added here
  },
  
  /**
   * Display permissions
   * @param {Object} permissions
   */
  displayPermissions(permissions) {
    const container = document.getElementById('permissions-list');
    container.innerHTML = '';
    
    for (const [origin, perms] of Object.entries(permissions)) {
      const permEl = document.createElement('div');
      permEl.className = 'permission-item';
      permEl.innerHTML = `
        <div class="permission-origin">${origin}</div>
        <div class="permission-controls">
          <select data-origin="${origin}" data-permission="getPublicKey">
            <option value="allow" ${perms.getPublicKey === 'allow' ? 'selected' : ''}>Allow</option>
            <option value="deny" ${perms.getPublicKey === 'deny' ? 'selected' : ''}>Deny</option>
            <option value="ask" ${perms.getPublicKey === 'ask' ? 'selected' : ''}>Ask</option>
          </select>
        </div>
      `;
      
      // Add change listener
      const select = permEl.querySelector('select');
      select.addEventListener('change', () => {
        this.updatePermission(origin, 'getPublicKey', select.value);
      });
      
      container.appendChild(permEl);
    }
  },
  
  /**
   * Set up local relay section
   */
  setupLocalRelaySection() {
    const enabledCheckbox = document.getElementById('relay-enabled');
    const settingsDiv = document.getElementById('relay-settings');
    
    enabledCheckbox.addEventListener('change', () => {
      settingsDiv.style.display = enabledCheckbox.checked ? 'block' : 'none';
      this.saveRelaySettings();
    });
    
    // Add listeners to all relay inputs
    const inputs = settingsDiv.querySelectorAll('input');
    inputs.forEach(input => {
      input.addEventListener('change', () => this.saveRelaySettings());
    });
  },
  
  /**
   * Display relay configuration
   * @param {Object} config
   */
  displayRelayConfig(config) {
    document.getElementById('relay-enabled').checked = config.enabled;
    document.getElementById('relay-port').value = config.port;
    document.getElementById('relay-interface').value = config.interface;
    document.getElementById('relay-external').checked = config.externalAccess;
    document.getElementById('relay-storage').value = config.maxStorageGB;
    document.getElementById('relay-events').value = config.maxEvents;
    document.getElementById('relay-retention').value = config.retentionDays;
    
    const settingsDiv = document.getElementById('relay-settings');
    settingsDiv.style.display = config.enabled ? 'block' : 'none';
  },
  
  /**
   * Save relay settings
   */
  async saveRelaySettings() {
    const config = {
      enabled: document.getElementById('relay-enabled').checked,
      port: parseInt(document.getElementById('relay-port').value),
      interface: document.getElementById('relay-interface').value,
      externalAccess: document.getElementById('relay-external').checked,
      maxStorageGB: parseInt(document.getElementById('relay-storage').value),
      maxEvents: parseInt(document.getElementById('relay-events').value),
      retentionDays: parseInt(document.getElementById('relay-retention').value)
    };
    
    await this.sendMessage('setLocalRelayConfig', config);
  },
  
  /**
   * Set up Blossom section
   */
  setupBlossomSection() {
    const enabledCheckbox = document.getElementById('blossom-enabled');
    const settingsDiv = document.getElementById('blossom-settings');
    
    enabledCheckbox.addEventListener('change', () => {
      settingsDiv.style.display = enabledCheckbox.checked ? 'block' : 'none';
    });
    
    document.getElementById('add-blossom-server').addEventListener('click', () => {
      this.showAddBlossomServerDialog();
    });
  },
  
  /**
   * Set up security section
   */
  setupSecuritySection() {
    document.getElementById('export-keys').addEventListener('click', () => {
      this.exportKeys();
    });
    
    document.getElementById('backup-settings').addEventListener('click', () => {
      this.backupSettings();
    });
  },
  
  /**
   * Update a permission
   * @param {string} origin
   * @param {string} permission
   * @param {string} state
   */
  async updatePermission(origin, permission, state) {
    await this.sendMessage('setPermission', origin, permission, state);
  },
  
  /**
   * Truncate pubkey for display
   * @param {string} pubkey
   * @return {string}
   */
  truncatePubkey(pubkey) {
    if (!pubkey || pubkey.length < 16) return pubkey;
    return pubkey.substring(0, 8) + '...' + pubkey.substring(pubkey.length - 8);
  },
  
  /**
   * Show add account dialog
   */
  showAddAccountDialog() {
    // TODO: Implement account creation dialog
    alert('Account creation not yet implemented');
  },
  
  /**
   * Edit account
   * @param {string} pubkey
   */
  editAccount(pubkey) {
    // TODO: Implement account editing
    alert('Account editing not yet implemented');
  },
  
  /**
   * Delete account
   * @param {string} pubkey
   */
  async deleteAccount(pubkey) {
    if (!confirm('Are you sure you want to delete this account?')) {
      return;
    }
    
    // TODO: Implement account deletion
    alert('Account deletion not yet implemented');
  },
  
  /**
   * Show add Blossom server dialog
   */
  showAddBlossomServerDialog() {
    const url = prompt('Enter Blossom server URL:');
    if (url) {
      // TODO: Add server
      alert('Server management not yet implemented');
    }
  },
  
  /**
   * Export keys
   */
  async exportKeys() {
    // TODO: Implement key export
    alert('Key export not yet implemented');
  },
  
  /**
   * Backup settings
   */
  async backupSettings() {
    // TODO: Implement settings backup
    alert('Settings backup not yet implemented');
  },
  
  /**
   * Show success notification
   * @param {string} message
   */
  showSuccess(message) {
    this.showToast(message, 'success');
  },
  
  /**
   * Show error notification
   * @param {string} message
   */
  showError(message) {
    this.showToast(message, 'error');
  },
  
  /**
   * Show info notification
   * @param {string} message
   */
  showInfo(message) {
    this.showToast(message, 'info');
  },
  
  /**
   * Show toast notification
   * @param {string} message
   * @param {string} type - 'success', 'error', or 'info'
   */
  showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) {
      // Fallback to console for development
      console[type === 'error' ? 'error' : 'log'](message);
      return;
    }
    
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
      <div class="toast-icon"></div>
      <div class="toast-message">${message}</div>
      <button class="toast-close">&times;</button>
    `;
    
    // Add close button functionality
    const closeButton = toast.querySelector('.toast-close');
    closeButton.addEventListener('click', () => {
      this.removeToast(toast);
    });
    
    container.appendChild(toast);
    
    // Auto-remove after 4 seconds (6 seconds for errors)
    const timeout = type === 'error' ? 6000 : 4000;
    setTimeout(() => {
      this.removeToast(toast);
    }, timeout);
  },
  
  /**
   * Remove toast with animation
   * @param {Element} toast
   */
  removeToast(toast) {
    if (!toast || !toast.parentNode) return;
    
    toast.style.animation = 'toastFadeOut 0.3s ease-in';
    setTimeout(() => {
      if (toast.parentNode) {
        toast.parentNode.removeChild(toast);
      }
    }, 300);
  },
  
  /**
   * Send message to backend
   * @param {string} method
   * @param {...any} args
   * @return {Promise}
   */
  sendMessage(method, ...args) {
    return cr.sendWithPromise(method, ...args);
  }
};

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  NostrSettings.initialize();
});