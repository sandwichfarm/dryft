// alby-sdk v3.0.0
// Placeholder for dryft
// Full library: https://github.com/getAlby/sdk

(function(global, factory) {
  if (typeof exports === 'object' && typeof module !== 'undefined') {
    module.exports = factory();
  } else if (typeof define === 'function' && define.amd) {
    define(factory);
  } else {
    global.albySDK = factory();
  }
}(typeof self !== 'undefined' ? self : this, function() {
  'use strict';

  class AlbyClient {
    constructor(options = {}) {
      this.apiUrl = options.apiUrl || 'https://api.getalby.com';
      this.token = options.token;
    }

    // Check if WebLN is available
    async isWebLNAvailable() {
      return typeof window.webln !== 'undefined';
    }

    // Enable WebLN
    async enableWebLN() {
      if (!window.webln) {
        throw new Error('WebLN not available');
      }
      return window.webln.enable();
    }

    // Send payment via WebLN
    async sendPayment(invoice) {
      if (!window.webln) {
        throw new Error('WebLN not available');
      }
      return window.webln.sendPayment(invoice);
    }

    // Make invoice
    async makeInvoice(amount, memo) {
      if (!window.webln) {
        throw new Error('WebLN not available');
      }
      return window.webln.makeInvoice({ amount, defaultMemo: memo });
    }

    // Sign message with Nostr
    async signMessage(message) {
      if (window.nostr && window.nostr.signEvent) {
        const event = {
          kind: 1,
          content: message,
          created_at: Math.floor(Date.now() / 1000),
          tags: []
        };
        return window.nostr.signEvent(event);
      }
      throw new Error('Nostr signing not available');
    }
  }

  // NWC (Nostr Wallet Connect) support
  class NWC {
    constructor(options = {}) {
      this.relayUrl = options.relayUrl;
      this.walletPubkey = options.walletPubkey;
    }

    async connect() {
      // Placeholder for NWC connection
      console.log('NWC connect:', this.relayUrl);
      return true;
    }

    async payInvoice(invoice) {
      // Placeholder for NWC payment
      console.log('NWC pay invoice:', invoice);
      return { preimage: 'placeholder' };
    }
  }

  return {
    AlbyClient,
    NWC,
    version: '3.0.0'
  };
}))