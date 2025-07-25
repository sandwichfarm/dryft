# CLAUDE.md - dryft browser Development Guide

## Project Overview
dryft is a fork of Thorium browser that adds native Nostr protocol support, including built-in NIP-07 interface, local relay, Blossom server, nostr:// scheme support, and Nsite static website hosting.

## Development Workflow

### Quick Reference Decision Tree

```
START
  ↓
TodoRead → Check current state
  ├─ Active todos? → Continue current work
  ├─ Review comments pending? → Address reviews (→ Review Process)
  └─ No active work → Select new issue (→ Issue Selection)
       ↓
IMPLEMENTATION LOOP:
  ├─ 1. Research & Plan (→ Implementation Workflow)
  ├─ 2. Code & Test
  ├─ 3. Create PR (CRITICAL: --base main)
  ├─ 4. Monitor Reviews (→ Review Process)
  ├─ 5. Address Feedback
  ├─ 6. Merge & Continue (→ Merge Process)
  └─ 7. Next Issue → Return to START
```

### Task Groups (Implementation Priority Order)

**A. Core Browser** → **B. Nostr Protocol** → **C. Local Services** → **D. Protocol Handlers** → **E. Browser APIs** → **F. User Interface** → **G. Platform Integration**

| Group | Focus Area | Examples |
|-------|------------|----------|
| **A** | Core Browser Integration | Chromium/Thorium mods, Build system, IPC |
| **B** | Nostr Protocol | NIP-07, Key management, Permissions |
| **C** | Local Services | Local relay, Blossom server, Storage |
| **D** | Protocol Handlers | nostr:// URLs, Nsite support, Content verification |
| **E** | Browser APIs | window.nostr, window.blossom, Pre-loaded libs |
| **F** | User Interface | Settings, Dialogs, Status indicators |
| **G** | Platform Integration | Windows/macOS/Linux platform-specific features |

### 2. Issue Creation Template

When creating issues, use this format:

```markdown
## Issue Title: [Group Letter]-[Number]: Brief Description

### Context
- **Group**: [A-G from above]
- **Dependencies**: [List other issues that must be completed first]
- **Milestone**: [Current milestone name]

### Description
[Detailed description of what needs to be implemented]

### Acceptance Criteria
- [ ] Criterion 1
- [ ] Criterion 2
- [ ] Tests written
- [ ] Documentation updated

### Technical Notes
- Key files to modify
- Important considerations
- Potential gotchas

### References
- Related LLDDs: [List relevant design documents]
- Related NIPs/BUDs: [List protocol specs]
```

### 3. Milestone Structure

Create milestones in this order:

1. **M1: Foundation** - Core browser modifications, build system
2. **M2: Nostr Core** - NIP-07, key management, permissions
3. **M3: Local Services** - Local relay and Blossom server
4. **M4: Protocol Support** - nostr:// and Nsite implementation
5. **M5: Enhanced APIs** - window.nostr.libs, window.blossom
6. **M6: User Interface** - Settings, dialogs, indicators
7. **M7: Polish** - Platform integration, testing, documentation

### 4. Implementation Order

Always implement in dependency order:
1. Start with Group A (Core Browser) issues
2. Then Group B (Nostr Protocol) 
3. Groups C & D can be parallelized
4. Group E depends on B, C, D
5. Group F can start after B
6. Group G can be done incrementally

### 5. Key Implementation Details

#### Storage Architecture
- **Local Relay**: SQLite with WAL mode for complex queries
- **Blossom**: Direct file system storage with sharding
- **Browser State**: LMDB for fast key-value storage
- **Preferences**: JSON files via Chromium prefs system

#### Security Considerations
- Remove 'unsafe-inline' and 'unsafe-eval' from CSP
- Implement proper key rotation mechanisms
- Add rate limiting for all operations
- Use platform-specific secure storage for keys

#### API Design
- `window.nostr.libs.[library]` returns importable URL paths
- `window.nostr.relay.url` is a read-only getter
- `window.blossom` provides upload/download/mirror methods
- All async operations return Promises

#### Testing Strategy
- Unit tests for each component
- Integration tests for IPC communication
- E2E tests for user workflows
- Performance benchmarks for targets

### 6. Common Commands

```bash
# Build dryft
cd src
gn gen out/Release --args="is_official_build=true enable_nostr=true enable_local_relay=true enable_blossom_server=true"
autoninja -C out/Release chrome

# Run tests
./out/Release/unit_tests --gtest_filter="Nostr*"
./out/Release/browser_tests --gtest_filter="Nostr*"

# Check for specification compliance
grep -r "kind.*30503\|kind.*30564" . # Should return nothing (old Nsite spec)
grep -r "kind.*34128" . # Should find Nsite implementation
grep -r "kind.*24242" . # Should find Blossom auth
```

### 7. Code Review Checklist

Before submitting PRs:
- [ ] No hardcoded ports or URLs
- [ ] All preferences have defaults
- [ ] Error handling for all async operations
- [ ] No memory leaks in native code
- [ ] Proper cleanup in destructors
- [ ] Rate limiting implemented
- [ ] Tests cover happy and error paths
- [ ] Documentation updated

### 8. Performance Targets

Remember these targets:
- Startup overhead: <50ms
- Memory overhead: <50MB base
- CPU usage idle: <0.1%
- NIP-07 operations: <20ms
- Local relay query: <10ms

### 9. Specification References

Always refer to:
- `/memory/reference/Storage_Architecture.md` - Storage design
- `/memory/reference/Browser_API_Extensions.md` - API specifications
- `/memory/reference/Preferences_Schema.md` - Settings structure
- `/memory/specifications/nsite-spec.md` - Correct Nsite specification (kind 34128)
- `/memory/specifications/BUD-*.md` - Correct Blossom specifications

### 10. Streamlined Development Process

**Claude should follow this exact sequence for all work:**

#### Phase 1: Issue Selection & Setup
```
1. TodoRead → Check current state
2. If active todos → Continue current work
3. If no todos → Select next issue (priority: reviews → current milestone → next milestone)
4. TodoWrite → Create task breakdown
5. git checkout -b issue-[group][number]-description
```

#### Phase 2: Implementation
```
1. Research specifications and existing patterns
2. Implement solution incrementally
3. TodoWrite → Update progress after each major step
4. Add comprehensive tests
5. Update documentation
6. Verify all acceptance criteria met
```

#### Phase 3: PR Creation (CRITICAL)
```bash
# !IMPORTANT ALWAYS set base branch to main
gh pr create \
  --title "[Group-Number]: Brief description" \
  --base main \
  --body "Resolves #[issue]

## Summary
- Implementation details
- Key changes

## Testing  
- Test coverage
- Performance verified

🤖 Generated with [Claude Code](https://claude.ai/code)"
```

#### Phase 4: Review & Merge
```
1. Monitor for reviews (5 min timeout)
2. Address ALL review comments systematically
3. Push fixes and request re-review
4. Merge when approved: gh pr merge --squash --delete-branch
5. TodoWrite → Mark all tasks completed
6. Return to Phase 1 for next issue
```

#### Detailed Process Documentation
- **Issue Selection**: `/memory/processes/issue_selection_process.md`
- **Implementation**: `/memory/processes/implementation_workflow.md`  
- **Review Handling**: `/memory/processes/review_process.md`
- **Merge & Continue**: `/memory/processes/merge_and_continuation.md`
- **Error Recovery**: `/memory/processes/error_handling_process.md`

### 11. PR Best Practices

1. **Keep PRs focused**: One issue per PR
2. **Write clear descriptions**: Explain what and why
3. **Include tests**: Every PR must have tests
4. **Update docs**: Keep documentation in sync
5. **Check CI status**: Ensure all checks pass
6. **Respond promptly**: Address review feedback quickly

### 12. PR Memory
- After submitting a PR, always sleep and check the PR for reviews, respond/iterate in response to review, and then merge when complete.
- When resolving an issue on a PR, you immediately move on to the next issue.

### 13. Debug Helpers

Enable debug logging:
```javascript
// In DevTools console
chrome.storage.local.set({'dryft.dev.debug_logging': true});
```

View local relay status:
```javascript
console.log(window.nostr.relay.url, window.nostr.relay.connected);
```

Test library loading:
```javascript
const NDK = await import(window.nostr.libs.ndk);
console.log('NDK loaded:', NDK);
```

## Remember

1. **Always check dependencies** before starting an issue
2. **Create PRs early** for visibility and feedback
3. **Iterate based on reviews** - reviews improve code quality
4. **Test on all platforms** before marking complete
5. **Update documentation** as you implement
6. **Follow the specifications** exactly - no improvisation
7. **Ask for clarification** if specs are ambiguous

This structured approach ensures systematic implementation with clear dependencies, proper review cycles, and testable milestones.