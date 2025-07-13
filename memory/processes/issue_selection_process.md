# Issue Selection Process

## Overview
How Claude should select and prioritize issues for implementation.

## Decision Tree

### 1. Check Current State
```
START
  ↓
Are there open PRs awaiting review?
  ├─ YES → Go to Review Process
  └─ NO → Continue to Issue Selection
```

### 2. Issue Selection Criteria
```
Issue Selection
  ↓
Check TodoRead for pending tasks
  ├─ Has active todos? → Continue current work
  └─ No active todos → Select new issue
```

### 3. Priority Matrix
Select issues based on:

**Priority 1: Blocking Issues**
- Review comments on existing PRs
- Failed builds or tests
- Dependency blockers for current milestone

**Priority 2: Current Milestone**
- Issues in current milestone with resolved dependencies
- Critical path items for milestone completion

**Priority 3: Next Milestone Preparation**
- Issues with no dependencies from next milestone
- Foundation work for upcoming features

### 4. Dependency Verification
Before starting any issue:

```
Selected Issue
  ↓
Check dependencies in issue description
  ├─ All dependencies resolved? → Proceed
  ├─ Some dependencies pending? → Select different issue
  └─ Unclear dependencies? → Ask user for clarification
```

### 5. Implementation Readiness
```
Dependencies Verified
  ↓
Check specifications and design docs
  ├─ Clear specifications? → Start implementation
  ├─ Ambiguous requirements? → Ask for clarification
  └─ Missing specs? → Request specifications
```

## Commands for Issue Selection

```bash
# Check current milestone progress
gh issue list --milestone "M4: Protocol Support" --state open

# Check dependencies for specific issue
gh issue view [issue-number] --json body -q '.body'

# List issues by priority labels
gh issue list --label "priority/high" --state open
```

## Decision Points

### When to Continue vs Switch Issues
- **Continue**: Active todos, review comments pending, current implementation in progress
- **Switch**: Issue blocked by dependencies, unclear requirements, waiting for external input

### When to Ask for Clarification
- Issue dependencies unclear
- Acceptance criteria ambiguous
- Technical approach undefined
- Conflicting specifications

### When to Create New Issues
- Discover missing dependencies during implementation
- Find specification gaps that need addressing
- Identify testing requirements not covered