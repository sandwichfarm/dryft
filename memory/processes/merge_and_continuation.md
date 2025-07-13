# Merge and Continuation Process

## Overview
Process for merging completed work and transitioning to next tasks seamlessly.

## Phase 1: Pre-Merge Validation

### 1.1 Final Checks
```
PR Approved and Ready
  ↓
Verify all review comments addressed
  ├─ Comments unresolved? → Address remaining comments
  └─ All resolved → Continue
       ↓
Check CI/CD status
  ├─ Tests failing? → Fix failing tests
  └─ All green → Continue
       ↓
Verify merge conflicts
  ├─ Conflicts exist? → Go to Conflict Resolution
  └─ Clean merge → Proceed to merge
```

### 1.2 Conflict Resolution
```bash
# Standard rebase workflow
git fetch origin main
git rebase origin/main

# If conflicts:
# 1. Resolve conflicts in files
# 2. Keep both sets of changes when possible
# 3. Ensure all functionality preserved

git add [resolved-files]
git rebase --continue
git push --force-with-lease

# Re-check CI status
```

## Phase 2: Merge Execution

### 2.1 Merge Strategy Decision
```
Ready to Merge
  ↓
Check repository merge settings
  ├─ Squash only? → Use squash merge
  ├─ Merge commits allowed? → Use merge commit
  └─ Rebase only? → Use rebase merge
```

### 2.2 Merge Commands
```bash
# Preferred: Squash merge (most repos)
gh pr merge [pr-number] --squash --delete-branch

# Alternative: Regular merge
gh pr merge [pr-number] --merge --delete-branch

# If merge fails due to conflicts:
# Return to Conflict Resolution phase
```

## Phase 3: Post-Merge Cleanup

### 3.1 Todo Management
```
Merge Successful
  ↓
TodoWrite: Mark all related tasks as completed
  ↓
TodoRead: Check for remaining tasks
  ├─ More tasks in current issue? → Continue current work
  └─ Current issue complete → Move to next issue
```

### 3.2 Branch Cleanup
```bash
# Verify branch deletion
git branch -a | grep [branch-name]

# If local branch still exists:
git branch -d [branch-name]

# Update local main
git checkout main
git pull origin main
```

## Phase 4: Next Issue Selection

### 4.1 Completion Assessment
```
Current Issue Complete
  ↓
Check issue status
  ├─ All acceptance criteria met? → Close issue
  └─ More work needed? → Create follow-up issue
       ↓
Update milestone progress
  ├─ Milestone complete? → Move to next milestone
  └─ More issues in milestone → Continue milestone
```

### 4.2 Issue Closure
```bash
# Close completed issue
gh issue close [issue-number] --comment "Completed in PR #[pr-number]

All acceptance criteria met:
- [x] Criterion 1
- [x] Criterion 2
- [x] Tests written
- [x] Documentation updated"
```

### 4.3 Next Issue Selection Algorithm
```
Issue Complete
  ↓
Check for urgent items:
  ├─ Review comments on other PRs? → Address reviews first
  ├─ Build failures? → Fix build issues
  └─ No urgent items → Select next issue
       ↓
Priority order:
  1. Current milestone blocking issues
  2. Current milestone ready issues
  3. Next milestone foundation issues
  4. Documentation and improvement tasks
```

## Phase 5: Seamless Transition

### 5.1 Context Switching
```
Next Issue Selected
  ↓
Create new todo list for issue
  ↓
Read specifications and dependencies
  ↓
Check branch requirements
  ├─ Need new branch? → Create feature branch
  └─ Continue on main? → Stay on main
       ↓
Begin implementation immediately
```

### 5.2 Implementation Continuity
```bash
# Immediate start on next issue
TodoWrite: Create breakdown for new issue
git checkout -b issue-[next-issue]
# Begin research and implementation
```

## Decision Points

### When to Create Follow-up Issues
- Original issue too large for single PR
- New requirements discovered during implementation
- Technical debt identified that needs separate attention
- Related improvements that exceed current scope

### When to Continue vs Switch Context
- **Continue**: Logical continuation of current work, related functionality
- **Switch**: Different subsystem, unrelated feature, blocking dependency

### When to Take a Break
- After completing major milestone
- Before starting complex new feature
- When user explicitly requests pause
- When unclear about next priorities

## Automation Opportunities

### Scripts for Common Tasks
```bash
# Complete issue and move to next
complete_issue() {
  local issue_num=$1
  local pr_num=$2
  
  gh issue close $issue_num --comment "Completed in PR #$pr_num"
  TodoWrite: Mark all tasks completed
  TodoRead: Check next priorities
}

# Seamless transition
next_issue() {
  local next_issue=$1
  
  git checkout main
  git pull origin main
  git checkout -b issue-$next_issue
  TodoWrite: Create new breakdown
}
```

## Quality Gates

### Before Moving to Next Issue
- [ ] All tests passing
- [ ] Documentation updated
- [ ] Performance targets met
- [ ] Security requirements satisfied
- [ ] Code review feedback addressed
- [ ] Issue acceptance criteria completed

### Before Milestone Completion
- [ ] All milestone issues closed
- [ ] Integration tests passing
- [ ] Performance benchmarks met
- [ ] Documentation comprehensive
- [ ] Security audit complete