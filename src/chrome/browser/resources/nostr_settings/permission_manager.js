// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Permission management functionality for Nostr settings.
 */

const PermissionManager = {
  permissions: new Map(),
  filteredPermissions: new Map(),
  currentEditingOrigin: null,
  
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
    { kind: 1984, name: 'Report', category: 'social' },
    { kind: 9735, name: 'Zap', category: 'social' },
    { kind: 10002, name: 'Relay List', category: 'social' },
    { kind: 30023, name: 'Long Form', category: 'social' },
  ],
  
  /**
   * Initialize permission management
   */
  initialize() {
    this.setupEventListeners();
    this.setupSearch();
  },
  
  /**
   * Set up all event listeners
   */
  setupEventListeners() {
    // Control buttons
    document.getElementById('bulk-permissions-button').addEventListener('click', () => {
      this.showBulkActionsDialog();
    });
    
    document.getElementById('add-permission-button').addEventListener('click', () => {
      this.showAddPermissionDialog();
    });
    
    document.getElementById('clear-all-permissions').addEventListener('click', () => {
      this.clearAllPermissions();
    });
    
    // Dialog close buttons
    document.getElementById('close-permission-dialog').addEventListener('click', () => {
      document.getElementById('permission-edit-dialog').close();
    });
    
    document.getElementById('close-bulk-dialog').addEventListener('click', () => {
      document.getElementById('bulk-actions-dialog').close();
    });
    
    // Permission edit dialog buttons
    document.getElementById('cancel-permission-edit').addEventListener('click', () => {
      document.getElementById('permission-edit-dialog').close();
    });
    
    document.getElementById('save-permission-edit').addEventListener('click', () => {
      this.savePermissionEdit();
    });
    
    document.getElementById('reset-permission').addEventListener('click', () => {
      this.resetPermission();
    });
    
    // Bulk actions
    document.querySelectorAll('.bulk-action-button').forEach(button => {
      button.addEventListener('click', () => {
        this.performBulkAction(button.dataset.action);
      });
    });
    
    // Kind presets
    document.querySelectorAll('.preset-button').forEach(button => {
      button.addEventListener('click', () => {
        this.selectKindPreset(button.dataset.preset);
      });
    });
    
    // Expiration radio buttons
    document.querySelectorAll('input[name="expiration"]').forEach(radio => {
      radio.addEventListener('change', () => {
        this.updateExpirationUI();
      });
    });
  },
  
  /**
   * Set up search functionality
   */
  setupSearch() {
    const searchInput = document.getElementById('permission-search');
    searchInput.addEventListener('input', (e) => {
      this.filterPermissions(e.target.value);
    });
  },
  
  /**
   * Display permissions with enhanced UI
   * @param {Object} permissions
   */
  displayPermissions(permissions) {
    this.permissions.clear();
    
    // Convert permissions to Map for easier handling
    for (const [origin, permissionData] of Object.entries(permissions)) {
      this.permissions.set(origin, permissionData);
    }
    
    this.filteredPermissions = new Map(this.permissions);
    this.updatePermissionsList();
    this.updatePermissionsSummary();
  },
  
  /**
   * Update the permissions list display
   */
  updatePermissionsList() {
    const container = document.getElementById('permissions-list');
    const emptyState = document.getElementById('permissions-empty');
    
    // Clear existing items
    container.querySelectorAll('.permission-item').forEach(item => item.remove());
    
    if (this.filteredPermissions.size === 0) {
      emptyState.style.display = 'block';
      return;
    }
    
    emptyState.style.display = 'none';
    
    for (const [origin, permissionData] of this.filteredPermissions) {
      const permissionEl = this.createPermissionItem(origin, permissionData);
      container.appendChild(permissionEl);
    }
  },
  
  /**
   * Create a permission item element
   * @param {string} origin
   * @param {Object} permissionData
   * @returns {Element}
   */
  createPermissionItem(origin, permissionData) {
    const permissionEl = document.createElement('div');
    permissionEl.className = 'permission-item';
    
    const domain = this.extractDomain(origin);
    const icon = this.getSiteIcon(domain);
    const defaultPolicy = permissionData.default || 'ask';
    const lastUsed = this.formatLastUsed(permissionData.lastUsed);
    
    // Count method permissions
    const methods = permissionData.methods || {};
    const allowedMethods = Object.values(methods).filter(p => p === 'allow').length;
    const deniedMethods = Object.values(methods).filter(p => p === 'deny').length;
    
    permissionEl.innerHTML = `
      <div class="permission-item-info">
        <div class="permission-site-icon">${icon}</div>
        <div class="permission-site-details">
          <h4>${domain}</h4>
          <div class="permission-site-origin">${origin}</div>
          <div class="permission-site-status">
            <span class="permission-badge ${defaultPolicy}">${this.formatPolicy(defaultPolicy)}</span>
            ${allowedMethods > 0 ? `<span class="permission-badge allow">${allowedMethods} allowed</span>` : ''}
            ${deniedMethods > 0 ? `<span class="permission-badge deny">${deniedMethods} denied</span>` : ''}
          </div>
        </div>
      </div>
      <div class="permission-item-actions">
        ${lastUsed ? `<span class="permission-last-used">${lastUsed}</span>` : ''}
        <button class="secondary-button" data-action="edit" data-origin="${origin}">
          Edit
        </button>
        <button class="icon-button" data-action="delete" data-origin="${origin}">
          Delete
        </button>
      </div>
    `;
    
    // Add event listeners
    permissionEl.querySelectorAll('button').forEach(button => {
      button.addEventListener('click', () => {
        const action = button.dataset.action;
        const origin = button.dataset.origin;
        
        if (action === 'edit') {
          this.editPermission(origin);
        } else if (action === 'delete') {
          this.deletePermission(origin);
        }
      });
    });
    
    return permissionEl;
  },
  
  /**
   * Update permissions summary statistics
   */
  updatePermissionsSummary() {
    let totalSites = 0;
    let allowedSites = 0;
    let blockedSites = 0;
    let askSites = 0;
    
    for (const [origin, permissionData] of this.permissions) {
      totalSites++;
      const defaultPolicy = permissionData.default || 'ask';
      
      switch (defaultPolicy) {
        case 'allow':
          allowedSites++;
          break;
        case 'deny':
          blockedSites++;
          break;
        case 'ask':
        default:
          askSites++;
          break;
      }
    }
    
    document.getElementById('total-sites').textContent = totalSites;
    document.getElementById('allowed-sites').textContent = allowedSites;
    document.getElementById('blocked-sites').textContent = blockedSites;
    document.getElementById('ask-sites').textContent = askSites;
  },
  
  /**
   * Filter permissions by search term
   * @param {string} searchTerm
   */
  filterPermissions(searchTerm) {
    if (!searchTerm.trim()) {
      this.filteredPermissions = new Map(this.permissions);
    } else {
      this.filteredPermissions.clear();
      const term = searchTerm.toLowerCase();
      
      for (const [origin, permissionData] of this.permissions) {
        if (origin.toLowerCase().includes(term) || 
            this.extractDomain(origin).toLowerCase().includes(term)) {
          this.filteredPermissions.set(origin, permissionData);
        }
      }
    }
    
    this.updatePermissionsList();
  },
  
  /**
   * Edit permission for an origin
   * @param {string} origin
   */
  editPermission(origin) {
    const permissionData = this.permissions.get(origin);
    if (!permissionData) return;
    
    this.currentEditingOrigin = origin;
    this.populateEditDialog(origin, permissionData);
    document.getElementById('permission-edit-dialog').showModal();
  },
  
  /**
   * Populate the edit dialog with permission data
   * @param {string} origin
   * @param {Object} permissionData
   */
  populateEditDialog(origin, permissionData) {
    const domain = this.extractDomain(origin);
    const icon = this.getSiteIcon(domain);
    
    // Site info
    document.getElementById('permission-site-icon').textContent = icon;
    document.getElementById('permission-site-name').textContent = domain;
    document.getElementById('permission-site-origin').textContent = origin;
    
    // Default policy
    const defaultPolicy = permissionData.default || 'ask';
    document.getElementById('default-policy').value = defaultPolicy;
    
    // Method permissions
    const methods = permissionData.methods || {};
    document.querySelectorAll('.permission-method select').forEach(select => {
      const method = select.dataset.method;
      const value = methods[method] || 'default';
      select.value = value;
    });
    
    // Rate limits
    const rateLimits = permissionData.rateLimits || {};
    document.getElementById('requests-per-minute').value = rateLimits.requestsPerMinute || 60;
    document.getElementById('signs-per-hour').value = rateLimits.signsPerHour || 20;
    
    // Generate kind matrix
    this.populateKindMatrix(permissionData.kindPermissions || {});
    
    // Expiration
    this.populateExpirationSettings(permissionData.grantedUntil);
  },
  
  /**
   * Populate the kind permission matrix
   * @param {Object} kindPermissions
   */
  populateKindMatrix(kindPermissions) {
    const matrix = document.getElementById('kind-matrix');
    matrix.innerHTML = '';
    
    this.eventKinds.forEach(kindInfo => {
      const kindEl = document.createElement('div');
      kindEl.className = 'kind-item';
      
      const isAllowed = kindPermissions[kindInfo.kind] === 'allow';
      
      kindEl.innerHTML = `
        <input type="checkbox" id="kind-${kindInfo.kind}" ${isAllowed ? 'checked' : ''}>
        <label for="kind-${kindInfo.kind}">${kindInfo.name} (${kindInfo.kind})</label>
      `;
      
      matrix.appendChild(kindEl);
    });
  },
  
  /**
   * Populate expiration settings
   * @param {string} grantedUntil
   */
  populateExpirationSettings(grantedUntil) {
    let expirationType = 'never';
    
    if (grantedUntil) {
      const expirationDate = new Date(grantedUntil);
      const now = new Date();
      
      if (expirationDate > now) {
        expirationType = 'custom';
        const daysDiff = Math.ceil((expirationDate - now) / (1000 * 60 * 60 * 24));
        document.getElementById('expiration-days').value = daysDiff;
      }
    }
    
    document.querySelector(`input[name="expiration"][value="${expirationType}"]`).checked = true;
    this.updateExpirationUI();
  },
  
  /**
   * Update expiration UI based on selection
   */
  updateExpirationUI() {
    const selectedExpiration = document.querySelector('input[name="expiration"]:checked').value;
    const customExpiration = document.getElementById('custom-expiration');
    customExpiration.style.display = selectedExpiration === 'custom' ? 'flex' : 'none';
  },
  
  /**
   * Select a kind preset
   * @param {string} preset
   */
  selectKindPreset(preset) {
    // Update preset button states
    document.querySelectorAll('.preset-button').forEach(button => {
      button.classList.toggle('active', button.dataset.preset === preset);
    });
    
    // Update checkboxes based on preset
    const checkboxes = document.querySelectorAll('#kind-matrix input[type="checkbox"]');
    
    switch (preset) {
      case 'social':
        checkboxes.forEach(checkbox => {
          const kindId = parseInt(checkbox.id.replace('kind-', ''));
          const kindInfo = this.eventKinds.find(k => k.kind === kindId);
          checkbox.checked = kindInfo && kindInfo.category === 'social';
        });
        break;
        
      case 'messaging':
        checkboxes.forEach(checkbox => {
          const kindId = parseInt(checkbox.id.replace('kind-', ''));
          const kindInfo = this.eventKinds.find(k => k.kind === kindId);
          checkbox.checked = kindInfo && kindInfo.category === 'messaging';
        });
        break;
        
      case 'custom':
        // Don't change anything for custom
        break;
    }
  },
  
  /**
   * Save permission edits
   */
  async savePermissionEdit() {
    if (!this.currentEditingOrigin) return;
    
    const permissionData = this.gatherPermissionData();
    
    try {
      await NostrSettings.sendMessage('setPermissionFull', {
        origin: this.currentEditingOrigin,
        permission: permissionData
      });
      
      // Update local data
      this.permissions.set(this.currentEditingOrigin, permissionData);
      this.updatePermissionsList();
      this.updatePermissionsSummary();
      
      document.getElementById('permission-edit-dialog').close();
      NostrSettings.showSuccess('Permission updated successfully');
    } catch (error) {
      console.error('Failed to save permission:', error);
      NostrSettings.showError('Failed to save permission');
    }
  },
  
  /**
   * Gather permission data from the dialog
   * @returns {Object}
   */
  gatherPermissionData() {
    const defaultPolicy = document.getElementById('default-policy').value;
    
    // Method permissions
    const methods = {};
    document.querySelectorAll('.permission-method select').forEach(select => {
      const method = select.dataset.method;
      const value = select.value;
      if (value !== 'default') {
        methods[method] = value;
      }
    });
    
    // Kind permissions
    const kindPermissions = {};
    document.querySelectorAll('#kind-matrix input[type="checkbox"]').forEach(checkbox => {
      if (checkbox.checked) {
        const kindId = parseInt(checkbox.id.replace('kind-', ''));
        kindPermissions[kindId] = 'allow';
      }
    });
    
    // Rate limits
    const rateLimits = {
      requestsPerMinute: parseInt(document.getElementById('requests-per-minute').value),
      signsPerHour: parseInt(document.getElementById('signs-per-hour').value)
    };
    
    // Expiration
    let grantedUntil = null;
    const expirationType = document.querySelector('input[name="expiration"]:checked').value;
    
    if (expirationType === 'custom') {
      const days = parseInt(document.getElementById('expiration-days').value);
      const expirationDate = new Date();
      expirationDate.setDate(expirationDate.getDate() + days);
      grantedUntil = expirationDate.toISOString();
    }
    
    return {
      default: defaultPolicy,
      methods,
      kindPermissions,
      rateLimits,
      grantedUntil,
      lastUsed: new Date().toISOString()
    };
  },
  
  /**
   * Reset permission to default
   */
  async resetPermission() {
    if (!this.currentEditingOrigin) return;
    
    if (!confirm('Reset this site\'s permissions to default settings?')) {
      return;
    }
    
    try {
      await NostrSettings.sendMessage('resetPermission', {
        origin: this.currentEditingOrigin
      });
      
      // Update local data to show as reset to default (ask)
      const resetPermission = {
        default: 'ask',
        methods: {},
        kindPermissions: {},
        rateLimits: {
          requestsPerMinute: 60,
          signsPerHour: 20
        },
        lastUsed: new Date().toISOString()
      };
      this.permissions.set(this.currentEditingOrigin, resetPermission);
      this.updatePermissionsList();
      this.updatePermissionsSummary();
      
      document.getElementById('permission-edit-dialog').close();
      NostrSettings.showSuccess('Permission reset to default');
    } catch (error) {
      console.error('Failed to reset permission:', error);
      NostrSettings.showError('Failed to reset permission');
    }
  },
  
  /**
   * Delete a permission
   * @param {string} origin
   */
  async deletePermission(origin) {
    if (!confirm(`Delete all permissions for ${this.extractDomain(origin)}?`)) {
      return;
    }
    
    try {
      await NostrSettings.sendMessage('deletePermission', { origin });
      
      // Remove from local data
      this.permissions.delete(origin);
      this.filteredPermissions.delete(origin);
      this.updatePermissionsList();
      this.updatePermissionsSummary();
      
      NostrSettings.showSuccess('Permission deleted');
    } catch (error) {
      console.error('Failed to delete permission:', error);
      NostrSettings.showError('Failed to delete permission');
    }
  },
  
  /**
   * Show bulk actions dialog
   */
  showBulkActionsDialog() {
    document.getElementById('bulk-actions-dialog').showModal();
  },
  
  /**
   * Show add permission dialog
   */
  showAddPermissionDialog() {
    // TODO: Implement add permission dialog
    NostrSettings.showInfo('Add permission functionality coming soon');
  },
  
  /**
   * Perform bulk action
   * @param {string} action
   */
  async performBulkAction(action) {
    let confirmMessage = '';
    
    switch (action) {
      case 'allow-all':
        confirmMessage = 'Allow all sites to access Nostr?';
        break;
      case 'deny-all':
        confirmMessage = 'Deny all sites access to Nostr?';
        break;
      case 'reset-all':
        confirmMessage = 'Reset all sites to ask for permission?';
        break;
      case 'clear-all':
        confirmMessage = 'Clear all site permissions?';
        break;
    }
    
    if (!confirm(confirmMessage)) {
      return;
    }
    
    try {
      await NostrSettings.sendMessage('bulkPermissionAction', { action });
      
      // Reload permissions
      const permissions = await NostrSettings.sendMessage('getPermissions');
      this.displayPermissions(permissions);
      
      document.getElementById('bulk-actions-dialog').close();
      NostrSettings.showSuccess('Bulk action completed');
    } catch (error) {
      console.error('Failed to perform bulk action:', error);
      NostrSettings.showError('Failed to perform bulk action');
    }
  },
  
  /**
   * Clear all permissions
   */
  async clearAllPermissions() {
    if (!confirm('This will delete ALL site permissions. Continue?')) {
      return;
    }
    
    await this.performBulkAction('clear-all');
  },
  
  /**
   * Extract domain from origin
   * @param {string} origin
   * @returns {string}
   */
  extractDomain(origin) {
    try {
      const url = new URL(origin);
      return url.hostname;
    } catch {
      return origin;
    }
  },
  
  /**
   * Get site icon for domain
   * @param {string} domain
   * @returns {string}
   */
  getSiteIcon(domain) {
    // Simple icon mapping - could be enhanced with favicons
    const iconMap = {
      'localhost': '🏠',
      'github.com': '🐙',
      'twitter.com': '🐦',
      'nostrudel.ninja': '🥷',
      'primal.net': '⚡',
      'snort.social': '🐽',
      'damus.io': '🦊',
      'iris.to': '👁️'
    };
    
    return iconMap[domain] || '🌐';
  },
  
  /**
   * Format policy for display
   * @param {string} policy
   * @returns {string}
   */
  formatPolicy(policy) {
    const policyMap = {
      'allow': 'Always Allow',
      'deny': 'Always Deny',
      'ask': 'Ask Each Time'
    };
    
    return policyMap[policy] || 'Ask Each Time';
  },
  
  /**
   * Format last used date
   * @param {string} lastUsed
   * @returns {string}
   */
  formatLastUsed(lastUsed) {
    if (!lastUsed) return '';
    
    const date = new Date(lastUsed);
    const now = new Date();
    const diffMs = now - date;
    const diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24));
    
    if (diffDays === 0) {
      return 'Today';
    } else if (diffDays === 1) {
      return 'Yesterday';
    } else if (diffDays < 7) {
      return `${diffDays} days ago`;
    } else {
      return date.toLocaleDateString();
    }
  }
};

// Export for use in main settings file
window.PermissionManager = PermissionManager;