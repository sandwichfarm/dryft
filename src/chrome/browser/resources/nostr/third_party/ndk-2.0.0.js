// NDK (Nostr Development Kit) v2.0.0
// Simplified browser-compatible version for dryft
// Full library available at: https://github.com/nostr-dev-kit/ndk

(function(global, factory) {
  // UMD pattern for compatibility
  if (typeof exports === 'object' && typeof module !== 'undefined') {
    module.exports = factory();
  } else if (typeof define === 'function' && define.amd) {
    define(factory);
  } else {
    global.NDK = factory();
  }
}(typeof self !== 'undefined' ? self : this, function() {
  'use strict';

  // Event class
  class NDKEvent {
    constructor(ndk, event) {
      this.ndk = ndk;
      this.id = event?.id || '';
      this.pubkey = event?.pubkey || '';
      this.created_at = event?.created_at || Math.floor(Date.now() / 1000);
      this.kind = event?.kind || 1;
      this.tags = event?.tags || [];
      this.content = event?.content || '';
      this.sig = event?.sig || '';
    }

    static from(event) {
      return new NDKEvent(null, event);
    }

    async sign(signer) {
      if (!signer && this.ndk?.signer) {
        signer = this.ndk.signer;
      }
      if (!signer) {
        throw new Error('No signer available');
      }
      
      // Use window.nostr if available
      if (!this.pubkey && window.nostr) {
        this.pubkey = await window.nostr.getPublicKey();
      }
      
      const signedEvent = await signer.signEvent(this);
      this.id = signedEvent.id;
      this.sig = signedEvent.sig;
      return this;
    }

    async publish() {
      if (!this.ndk) {
        throw new Error('Event not associated with NDK instance');
      }
      
      // Ensure event is signed
      if (!this.sig) {
        await this.sign();
      }
      
      // Use local relay if available
      if (window.nostr && window.nostr.relay) {
        // TODO: Implement publishing to local relay
        console.log('Publishing to local relay:', this);
      }
      
      return this;
    }
  }

  // Subscription class
  class NDKSubscription {
    constructor(ndk, filters, opts = {}) {
      this.ndk = ndk;
      this.filters = Array.isArray(filters) ? filters : [filters];
      this.opts = opts;
      this.events = new Map();
      this.eventListeners = [];
      this.eoseListeners = [];
    }

    on(event, callback) {
      if (event === 'event') {
        this.eventListeners.push(callback);
      } else if (event === 'eose') {
        this.eoseListeners.push(callback);
      }
      return this;
    }

    start() {
      // Connect to local relay if available
      if (window.nostr && window.nostr.relay) {
        this.connectToLocalRelay();
      }
      return this;
    }

    async connectToLocalRelay() {
      try {
        const events = await window.nostr.relay.query(this.filters[0]);
        events.forEach(event => {
          const ndkEvent = NDKEvent.from(event);
          this.events.set(event.id, ndkEvent);
          this.eventListeners.forEach(cb => cb(ndkEvent, this));
        });
        this.eoseListeners.forEach(cb => cb(this));
      } catch (error) {
        console.error('Failed to query local relay:', error);
      }
    }

    stop() {
      // Clean up subscription
      this.events.clear();
      this.eventListeners = [];
      this.eoseListeners = [];
    }
  }

  // Main NDK class
  class NDK {
    constructor(opts = {}) {
      this.explicitRelayUrls = opts.explicitRelayUrls || [];
      this.signer = opts.signer;
      this.debug = opts.debug || false;
      this._connected = false;
      
      // Check for local relay
      if (window.nostr && window.nostr.relay) {
        const localRelayUrl = window.nostr.relay.getLocalRelayURL();
        if (localRelayUrl && !this.explicitRelayUrls.includes(localRelayUrl)) {
          this.explicitRelayUrls.push(localRelayUrl);
        }
      }
    }

    async connect() {
      // Mark as connected
      this._connected = true;
      
      if (this.debug) {
        console.log('NDK connected to relays:', this.explicitRelayUrls);
      }
      
      return this;
    }

    subscribe(filters, opts) {
      const subscription = new NDKSubscription(this, filters, opts);
      return subscription;
    }

    async fetchEvent(filter) {
      const sub = this.subscribe(filter, { closeOnEose: true });
      
      return new Promise((resolve) => {
        let event = null;
        
        sub.on('event', (e) => {
          event = e;
        });
        
        sub.on('eose', () => {
          sub.stop();
          resolve(event);
        });
        
        sub.start();
      });
    }

    async fetchEvents(filter, opts = {}) {
      const sub = this.subscribe(filter, { ...opts, closeOnEose: true });
      const events = [];
      
      return new Promise((resolve) => {
        sub.on('event', (event) => {
          events.push(event);
        });
        
        sub.on('eose', () => {
          sub.stop();
          resolve(events);
        });
        
        sub.start();
      });
    }
  }

  // Export
  return {
    NDK,
    NDKEvent,
    NDKSubscription,
    version: '2.0.0'
  };
}));