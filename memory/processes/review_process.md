# Review Process

## Overview
Comprehensive process for handling PR reviews and addressing feedback.

## Phase 1: PR Creation

### 1.1 Pre-PR Checklist
```
Implementation Complete
  ↓
Run local tests
  ├─ Tests failing? → Fix tests first
  └─ Tests passing → Continue
       ↓
Check code quality
  ├─ Linting errors? → Fix linting
  └─ Clean code → Continue
       ↓
Update documentation
  ├─ Docs missing? → Add documentation
  └─ Docs complete → Ready for PR
```

### 1.2 PR Creation Command
```bash
# CRITICAL: Always set base branch to main
gh pr create \
  --title "[Group-Number]: Brief description" \
  --base main \
  --body "$(cat <<'EOF'
## Summary
- What was implemented
- Key changes made

## Testing
- How it was tested
- Test results

## Files Modified
- List of key files changed

Fixes #[issue-number]

🤖 Generated with [Claude Code](https://claude.ai/code)
EOF
)"
```

## Phase 2: Review Monitoring

### 2.1 Review Check Loop
```bash
# Wait for review with timeout
timeout=300  # 5 minutes
start_time=$(date +%s)

while true; do
  review_status=$(gh pr view --json reviews -q '.reviews[-1].state' 2>/dev/null || echo "PENDING")
  
  if [[ "$review_status" == "APPROVED" ]]; then
    echo "PR approved!"
    break
  elif [[ "$review_status" == "CHANGES_REQUESTED" ]]; then
    echo "Changes requested"
    gh pr view --json reviews -q '.reviews[-1].body'
    break
  elif [[ "$review_status" == "COMMENTED" ]]; then
    echo "Comments provided"
    gh api repos/{owner}/{repo}/pulls/[pr-number]/comments
    break
  fi
  
  current_time=$(date +%s)
  elapsed=$((current_time - start_time))
  if [[ $elapsed -gt $timeout ]]; then
    echo "Review timeout - continue with next task"
    break
  fi
  
  sleep 30
done
```

## Phase 3: Addressing Review Comments

### 3.1 Comment Analysis
```
Review Comments Received
  ↓
Categorize comments:
  ├─ Bug fixes required → High priority
  ├─ Security concerns → Critical priority  
  ├─ Performance issues → Medium priority
  ├─ Code style → Low priority
  └─ Questions/clarifications → Address immediately
```

### 3.2 Implementation Strategy
For each comment:

```
Review Comment
  ↓
Understand the issue
  ├─ Clear what's needed? → Implement fix
  └─ Unclear requirement? → Ask for clarification
       ↓
Implement fix
  ↓
Test fix locally
  ├─ Fix works? → Commit and push
  └─ Fix broken? → Debug and retry
```

### 3.3 Comment Resolution Workflow
```bash
# For each review comment:
# 1. Create commit addressing the comment
git add [modified-files]
git commit -m "Address review comment: [brief description]

- Specific change made
- Why this change addresses the issue

🤖 Generated with [Claude Code](https://claude.ai/code)"

# 2. Push changes
git push

# 3. Mark comment as resolved if applicable
# (GitHub will auto-resolve when changes pushed)
```

## Phase 4: Re-review Process

### 4.1 After Addressing Comments
```
Changes Pushed
  ↓
Request re-review if needed
  ↓
gh pr review --request
  ↓
Return to Review Check Loop
```

### 4.2 Multiple Review Cycles
```
Review Cycle
  ↓
Address all comments → Push changes → Wait for re-review
  ↓
More comments?
  ├─ YES → Repeat cycle
  └─ NO → Proceed to merge
```

## Phase 5: Merge Process

### 5.1 Pre-Merge Verification
```
PR Approved
  ↓
Check merge conflicts
  ├─ Conflicts exist? → Resolve conflicts
  └─ No conflicts → Proceed
       ↓
Verify CI status
  ├─ CI failing? → Fix CI issues
  └─ CI passing → Ready to merge
```

### 5.2 Merge Commands
```bash
# Check merge status
gh pr view [pr-number] --json mergeable,mergeStateStatus

# If mergeable:
gh pr merge [pr-number] --squash --delete-branch

# If conflicts, rebase:
git fetch origin main
git rebase origin/main
# Resolve conflicts
git add [resolved-files]
git rebase --continue
git push --force-with-lease
```

## Decision Points

### When to Rebase vs Merge
- **Rebase**: Conflicts with main, outdated branch
- **Merge**: Clean branch, no conflicts

### When to Ask for Clarification
- Review comment unclear
- Conflicting feedback from multiple reviewers
- Unsure about implementation approach
- Breaking change implications unclear

### When to Push Back on Reviews
- Request conflicts with specifications
- Performance regression required for stylistic change
- Security implications of suggested change
- Scope creep beyond original issue

## Common Review Issues

### Missing SetNotificationManager Pattern
- Always wire up manager references in service classes
- Call Set* methods after creating managers
- Update streaming server to receive manager references

### Format String Issues
- Ensure all %s placeholders have corresponding parameters
- Use unused parameters or remove them
- Validate string formatting with actual values

### Missing Callback Implementation
- JavaScript dismiss actions must call back to C++ side
- Implement proper communication channels (fetch, postMessage)
- Handle callback failures gracefully