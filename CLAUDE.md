# CLAUDE.md - Tungsten Browser Development Guide

## Project Overview
Tungsten is a fork of Thorium browser that adds native Nostr protocol support, including built-in NIP-07 interface, local relay, Blossom server, nostr:// scheme support, and Nsite static website hosting.

## Development Workflow

### 1. Task Breakdown Structure
When implementing Tungsten features, break down work into these major groups:

#### Group A: Core Browser Integration
- Chromium/Thorium source modifications
- Build system integration
- IPC message definitions
- Process sandboxing

#### Group B: Nostr Protocol Implementation
- NIP-07 window.nostr injection
- Key management system
- Permission system
- Multi-account support

#### Group C: Local Services
- Local relay (SQLite-based)
- Blossom server (file system-based)
- Service coordination
- Storage management

#### Group D: Protocol Handlers
- nostr:// URL scheme
- Nsite static website support
- Content verification
- Sandboxed rendering

#### Group E: Browser APIs
- window.nostr extensions
- window.blossom API
- Pre-loaded libraries
- Local service interfaces

#### Group F: User Interface
- Settings pages
- Permission dialogs
- Status indicators
- Account management UI

#### Group G: Platform Integration
- Windows (Credential Manager)
- macOS (Keychain)
- Linux (Secret Service)
- Cross-platform abstractions

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
# Build Tungsten
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
- `/memory/Storage_Architecture.md` - Storage design
- `/memory/Browser_API_Extensions.md` - API specifications
- `/memory/Preferences_Schema.md` - Settings structure
- `/memory/nsite-spec.md` - Correct Nsite specification (kind 34128)
- `/memory/BUD-*.md` - Correct Blossom specifications

### 10. Development Process (PR Workflow)

When working on issues, follow this process:

#### Step 1: Work on an Issue
1. Select an issue from the current milestone
2. Verify all dependencies are resolved
3. Create a feature branch: `git checkout -b issue-[number]-description`
4. Implement the solution following specifications
5. Write/update tests
6. Update documentation
7. Run linters and tests locally

#### Step 2: Open a Pull Request
```bash
# Push your branch
git push -u origin issue-[number]-description

# Create PR using GitHub CLI
gh pr create --title "[Issue #X] Brief description" \
  --body "Fixes #X\n\n## Summary\n- What was implemented\n- Key changes\n\n## Testing\n- How it was tested\n- Test results" \
  --assignee @me
```

#### Step 3: Wait for Review (1-5 minutes)
```bash
# Set a timeout and query for review status
timeout=300  # 5 minutes
start_time=$(date +%s)

while true; do
  # Check PR review status
  review_status=$(gh pr view --json reviews -q '.reviews[-1].state' 2>/dev/null || echo "PENDING")
  
  if [[ "$review_status" == "APPROVED" ]]; then
    echo "PR approved!"
    break
  elif [[ "$review_status" != "PENDING" && "$review_status" != "" ]]; then
    echo "Review status: $review_status"
    # Fetch review comments
    gh pr view --json reviews -q '.reviews[-1].body'
    break
  fi
  
  # Check timeout
  current_time=$(date +%s)
  elapsed=$((current_time - start_time))
  if [[ $elapsed -gt $timeout ]]; then
    echo "Review timeout reached"
    break
  fi
  
  # Wait 30 seconds before checking again
  sleep 30
done
```

#### Step 4: Handle Review Outcome

**If Approved:**
```bash
# Merge the PR
gh pr merge --merge --delete-branch
```

**If Changes Requested:**
1. Read review comments carefully
2. Make requested changes
3. Commit and push updates
4. Request re-review: `gh pr review --request`
5. Return to Step 3

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
- When complete, you execute a timeout that checks the status of the pr until all review comments have been resolved or a comment has been left explaining why review comments were not resolved. Only then is the PR merged.
- Then continue on to the next task.

### 13. Debug Helpers

Enable debug logging:
```javascript
// In DevTools console
chrome.storage.local.set({'tungsten.dev.debug_logging': true});
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