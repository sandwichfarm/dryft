# UI/UX Mockups and User Flows
## dryft browser - User Interface Design and Experience

### 1. Core UI Components Overview

```
┌─────────────────────────────────────────────────────────────┐
│ dryft browser - Main Window                              │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ ←  →  ⟳  🏠  [nostr://npub1a2b3c...         ] ⚡ 🔔 ⋮  │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                               │
│ ┌───────────────────────────────────────────────────────┐   │
│ │                    Web Content Area                     │   │
│ │                                                         │   │
│ │              ┌─────────────────────────┐               │   │
│ │              │   Nostr-enabled Site    │               │   │
│ │              │                         │               │   │
│ │              │  [window.nostr active]  │               │   │
│ │              └─────────────────────────┘               │   │
│ │                                                         │   │
│ └───────────────────────────────────────────────────────┘   │
│                                                               │
│ Status Bar: Connected to 5 relays | Local relay active       │
└─────────────────────────────────────────────────────────────┘

Legend:
⚡ = Nostr Status Indicator (click for menu)
🔔 = Nostr Notifications
⋮  = Browser Menu
```

### 2. Nostr Status Indicator States

```
┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│     ⚡      │  │     ⚡      │  │     ⚡      │  │     ⚡      │
│   (green)   │  │  (yellow)   │  │    (red)    │  │   (gray)    │
├─────────────┤  ├─────────────┤  ├─────────────┤  ├─────────────┤
│  Connected  │  │  Degraded   │  │   Offline   │  │  Disabled   │
│ All relays  │  │ Some relays │  │  No relays  │  │ User choice │
└─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘

Click Behavior:
┌─────────────────────────────────┐
│ Nostr Status Menu               │
├─────────────────────────────────┤
│ ✓ Connected to 5/8 relays       │
│ ○ Local relay (port 8081)       │
│ • Blossom server active         │
│ ─────────────────────────────── │
│ 👤 alice@nostr.com              │
│    npub1a2b3c4d5e...           │
│ ─────────────────────────────── │
│ ⚙️ Nostr Settings...            │
│ 🔑 Manage Keys...               │
│ 📊 Connection Details...        │
└─────────────────────────────────┘
```

### 3. First-Time Setup Flow

```
Welcome Screen:
┌─────────────────────────────────────────────────────────┐
│                                                         │
│              Welcome to dryft browser                │
│                                                         │
│         Your gateway to the decentralized web           │
│                                                         │
│    ┌───────────────────┐  ┌───────────────────┐       │
│    │   Get Started →   │  │  Import Existing  │       │
│    └───────────────────┘  └───────────────────┘       │
│                                                         │
│              [ ] Skip Nostr setup for now               │
│                                                         │
└─────────────────────────────────────────────────────────┘
                           ↓
Key Generation:
┌─────────────────────────────────────────────────────────┐
│                                                         │
│              Create Your Nostr Identity                 │
│                                                         │
│  ┌─────────────────────────────────────────────────┐  │
│  │ Generating secure key pair...                    │  │
│  │ ████████████████░░░░░░░░░  67%                  │  │
│  └─────────────────────────────────────────────────┘  │
│                                                         │
│  🔐 Your keys are being securely generated locally     │
│                                                         │
└─────────────────────────────────────────────────────────┘
                           ↓
Backup Prompt:
┌─────────────────────────────────────────────────────────┐
│                                                         │
│              Backup Your Recovery Phrase                │
│                                                         │
│  ┌─────────────────────────────────────────────────┐  │
│  │  1. ancient   5. forest    9. scatter           │  │
│  │  2. marine    6. grab     10. tissue            │  │
│  │  3. better    7. harvest  11. universe          │  │
│  │  4. domain    8. noble    12. witness           │  │
│  └─────────────────────────────────────────────────┘  │
│                                                         │
│  ⚠️  Write these words down and store them safely      │
│                                                         │
│  [ ] I have written down my recovery phrase            │
│                                                         │
│         [ Back ]            [ Continue ]               │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 4. NIP-07 Permission Dialog

```
When a website requests window.nostr access:
┌─────────────────────────────────────────────────────────┐
│ example.com wants to access your Nostr identity        │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  🌐 example.com                                        │
│                                                         │
│  This site is requesting permission to:                │
│                                                         │
│  ✓ View your public key                               │
│  ✓ Sign events on your behalf                         │
│  ✓ Access your relay list                             │
│                                                         │
│  ┌─────────────────────────────────────────────────┐  │
│  │ ⚠️  Only grant access to sites you trust         │  │
│  └─────────────────────────────────────────────────┘  │
│                                                         │
│  [ ] Remember this decision                            │
│                                                         │
│      [ Deny ]    [ Allow Once ]    [ Always Allow ]    │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 5. Nostr Settings Page

```
┌─────────────────────────────────────────────────────────────┐
│ Settings > Nostr                                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ Accounts ─────────────────────────────────────────────────  │
│                                                             │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ 👤 alice@nostr.com                          Active ✓ │    │
│ │    npub1a2b3c4d5e6f7g8h9i0j1k2l3m4n5o6p7...        │    │
│ │    Created: 2024-01-15                              │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ 👤 bob.nostr                                        │    │
│ │    npub1q2w3e4r5t6y7u8i9o0p1a2s3d4f5g6h7...        │    │
│ │    Created: 2024-02-20                              │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ [ + Add Account ]  [ Import ]  [ Export ]                   │
│                                                             │
│ Relays ───────────────────────────────────────────────────  │
│                                                             │
│ Default Relays:                                             │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ wss://relay.damus.io                    Connected ✓ │    │
│ │ wss://nos.lol                           Connected ✓ │    │
│ │ wss://relay.nostr.band                  Connecting  │    │
│ │ wss://nostr.wine                        Failed ✗    │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ [ + Add Relay ]                                             │
│                                                             │
│ Local Services ──────────────────────────────────────────   │
│                                                             │
│ [ ] Enable Local Relay (Port: [8081])                      │
│     Cache events locally for offline access                 │
│                                                             │
│ [ ] Enable Blossom Server (Port: [8080])                   │
│     Host and cache content-addressed files                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6. Key Management Interface

```
Key Manager:
┌─────────────────────────────────────────────────────────────┐
│ Nostr Key Manager                                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ Current Account: alice@nostr.com                            │
│                                                             │
│ Public Key:                                                 │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ npub1a2b3c4d5e6f7g8h9i0j1k2l3m4n5o6p7q8r9s0t...   │    │
│ │                                          [ Copy ]    │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ Private Key: •••••••••••••••••••••••••••••••••••••••      │
│              [ Show ] [ Export ] [ Change Password ]        │
│                                                             │
│ Security ────────────────────────────────────────────────   │
│                                                             │
│ Password Protection: Enabled ✓                              │
│ Auto-lock after: [5 minutes ▼]                            │
│ Require password for signing: [ ]                          │
│                                                             │
│ Backup ──────────────────────────────────────────────────   │
│                                                             │
│ Last backup: 2024-03-15 14:32                             │
│ [ Backup Now ] [ View Recovery Phrase ]                    │
│                                                             │
│ Danger Zone ─────────────────────────────────────────────   │
│                                                             │
│ [ Delete Account ]                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 7. nostr:// URL Navigation

```
URL Bar Behavior:
┌─────────────────────────────────────────────────────────┐
│ [nostr://npub1a2b3c4d5e...                         ] 🔍 │
└─────────────────────────────────────────────────────────┘
                           ↓
Profile View:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  ┌─────┐  Alice Smith                                      │
│  │ 👤  │  @alice@nostr.com                                │
│  └─────┘  Following: 123 | Followers: 456                  │
│                                                             │
│  About: Decentralization enthusiast, developer, and        │
│  coffee addict. Building the future of social media.       │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ Recent Notes                                         │  │
│  ├─────────────────────────────────────────────────────┤  │
│  │ Just shipped a new update to my Nostr client! 🚀    │  │
│  │ 2 hours ago · 42 ⚡ · 15 💬                         │  │
│  ├─────────────────────────────────────────────────────┤  │
│  │ Thoughts on the latest NIP proposals...             │  │
│  │ 5 hours ago · 28 ⚡ · 7 💬                          │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Event View:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Alice Smith @alice@nostr.com · 2 hours ago                │
│                                                             │
│  Just shipped a new update to my Nostr client! 🚀          │
│  Check it out at https://example.com/download              │
│                                                             │
│  #nostr #development #opensource                            │
│                                                             │
│  ─────────────────────────────────────────────────────────  │
│                                                             │
│  42 ⚡ Reactions    15 💬 Comments    5 🔄 Reposts        │
│                                                             │
│  [ ⚡ React ] [ 💬 Comment ] [ 🔄 Repost ] [ ⋮ More ]    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 8. Nsite Navigation

```
Nsite URL:
┌─────────────────────────────────────────────────────────┐
│ [nostr://nsite/alice.com                           ] 🔍 │
└─────────────────────────────────────────────────────────┘
                           ↓
Loading State:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│              Loading Decentralized Website                  │
│                                                             │
│         Fetching site metadata from Nostr...               │
│         ████████████░░░░░░░░░░░░░░░░  45%                 │
│                                                             │
│         Resolving: alice.com → npub1a2b3c...              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                           ↓
Rendered Site:
┌─────────────────────────────────────────────────────────────┐
│ 🔒 npub1a2b3c.nsite.localhost     [ ⓘ Site Info ]         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│            Alice's Decentralized Website                    │
│                                                             │
│  Home | Blog | Projects | Contact                          │
│                                                             │
│  Welcome to my censorship-resistant website!               │
│  This site is hosted entirely on Nostr and IPFS.          │
│                                                             │
│  Latest Posts:                                             │
│  • Building on Nostr: A Developer's Guide                  │
│  • Why Decentralization Matters                            │
│  • My Journey into Web3                                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 9. Connection Status Details

```
Connection Details Dialog:
┌─────────────────────────────────────────────────────────────┐
│ Nostr Connection Details                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ Relay Connections ──────────────────────────────────────    │
│                                                             │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ wss://relay.damus.io                                │    │
│ │ Status: Connected ✓                                 │    │
│ │ Latency: 45ms                                       │    │
│ │ Messages: ↓ 1,234 | ↑ 567                         │    │
│ │ Subscriptions: 5 active                            │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ wss://nos.lol                                       │    │
│ │ Status: Connected ✓                                 │    │
│ │ Latency: 23ms                                       │    │
│ │ Messages: ↓ 2,456 | ↑ 789                         │    │
│ │ Subscriptions: 3 active                            │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ Local Services ─────────────────────────────────────────    │
│                                                             │
│ Local Relay: Active (Port 8081)                            │
│ • Events cached: 15,234                                    │
│ • Storage used: 45.2 MB                                    │
│                                                             │
│ Blossom Server: Active (Port 8080)                         │
│ • Files cached: 234                                        │
│ • Storage used: 128.5 MB                                   │
│                                                             │
│                                     [ Close ]              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 10. Event Signing Flow

```
Signing Request:
┌─────────────────────────────────────────────────────────────┐
│ Sign Nostr Event                                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ 🌐 social.example.com requests to sign:                    │
│                                                             │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ Event Type: Text Note (kind 1)                      │    │
│ │                                                      │    │
│ │ Content:                                             │    │
│ │ "Hello Nostr! This is my first post."              │    │
│ │                                                      │    │
│ │ Tags:                                                │    │
│ │ • p: npub1xyz... (replying to Alice)               │    │
│ │                                                      │    │
│ │ Created: 2024-03-20 15:42:30                       │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ [ ] Always allow social.example.com to sign text notes     │
│                                                             │
│         [ Cancel ]              [ Sign Event ]             │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Password Prompt (if enabled):
┌─────────────────────────────────────────────────────────────┐
│ Enter Password to Sign                                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ Enter your password to sign this event:                    │
│                                                             │
│ ┌─────────────────────────────────────────────────────┐    │
│ │ ••••••••••••••••                                    │    │
│ └─────────────────────────────────────────────────────┘    │
│                                                             │
│ [ ] Remember for 5 minutes                                 │
│                                                             │
│         [ Cancel ]                    [ Sign ]             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 11. Error States

```
Relay Connection Error:
┌─────────────────────────────────────────────────────────────┐
│ ⚠️  Connection Problem                                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ Unable to connect to Nostr relays.                         │
│ Your messages will be queued and sent when                 │
│ the connection is restored.                                │
│                                                             │
│ Error Details:                                             │
│ • wss://relay.damus.io - Timeout                          │
│ • wss://nos.lol - Connection refused                      │
│                                                             │
│ [ Try Again ]  [ Work Offline ]  [ Settings ]             │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Invalid Nostr URL:
┌─────────────────────────────────────────────────────────────┐
│ Invalid Nostr URL                                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ The URL you entered doesn't appear to be valid:           │
│                                                             │
│ nostr://invalid-format                                     │
│                                                             │
│ Nostr URLs should be in one of these formats:             │
│ • nostr://npub1...  (profile)                             │
│ • nostr://note1...  (note)                                │
│ • nostr://nprofile1... (profile with relays)             │
│ • nostr://nevent1... (event with relays)                  │
│ • nostr://nsite/identifier (static website)               │
│                                                             │
│                                     [ OK ]                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 12. Mobile Responsive Design

```
Mobile View (360px width):
┌─────────────────────┐
│ ≡  Tungsten    ⚡ ⋮ │
├─────────────────────┤
│ ┌─────────────────┐ │
│ │ URL Bar         │ │
│ └─────────────────┘ │
│                     │
│ ┌─────────────────┐ │
│ │                 │ │
│ │  Nostr Content  │ │
│ │                 │ │
│ │  Responsive     │ │
│ │  Layout         │ │
│ │                 │ │
│ └─────────────────┘ │
│                     │
│ [←] [→] [⟳] [🏠] [⊞]│
└─────────────────────┘

Collapsed Menu:
┌─────────────────────┐
│ ≡ Menu              │
├─────────────────────┤
│ 👤 alice@nostr     │
│ ⚡ Relay Status    │
│ 🔑 Manage Keys     │
│ ⚙️ Settings        │
│ ─────────────────── │
│ 📊 Connection Info │
│ 💾 Local Services  │
└─────────────────────┘
```

### 13. Accessibility Features

```
Screen Reader Announcements:
- "Nostr status: Connected to 5 relays"
- "Website requests Nostr access. Dialog"
- "Sign event dialog. Event type: Text note"
- "Password required to sign event"

Keyboard Navigation:
- Tab: Navigate between UI elements
- Enter: Activate buttons/links
- Escape: Cancel dialogs
- Ctrl+Shift+N: Open Nostr status menu
- Ctrl+Shift+K: Open key manager

High Contrast Mode:
┌─────────────────────────────────────┐
│ High Contrast UI                    │
├─────────────────────────────────────┤
│ ■ Clear visual hierarchy            │
│ ■ Strong color contrast (WCAG AA)   │
│ ■ Larger touch targets (44x44px)    │
│ ■ Clear focus indicators            │
│ ■ Reduced motion options            │
└─────────────────────────────────────┘
```

### 14. User Onboarding Flow

```
Step 1: Introduction
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│              What is Nostr?                                 │
│                                                             │
│  Nostr is a decentralized protocol that gives you:         │
│                                                             │
│  ✓ Full ownership of your identity                         │
│  ✓ Censorship-resistant communication                      │
│  ✓ No central authority                                    │
│  ✓ Portable social graph                                   │
│                                                             │
│  dryft browser makes it easy to use Nostr              │
│  while browsing the web.                                   │
│                                                             │
│              [ Skip ]         [ Next → ]                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Step 2: Key Concepts
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│              Understanding Keys                             │
│                                                             │
│  🔑 Your identity on Nostr is a cryptographic key pair:   │
│                                                             │
│  Public Key (npub...):                                     │
│  • Your identity that others can see                       │
│  • Safe to share                                           │
│                                                             │
│  Private Key (nsec...):                                    │
│  • Your password to the Nostr network                      │
│  • Never share this with anyone!                           │
│  • Tungsten stores this securely for you                   │
│                                                             │
│           [ ← Back ]         [ Next → ]                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘

Step 3: Try It Out
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│              Try Nostr!                                     │
│                                                             │
│  Visit these Nostr-enabled sites to get started:           │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ 🌐 social.example.com                               │  │
│  │    A decentralized social network                   │  │
│  │    [ Visit → ]                                       │  │
│  ├─────────────────────────────────────────────────────┤  │
│  │ 🌐 marketplace.nostr                                │  │
│  │    P2P marketplace using Nostr                      │  │
│  │    [ Visit → ]                                       │  │
│  ├─────────────────────────────────────────────────────┤  │
│  │ 🌐 blog.nsite.example                               │  │
│  │    A blog hosted on Nostr                           │  │
│  │    [ Visit → ]                                       │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                             │
│           [ ← Back ]         [ Done ✓ ]                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 15. Performance Indicators

```
Loading States:
┌─────────────────────────────────────────────────────────┐
│ Relay Connection Status                                 │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ Connecting to relays...                                │
│                                                         │
│ ⠋ wss://relay.damus.io     (connecting...)            │
│ ✓ wss://nos.lol            (23ms)                     │
│ ⠙ wss://relay.nostr.band   (connecting...)            │
│ ✗ wss://nostr.wine         (failed)                   │
│                                                         │
│ Connected to 1/4 relays                                │
│                                                         │
└─────────────────────────────────────────────────────────┘

Event Broadcasting:
┌─────────────────────────────────────────────────────────┐
│ Publishing Event                                        │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ Sending to relays...                                   │
│                                                         │
│ ████████████████████░░░░░  4/5 relays                 │
│                                                         │
│ ✓ Sent to relay.damus.io                              │
│ ✓ Sent to nos.lol                                     │
│ ✓ Sent to relay.nostr.band                            │
│ ✓ Sent to nostr.wine                                  │
│ ⠸ Sending to umbrel.local                             │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 16. Design System

```
Color Palette:
┌─────────────────────────────────────────────────────────┐
│ Primary Colors                                          │
├─────────────────────────────────────────────────────────┤
│ ████ Purple      #7C3AED  - Primary actions            │
│ ████ Light Purple #A78BFA  - Hover states              │
│ ████ Dark Purple  #5B21B6  - Active states             │
├─────────────────────────────────────────────────────────┤
│ Status Colors                                           │
├─────────────────────────────────────────────────────────┤
│ ████ Green       #10B981  - Connected/Success          │
│ ████ Yellow      #F59E0B  - Warning/Degraded           │
│ ████ Red         #EF4444  - Error/Offline              │
│ ████ Gray        #6B7280  - Disabled/Inactive          │
├─────────────────────────────────────────────────────────┤
│ Neutral Colors                                          │
├─────────────────────────────────────────────────────────┤
│ ████ Background  #FFFFFF  - Main background            │
│ ████ Surface     #F9FAFB  - Card backgrounds           │
│ ████ Border      #E5E7EB  - Borders and dividers      │
│ ████ Text        #111827  - Primary text               │
│ ████ Text Muted  #6B7280  - Secondary text             │
└─────────────────────────────────────────────────────────┘

Typography:
┌─────────────────────────────────────────────────────────┐
│ Font Stack                                              │
├─────────────────────────────────────────────────────────┤
│ Headers:    Inter, system-ui, sans-serif               │
│ Body:       Inter, system-ui, sans-serif               │
│ Monospace:  'SF Mono', Monaco, monospace               │
│                                                         │
│ Scale:                                                  │
│ H1: 32px/40px  - Page titles                          │
│ H2: 24px/32px  - Section headers                      │
│ H3: 20px/28px  - Subsections                          │
│ Body: 16px/24px - Default text                        │
│ Small: 14px/20px - Secondary text                     │
│ Tiny: 12px/16px  - Captions                           │
└─────────────────────────────────────────────────────────┘

Component Spacing:
┌─────────────────────────────────────────────────────────┐
│ Spacing System (8px base)                               │
├─────────────────────────────────────────────────────────┤
│ xs:  4px  - Tight spacing                              │
│ sm:  8px  - Small elements                             │
│ md:  16px - Default spacing                            │
│ lg:  24px - Section spacing                            │
│ xl:  32px - Large sections                             │
│ 2xl: 48px - Page sections                              │
└─────────────────────────────────────────────────────────┘
```

### 17. Animation and Transitions

```
Loading Animation:
@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

Connection Indicator:
@keyframes ping {
  75%, 100% {
    transform: scale(2);
    opacity: 0;
  }
}

Transition Timing:
- Hover effects: 150ms ease
- Modal open/close: 200ms ease-out
- Page transitions: 300ms ease-in-out
- Loading states: 400ms ease
```

### 18. Responsive Breakpoints

```
Mobile:    320px  - 768px
Tablet:    768px  - 1024px
Desktop:   1024px - 1280px
Wide:      1280px+

Key Adjustments:
- Mobile: Stack UI elements vertically
- Tablet: 2-column layouts where appropriate
- Desktop: Full feature set with sidebars
- Wide: Centered content with max-width
```