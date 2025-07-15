// Copyright 2024 The Tungsten Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// Migration phases
const Phase = {
  DETECTION: 'detection',
  SELECTION: 'selection',
  PREVIEW: 'preview',
  PROGRESS: 'progress',
  COMPLETE: 'complete',
  ERROR: 'error'
};

class MigrationPage {
  constructor() {
    this.currentPhase = Phase.DETECTION;
    this.detectedExtensions = [];
    this.selectedExtension = null;
    this.migrationData = null;
    
    this.initializeElements();
    this.attachEventListeners();
  }
  
  initializeElements() {
    // Phase containers
    this.phases = {
      detection: document.getElementById('detection-phase'),
      selection: document.getElementById('selection-phase'),
      preview: document.getElementById('preview-phase'),
      progress: document.getElementById('progress-phase'),
      complete: document.getElementById('complete-phase'),
      error: document.getElementById('error-phase')
    };
    
    // Buttons
    this.scanButton = document.getElementById('scan-button');
    this.backButton = document.getElementById('back-button');
    this.importButton = document.getElementById('import-button');
    this.cancelPreviewButton = document.getElementById('cancel-preview-button');
    this.confirmImportButton = document.getElementById('confirm-import-button');
    this.finishButton = document.getElementById('finish-button');
    this.retryButton = document.getElementById('retry-button');
    this.closeButton = document.getElementById('close-button');
    
    // Status elements
    this.scanStatus = document.getElementById('scan-status');
    this.extensionList = document.getElementById('extension-list');
    this.previewContent = document.getElementById('preview-content');
    this.progressFill = document.getElementById('progress-fill');
    this.progressStatus = document.getElementById('progress-status');
    this.resultSummary = document.getElementById('result-summary');
    this.disableExtensionOption = document.getElementById('disable-extension-option');
    this.disableExtensionCheckbox = document.getElementById('disable-extension-checkbox');
    this.errorMessage = document.getElementById('error-message');
  }
  
  attachEventListeners() {
    this.scanButton.addEventListener('click', () => this.scanForExtensions());
    this.backButton.addEventListener('click', () => this.showPhase(Phase.DETECTION));
    this.importButton.addEventListener('click', () => this.startImport());
    this.cancelPreviewButton.addEventListener('click', () => this.showPhase(Phase.SELECTION));
    this.confirmImportButton.addEventListener('click', () => this.confirmImport());
    this.finishButton.addEventListener('click', () => this.finish());
    this.retryButton.addEventListener('click', () => this.showPhase(Phase.DETECTION));
    this.closeButton.addEventListener('click', () => window.close());
  }
  
  showPhase(phase) {
    // Hide all phases
    Object.values(this.phases).forEach(el => el.hidden = true);
    
    // Show requested phase
    if (this.phases[phase]) {
      this.phases[phase].hidden = false;
      this.currentPhase = phase;
    }
  }
  
  async scanForExtensions() {
    this.scanButton.disabled = true;
    this.scanStatus.hidden = false;
    this.scanStatus.textContent = 'Scanning for Nostr extensions...';
    
    try {
      this.detectedExtensions = await sendWithPromise('detectNostrExtensions');
      
      if (this.detectedExtensions.length === 0) {
        this.scanStatus.textContent = 'No Nostr extensions found.';
        this.scanButton.disabled = false;
      } else {
        this.showExtensionList();
      }
    } catch (error) {
      this.showError('Failed to scan for extensions: ' + error.message);
      this.scanButton.disabled = false;
    }
  }
  
  showExtensionList() {
    this.extensionList.innerHTML = '';
    
    this.detectedExtensions.forEach(ext => {
      const item = document.createElement('div');
      item.className = 'extension-item';
      item.dataset.extensionId = ext.id;
      
      const checkbox = document.createElement('input');
      checkbox.type = 'radio';
      checkbox.name = 'extension';
      checkbox.className = 'extension-checkbox';
      checkbox.value = ext.id;
      
      const info = document.createElement('div');
      info.className = 'extension-info';
      
      const name = document.createElement('div');
      name.className = 'extension-name';
      name.textContent = ext.name;
      
      const version = document.createElement('div');
      version.className = 'extension-version';
      version.textContent = `Version ${ext.version}`;
      
      if (!ext.isEnabled) {
        const status = document.createElement('span');
        status.className = 'extension-status';
        status.textContent = '(Disabled)';
        name.appendChild(status);
      }
      
      info.appendChild(name);
      info.appendChild(version);
      
      item.appendChild(checkbox);
      item.appendChild(info);
      
      item.addEventListener('click', () => {
        checkbox.checked = true;
        this.selectExtension(ext);
      });
      
      checkbox.addEventListener('change', () => {
        if (checkbox.checked) {
          this.selectExtension(ext);
        }
      });
      
      this.extensionList.appendChild(item);
    });
    
    this.showPhase(Phase.SELECTION);
    this.importButton.disabled = true;
  }
  
  selectExtension(extension) {
    // Update visual selection
    document.querySelectorAll('.extension-item').forEach(item => {
      item.classList.toggle('selected', item.dataset.extensionId === extension.id);
    });
    
    this.selectedExtension = extension;
    this.importButton.disabled = false;
  }
  
  async startImport() {
    if (!this.selectedExtension) return;
    
    this.importButton.disabled = true;
    
    try {
      // Read data from the selected extension
      this.migrationData = await sendWithPromise('readExtensionData', this.selectedExtension);
      
      if (!this.migrationData.success) {
        throw new Error(this.migrationData.errorMessage || 'Failed to read extension data');
      }
      
      this.showPreview();
    } catch (error) {
      this.showError('Failed to read extension data: ' + error.message);
      this.importButton.disabled = false;
    }
  }
  
  showPreview() {
    this.previewContent.innerHTML = '';
    
    // Keys section
    if (this.migrationData.keys && this.migrationData.keys.length > 0) {
      const keysSection = this.createPreviewSection('Keys to Import', 
        this.migrationData.keys.map(k => k.name || 'Unnamed Account'));
      this.previewContent.appendChild(keysSection);
    }
    
    // Relays section
    if (this.migrationData.relayUrls && this.migrationData.relayUrls.length > 0) {
      const relaysSection = this.createPreviewSection('Relay URLs', 
        this.migrationData.relayUrls);
      this.previewContent.appendChild(relaysSection);
    }
    
    // Permissions section
    if (this.migrationData.permissions && this.migrationData.permissions.length > 0) {
      const permissionsSection = this.createPreviewSection('Permissions', 
        this.migrationData.permissions.map(p => 
          `${p.origin}: ${p.allowedMethods.join(', ')}`));
      this.previewContent.appendChild(permissionsSection);
    }
    
    this.showPhase(Phase.PREVIEW);
  }
  
  createPreviewSection(title, items) {
    const section = document.createElement('div');
    section.className = 'preview-section';
    
    const heading = document.createElement('h3');
    heading.textContent = title;
    section.appendChild(heading);
    
    const list = document.createElement('ul');
    items.forEach(item => {
      const li = document.createElement('li');
      li.textContent = item;
      list.appendChild(li);
    });
    section.appendChild(list);
    
    return section;
  }
  
  async confirmImport() {
    this.showPhase(Phase.PROGRESS);
    
    try {
      const result = await sendWithPromise('performMigration', {
        extension: this.selectedExtension,
        data: this.migrationData
      });
      
      // Update progress as we receive updates
      addWebUIListener('migration-progress', (completed, total, current) => {
        this.updateProgress(completed, total, current);
      });
      
      if (result.success) {
        this.showComplete(result);
      } else {
        throw new Error(result.message || 'Migration failed');
      }
    } catch (error) {
      this.showError('Migration failed: ' + error.message);
    }
  }
  
  updateProgress(completed, total, current) {
    const percentage = (completed / total) * 100;
    this.progressFill.style.width = `${percentage}%`;
    this.progressStatus.textContent = current;
  }
  
  showComplete(result) {
    this.resultSummary.innerHTML = '';
    
    // Show summary of imported items
    const items = [];
    if (this.migrationData.keys && this.migrationData.keys.length > 0) {
      items.push(`${this.migrationData.keys.length} key(s)`);
    }
    if (this.migrationData.relayUrls && this.migrationData.relayUrls.length > 0) {
      items.push(`${this.migrationData.relayUrls.length} relay(s)`);
    }
    if (this.migrationData.permissions && this.migrationData.permissions.length > 0) {
      items.push(`${this.migrationData.permissions.length} permission(s)`);
    }
    
    items.forEach(item => {
      const resultItem = document.createElement('div');
      resultItem.className = 'result-item';
      
      const icon = document.createElement('div');
      icon.className = 'result-icon success';
      
      const text = document.createElement('div');
      text.textContent = `Imported ${item}`;
      
      resultItem.appendChild(icon);
      resultItem.appendChild(text);
      this.resultSummary.appendChild(resultItem);
    });
    
    // Show disable extension option if applicable
    if (this.selectedExtension.isEnabled) {
      this.disableExtensionOption.hidden = false;
      this.disableExtensionCheckbox.checked = true;
    }
    
    this.showPhase(Phase.COMPLETE);
  }
  
  async finish() {
    // Disable extension if requested
    if (this.disableExtensionCheckbox.checked && this.selectedExtension.isEnabled) {
      try {
        await sendWithPromise('disableExtension', this.selectedExtension);
      } catch (error) {
        console.error('Failed to disable extension:', error);
      }
    }
    
    // Navigate to Nostr settings or close window
    window.location.href = kChromeUINostrSettingsURL;
  }
  
  showError(message) {
    this.errorMessage.textContent = message;
    this.showPhase(Phase.ERROR);
  }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  new MigrationPage();
});