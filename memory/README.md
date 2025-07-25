# dryft Memory Directory

This directory contains all documentation, specifications, and implementation records for the dryft browser project. Files are organized by purpose for easy navigation.

## Directory Structure

### 📐 `/design/` - Architecture & Design Documents
High-level and low-level design documents that define the system architecture:

- **[HLDD.md](design/HLDD.md)** - High-Level Design Document
- **[PRD.md](design/PRD.md)** - Product Requirements Document  
- **[TechnicalSpec.md](design/TechnicalSpec.md)** - Technical Specifications

#### Low-Level Design Documents (LLDDs)
- **[LLDD_Blossom_Server.md](design/LLDD_Blossom_Server.md)** - Blossom file server implementation
- **[LLDD_Key_Management.md](design/LLDD_Key_Management.md)** - Cryptographic key management
- **[LLDD_Local_Relay.md](design/LLDD_Local_Relay.md)** - Local Nostr relay server
- **[LLDD_NIP07_Implementation.md](design/LLDD_NIP07_Implementation.md)** - window.nostr API implementation
- **[LLDD_Nsite_Streaming_Server.md](design/LLDD_Nsite_Streaming_Server.md)** - Nsite content streaming
- **[LLDD_Nsite_Support.md](design/LLDD_Nsite_Support.md)** - Static website hosting
- **[LLDD_Protocol_Handler.md](design/LLDD_Protocol_Handler.md)** - nostr:// URL scheme handling

### 📋 `/specifications/` - Protocol & Format Specifications
External protocol and format specifications that dryft implements:

- **[BUD-00.md](specifications/BUD-00.md)** - Blossom Upload Descriptor Base
- **[BUD-01.md](specifications/BUD-01.md)** - Blossom Upload Descriptor v1
- **[BUD-03.md](specifications/BUD-03.md)** - Blossom Upload Descriptor v3
- **[nsite-spec.md](specifications/nsite-spec.md)** - Nsite static website specification

### 📚 `/reference/` - Reference Documentation
Supporting documentation for various system components:

#### API & Integration
- **[Browser_API_Extensions.md](reference/Browser_API_Extensions.md)** - Browser API extensions (window.nostr, window.blossom)
- **[Browser_Integration_Details.md](reference/Browser_Integration_Details.md)** - Chrome integration details

#### Configuration & Storage  
- **[Preferences_Schema.md](reference/Preferences_Schema.md)** - User preferences structure
- **[Storage_Architecture.md](reference/Storage_Architecture.md)** - Data storage design

#### Operations & Performance
- **[Performance_Benchmarks_and_Targets.md](reference/Performance_Benchmarks_and_Targets.md)** - Performance requirements
- **[Telemetry_and_Analytics_Approach.md](reference/Telemetry_and_Analytics_Approach.md)** - Usage analytics
- **[Error_Recovery_Strategies.md](reference/Error_Recovery_Strategies.md)** - Error handling strategies
- **[Deployment_and_Update_Strategies.md](reference/Deployment_and_Update_Strategies.md)** - Release management

#### Network & UI
- **[Relay_Connection_Management.md](reference/Relay_Connection_Management.md)** - Nostr relay connections
- **[UI_UX_Mockups_and_Flows.md](reference/UI_UX_Mockups_and_Flows.md)** - User interface designs

### 🔄 `/processes/` - Development Processes
Development workflows and processes (currently empty - to be populated with workflow documentation)

## Navigation Tips

1. **Starting Development**: Begin with [PRD.md](design/PRD.md) and [HLDD.md](design/HLDD.md)
2. **Implementation Details**: Check relevant LLDDs in `/design/` 
3. **Protocol Questions**: Refer to specifications in `/specifications/`
4. **Progress Tracking**: Check implementation summaries in `/implementation/`
5. **System Reference**: Use documents in `/reference/` for detailed technical information

## Cross-References

Many documents reference each other. When moved or renamed, the following common patterns are used:
- `memory/filename.md` → `memory/category/filename.md`
- `../memory/filename.md` → `../memory/category/filename.md`
- `./filename.md` → `./category/filename.md`

If you encounter broken links, please update them following the new directory structure.