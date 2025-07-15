// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Account management functionality for Nostr settings.
 */

const AccountManager = {
  currentStep: 1,
  generatedKeys: null,
  importedKey: null,
  
  /**
   * Initialize account management
   */
  initialize() {
    this.setupEventListeners();
  },
  
  /**
   * Set up all event listeners
   */
  setupEventListeners() {
    // Add account button
    document.getElementById('add-account-button').addEventListener('click', () => {
      this.showCreateAccountDialog();
    });
    
    // Import account button
    document.getElementById('import-account-button').addEventListener('click', () => {
      this.showImportAccountDialog();
    });
    
    // Dialog close buttons
    document.getElementById('close-creation-dialog').addEventListener('click', () => {
      document.getElementById('account-creation-dialog').close();
      this.resetWizard();
    });
    
    document.getElementById('close-import-dialog').addEventListener('click', () => {
      document.getElementById('import-account-dialog').close();
      this.resetImport();
    });
    
    document.getElementById('close-switch-dialog').addEventListener('click', () => {
      document.getElementById('switch-account-dialog').close();
    });
    
    // Wizard navigation
    document.getElementById('wizard-back').addEventListener('click', () => {
      this.previousStep();
    });
    
    document.getElementById('wizard-next').addEventListener('click', () => {
      this.nextStep();
    });
    
    document.getElementById('wizard-finish').addEventListener('click', () => {
      this.createAccount();
    });
    
    // Import tabs
    document.querySelectorAll('.tab-button').forEach(button => {
      button.addEventListener('click', () => {
        this.switchImportTab(button.dataset.tab);
      });
    });
    
    // Import confirm button
    document.getElementById('confirm-import').addEventListener('click', () => {
      this.importAccount();
    });
    
    document.getElementById('cancel-import').addEventListener('click', () => {
      document.getElementById('import-account-dialog').close();
      this.resetImport();
    });
    
    // Toggle visibility buttons
    document.querySelectorAll('.toggle-visibility').forEach(button => {
      button.addEventListener('click', () => {
        this.toggleVisibility(button.dataset.target);
      });
    });
    
    // Copy buttons
    document.querySelectorAll('.copy-button').forEach(button => {
      button.addEventListener('click', () => {
        this.copyToClipboard(button.dataset.copy);
      });
    });
    
    // QR scanner
    document.getElementById('start-qr-scan').addEventListener('click', () => {
      this.startQRScanner();
    });
    
    // File import
    document.getElementById('import-file-input').addEventListener('change', (e) => {
      this.handleFileSelect(e);
    });
  },
  
  /**
   * Display accounts with enhanced UI
   * @param {Array} accounts
   */
  displayAccounts(accounts) {
    const container = document.getElementById('accounts-list');
    container.innerHTML = '';
    
    // Find current account
    const currentAccount = accounts.find(acc => acc.isDefault);
    if (currentAccount) {
      this.displayCurrentAccount(currentAccount);
    }
    
    accounts.forEach(account => {
      const accountEl = document.createElement('div');
      accountEl.className = 'account-item';
      accountEl.innerHTML = `
        <div class="account-info">
          <div class="account-avatar">
            <img src="${account.picture || ''}" alt="Avatar">
            <div class="avatar-placeholder">${this.getInitial(account.name)}</div>
          </div>
          <div class="account-details">
            <div class="account-name">${account.name || 'Unnamed Account'}</div>
            <div class="account-pubkey">${this.truncatePubkey(account.pubkey)}</div>
            ${account.nip05 ? `<div class="account-nip05">${account.nip05}</div>` : ''}
          </div>
        </div>
        <div class="account-actions">
          ${account.isDefault ? '<span class="default-badge">Active</span>' : ''}
          ${!account.isDefault ? `<button class="secondary-button" data-action="switch" data-pubkey="${account.pubkey}">Switch</button>` : ''}
          <button class="icon-button" data-action="edit" data-pubkey="${account.pubkey}">Edit</button>
          <button class="icon-button" data-action="export" data-pubkey="${account.pubkey}">Export</button>
          <button class="icon-button" data-action="delete" data-pubkey="${account.pubkey}">Delete</button>
        </div>
      `;
      
      // Add event listeners
      accountEl.querySelectorAll('button').forEach(button => {
        button.addEventListener('click', () => {
          const action = button.dataset.action;
          const pubkey = button.dataset.pubkey;
          
          switch (action) {
            case 'switch':
              this.switchAccount(pubkey);
              break;
            case 'edit':
              this.editAccount(pubkey);
              break;
            case 'export':
              this.exportAccount(pubkey);
              break;
            case 'delete':
              this.deleteAccount(pubkey);
              break;
          }
        });
      });
      
      container.appendChild(accountEl);
    });
  },
  
  /**
   * Display current account in header
   * @param {Object} account
   */
  displayCurrentAccount(account) {
    const currentAccountDiv = document.getElementById('current-account');
    currentAccountDiv.hidden = false;
    
    const avatar = document.getElementById('current-account-avatar');
    avatar.src = account.picture || '';
    
    const placeholder = document.getElementById('current-account-placeholder');
    placeholder.textContent = this.getInitial(account.name);
    
    document.getElementById('current-account-name').textContent = account.name || 'Unnamed Account';
    document.getElementById('current-account-pubkey').textContent = this.truncatePubkey(account.pubkey);
    
    const nip05El = document.getElementById('current-account-nip05');
    if (account.nip05) {
      nip05El.textContent = account.nip05;
      nip05El.style.display = 'block';
    } else {
      nip05El.style.display = 'none';
    }
  },
  
  /**
   * Show create account dialog
   */
  showCreateAccountDialog() {
    const dialog = document.getElementById('account-creation-dialog');
    this.generateNewKeys();
    dialog.showModal();
  },
  
  /**
   * Show import account dialog
   */
  showImportAccountDialog() {
    const dialog = document.getElementById('import-account-dialog');
    dialog.showModal();
  },
  
  /**
   * Generate new keypair
   */
  async generateNewKeys() {
    try {
      const response = await NostrSettings.sendMessage('generateKeys');
      this.generatedKeys = response;
      
      // Display keys
      document.getElementById('generated-npub').textContent = response.npub;
      document.getElementById('generated-nsec').dataset.value = response.nsec;
    } catch (error) {
      console.error('Failed to generate keys:', error);
      NostrSettings.showError('Failed to generate keys');
    }
  },
  
  /**
   * Navigate to next wizard step
   */
  nextStep() {
    if (this.currentStep === 3) return;
    
    // Validate current step
    if (this.currentStep === 1 && !this.generatedKeys) {
      NostrSettings.showError('Please generate keys first');
      return;
    }
    
    if (this.currentStep === 2) {
      const name = document.getElementById('account-name').value.trim();
      if (!name) {
        NostrSettings.showError('Please enter a display name');
        return;
      }
    }
    
    this.currentStep++;
    this.updateWizardUI();
  },
  
  /**
   * Navigate to previous wizard step
   */
  previousStep() {
    if (this.currentStep === 1) return;
    this.currentStep--;
    this.updateWizardUI();
  },
  
  /**
   * Update wizard UI based on current step
   */
  updateWizardUI() {
    // Update step indicators
    document.querySelectorAll('.step').forEach(step => {
      const stepNum = parseInt(step.dataset.step);
      step.classList.toggle('active', stepNum === this.currentStep);
    });
    
    // Update step content
    document.querySelectorAll('.wizard-step').forEach(step => {
      step.hidden = true;
    });
    document.getElementById(`step-${this.currentStep}`).hidden = false;
    
    // Update buttons
    document.getElementById('wizard-back').hidden = this.currentStep === 1;
    document.getElementById('wizard-next').hidden = this.currentStep === 3;
    document.getElementById('wizard-finish').hidden = this.currentStep !== 3;
    
    // Special handling for step 3
    if (this.currentStep === 3) {
      this.setupBackupStep();
    }
  },
  
  /**
   * Setup backup step with QR code
   */
  setupBackupStep() {
    const downloadCheckbox = document.getElementById('backup-downloaded');
    downloadCheckbox.addEventListener('change', () => {
      if (downloadCheckbox.checked) {
        this.downloadBackup();
      }
    });
  },
  
  /**
   * Create the account
   */
  async createAccount() {
    // Check if at least one backup option is selected
    const backupDownloaded = document.getElementById('backup-downloaded').checked;
    const backupWritten = document.getElementById('backup-written').checked;
    const backupPasswordManager = document.getElementById('backup-password-manager').checked;
    
    if (!backupDownloaded && !backupWritten && !backupPasswordManager) {
      NostrSettings.showError('Please confirm you have backed up your keys');
      return;
    }
    
    // Gather account data
    const accountData = {
      pubkey: this.generatedKeys.pubkey,
      privkey: this.generatedKeys.privkey,
      name: document.getElementById('account-name').value.trim(),
      about: document.getElementById('account-about').value.trim(),
      picture: document.getElementById('account-picture').value.trim(),
      nip05: document.getElementById('account-nip05').value.trim()
    };
    
    try {
      await NostrSettings.sendMessage('createAccount', accountData);
      
      // Close dialog and refresh
      document.getElementById('account-creation-dialog').close();
      this.resetWizard();
      NostrSettings.loadSettings();
      NostrSettings.showSuccess('Account created successfully');
    } catch (error) {
      console.error('Failed to create account:', error);
      NostrSettings.showError('Failed to create account');
    }
  },
  
  /**
   * Reset wizard to initial state
   */
  resetWizard() {
    this.currentStep = 1;
    this.generatedKeys = null;
    this.updateWizardUI();
    
    // Clear form fields
    document.getElementById('account-name').value = '';
    document.getElementById('account-about').value = '';
    document.getElementById('account-picture').value = '';
    document.getElementById('account-nip05').value = '';
    
    // Clear checkboxes
    document.getElementById('backup-downloaded').checked = false;
    document.getElementById('backup-written').checked = false;
    document.getElementById('backup-password-manager').checked = false;
  },
  
  /**
   * Switch import tab
   * @param {string} tab
   */
  switchImportTab(tab) {
    // Update tab buttons
    document.querySelectorAll('.tab-button').forEach(button => {
      button.classList.toggle('active', button.dataset.tab === tab);
    });
    
    // Update panels
    document.querySelectorAll('.import-panel').forEach(panel => {
      panel.hidden = true;
    });
    document.getElementById(`import-${tab}`).hidden = false;
  },
  
  /**
   * Import account
   */
  async importAccount() {
    const activeTab = document.querySelector('.tab-button.active').dataset.tab;
    let privateKey = null;
    
    try {
      switch (activeTab) {
        case 'text':
          privateKey = document.getElementById('import-nsec').value.trim();
          if (!privateKey) {
            NostrSettings.showError('Please enter a private key');
            return;
          }
          break;
          
        case 'file':
          if (!this.importedKey) {
            NostrSettings.showError('Please select a file first');
            return;
          }
          privateKey = this.importedKey;
          break;
          
        case 'qr':
          // QR import handled separately
          NostrSettings.showError('QR scanning not yet implemented');
          return;
      }
      
      // Import the account
      await NostrSettings.sendMessage('importAccount', { privateKey });
      
      // Close dialog and refresh
      document.getElementById('import-account-dialog').close();
      this.resetImport();
      NostrSettings.loadSettings();
      NostrSettings.showSuccess('Account imported successfully');
    } catch (error) {
      console.error('Failed to import account:', error);
      NostrSettings.showError('Failed to import account: ' + error.message);
    }
  },
  
  /**
   * Reset import dialog
   */
  resetImport() {
    document.getElementById('import-nsec').value = '';
    document.getElementById('import-file-input').value = '';
    document.getElementById('import-file-password').value = '';
    document.getElementById('file-password-group').hidden = true;
    this.importedKey = null;
    this.switchImportTab('text');
  },
  
  /**
   * Handle file selection for import
   * @param {Event} event
   */
  async handleFileSelect(event) {
    const file = event.target.files[0];
    if (!file) return;
    
    try {
      const content = await this.readFile(file);
      const data = JSON.parse(content);
      
      if (data.encrypted) {
        // Show password field
        document.getElementById('file-password-group').hidden = false;
        // TODO: Implement decryption
      } else if (data.privateKey || data.nsec) {
        this.importedKey = data.privateKey || data.nsec;
      }
    } catch (error) {
      console.error('Failed to read file:', error);
      NostrSettings.showError('Failed to read backup file');
    }
  },
  
  /**
   * Read file as text
   * @param {File} file
   * @returns {Promise<string>}
   */
  readFile(file) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = e => resolve(e.target.result);
      reader.onerror = reject;
      reader.readAsText(file);
    });
  },
  
  /**
   * Switch to a different account
   * @param {string} pubkey
   */
  async switchAccount(pubkey) {
    try {
      await NostrSettings.sendMessage('switchAccount', { pubkey });
      NostrSettings.loadSettings();
      NostrSettings.showSuccess('Switched account');
    } catch (error) {
      console.error('Failed to switch account:', error);
      NostrSettings.showError('Failed to switch account');
    }
  },
  
  /**
   * Edit account
   * @param {string} pubkey
   */
  editAccount(pubkey) {
    // TODO: Implement account editing
    NostrSettings.showInfo('Account editing coming soon');
  },
  
  /**
   * Export account
   * @param {string} pubkey
   */
  async exportAccount(pubkey) {
    try {
      const data = await NostrSettings.sendMessage('exportAccount', { pubkey });
      this.downloadJSON(data, `nostr-account-${pubkey.slice(0, 8)}.json`);
      NostrSettings.showSuccess('Account exported');
    } catch (error) {
      console.error('Failed to export account:', error);
      NostrSettings.showError('Failed to export account');
    }
  },
  
  /**
   * Delete account
   * @param {string} pubkey
   */
  async deleteAccount(pubkey) {
    if (!confirm('Are you sure you want to delete this account? This cannot be undone.')) {
      return;
    }
    
    try {
      await NostrSettings.sendMessage('deleteAccount', { pubkey });
      NostrSettings.loadSettings();
      NostrSettings.showSuccess('Account deleted');
    } catch (error) {
      console.error('Failed to delete account:', error);
      NostrSettings.showError('Failed to delete account');
    }
  },
  
  /**
   * Download backup file
   */
  downloadBackup() {
    const backupData = {
      version: 1,
      created: new Date().toISOString(),
      encrypted: false,
      privateKey: this.generatedKeys.privkey,
      publicKey: this.generatedKeys.pubkey,
      nsec: this.generatedKeys.nsec,
      npub: this.generatedKeys.npub
    };
    
    this.downloadJSON(backupData, 'nostr-account-backup.json');
  },
  
  /**
   * Download JSON data as file
   * @param {Object} data
   * @param {string} filename
   */
  downloadJSON(data, filename) {
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  },
  
  /**
   * Toggle visibility of sensitive field
   * @param {string} targetId
   */
  toggleVisibility(targetId) {
    const element = document.getElementById(targetId);
    const isPassword = element.type === 'password';
    
    if (isPassword) {
      element.type = 'text';
    } else if (element.classList.contains('masked')) {
      // Handle masked divs
      const value = element.dataset.value;
      if (value) {
        element.textContent = value;
        element.classList.remove('masked');
      }
    } else {
      if (element.type === 'text') {
        element.type = 'password';
      } else {
        element.innerHTML = '<span class="masked-content">••••••••••••••••••••</span>';
        element.classList.add('masked');
      }
    }
  },
  
  /**
   * Copy text to clipboard
   * @param {string} elementId
   */
  async copyToClipboard(elementId) {
    const element = document.getElementById(elementId);
    let text = element.textContent || element.value;
    
    // Handle masked values
    if (element.dataset.value) {
      text = element.dataset.value;
    }
    
    try {
      await navigator.clipboard.writeText(text);
      NostrSettings.showSuccess('Copied to clipboard');
    } catch (error) {
      console.error('Failed to copy:', error);
      NostrSettings.showError('Failed to copy to clipboard');
    }
  },
  
  /**
   * Start QR scanner
   */
  async startQRScanner() {
    // TODO: Implement QR scanning
    NostrSettings.showInfo('QR scanning coming soon');
  },
  
  /**
   * Get initial letter for avatar
   * @param {string} name
   * @returns {string}
   */
  getInitial(name) {
    if (!name) return 'N';
    return name.charAt(0).toUpperCase();
  },
  
  /**
   * Truncate pubkey for display
   * @param {string} pubkey
   * @returns {string}
   */
  truncatePubkey(pubkey) {
    if (!pubkey) return '';
    if (pubkey.length <= 16) return pubkey;
    return pubkey.slice(0, 8) + '...' + pubkey.slice(-8);
  }
};

// Export for use in main settings file
window.AccountManager = AccountManager;