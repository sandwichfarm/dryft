# Error Handling and Recovery Process

## Overview
Systematic approach to handling errors, failures, and unexpected situations during development.

## Error Categories

### Category 1: Build and Compilation Errors
```
Build Failure
  ↓
Analyze error message
  ├─ Missing dependency? → Add to BUILD.gn
  ├─ Syntax error? → Fix syntax
  ├─ Include missing? → Add include
  └─ Linker error? → Check library dependencies
```

### Category 2: Test Failures
```
Test Failure
  ↓
Identify failure type:
  ├─ Unit test failure → Check implementation logic
  ├─ Integration test failure → Check component interactions
  ├─ Performance test failure → Optimize implementation
  └─ Security test failure → Add security measures
```

### Category 3: Review Process Errors
```
Review Issues
  ↓
Issue type:
  ├─ Missing functionality → Implement missing features
  ├─ Code quality issues → Refactor code
  ├─ Security concerns → Add security measures
  ├─ Performance problems → Optimize implementation
  └─ Documentation gaps → Update documentation
```

### Category 4: Git and Workflow Errors
```
Git Issues
  ↓
Problem type:
  ├─ Merge conflicts → Resolve conflicts systematically
  ├─ Wrong base branch → Fix base branch
  ├─ Missing commits → Cherry-pick or rebase
  └─ Branch diverged → Rebase and force-push
```

## Systematic Error Resolution

### Step 1: Error Analysis
```
Error Encountered
  ↓
Gather information:
  ├─ Error message text
  ├─ Stack trace if available
  ├─ Context when error occurred
  └─ Steps to reproduce
       ↓
Categorize error type
  ↓
Apply category-specific resolution
```

### Step 2: Root Cause Analysis
```
Error Categorized
  ↓
Ask key questions:
  ├─ What changed since last working state?
  ├─ Are dependencies up to date?
  ├─ Is environment configured correctly?
  ├─ Are there missing prerequisites?
  └─ Is this a known issue?
```

### Step 3: Resolution Strategy
```
Root Cause Identified
  ↓
Choose resolution approach:
  ├─ Quick fix possible? → Apply immediate fix
  ├─ Requires investigation? → Research thoroughly
  ├─ Blocking other work? → Escalate or ask for help
  └─ Non-critical? → Document and defer
```

## Common Error Patterns and Solutions

### Build System Errors
```cpp
// Missing include error
#include "missing/header/file.h"  // Add missing include

// Linker error - missing dependency
deps = [
  "//missing/dependency",  // Add to BUILD.gn
]

// Namespace error
using namespace correct_namespace;  // Add correct namespace
```

### Test-Related Errors
```cpp
// Assertion failure
EXPECT_EQ(expected_value, actual_value);  // Fix expected vs actual

// Memory leak
std::unique_ptr<Object> obj = std::make_unique<Object>();  // Use smart pointers

// Threading issue
base::AutoLock lock(mutex_);  // Add proper synchronization
```

### Integration Errors
```cpp
// Missing initialization
if (!component_initialized_) {
  InitializeComponent();  // Add initialization check
}

// Callback not set
if (callback_) {
  callback_.Run(result);  // Check callback before using
}
```

## Recovery Strategies

### When Implementation is Blocked
```
Blocked Implementation
  ↓
Identify blocking factor:
  ├─ Missing dependency → Implement dependency first
  ├─ Unclear requirements → Ask for clarification
  ├─ Technical limitation → Research alternatives
  └─ External dependency → Work around or wait
```

### When Tests are Failing
```
Failing Tests
  ↓
Debugging approach:
  1. Run single failing test in isolation
  2. Add debug logging to understand state
  3. Check test setup and teardown
  4. Verify test expectations are correct
  5. Fix implementation or test as needed
```

### When Reviews are Critical
```
Critical Review Feedback
  ↓
Response strategy:
  1. Acknowledge feedback immediately
  2. Understand the concern completely
  3. Research best practices
  4. Implement comprehensive fix
  5. Add tests to prevent regression
```

## Prevention Strategies

### Code Quality Prevention
```
Before Implementation:
  ├─ Read existing code patterns
  ├─ Check similar implementations
  ├─ Understand error handling patterns
  └─ Plan error scenarios
```

### Testing Prevention
```
Test Strategy:
  ├─ Write tests before implementation (TDD)
  ├─ Test error conditions explicitly
  ├─ Add integration tests for complex flows
  └─ Include performance tests for critical paths
```

### Review Prevention
```
Pre-Review Checklist:
  ├─ Self-review all changes
  ├─ Run all tests locally
  ├─ Check documentation completeness
  ├─ Verify security implications
  └─ Test performance impact
```

## Escalation Process

### When to Ask for Help
```
Problem Complexity Assessment
  ↓
Time spent debugging:
  ├─ >30 minutes on syntax/build errors → Ask for help
  ├─ >2 hours on logic errors → Ask for help  
  ├─ >1 day on design issues → Ask for help
  └─ Immediate for security concerns → Ask for help
```

### How to Ask for Help
```
Help Request Format:
  1. Describe what you're trying to achieve
  2. Explain what you've tried
  3. Show error messages/symptoms
  4. Provide minimal reproduction case
  5. Suggest possible solutions you've considered
```

## Recovery Commands

### Git Recovery
```bash
# Undo last commit (keep changes)
git reset --soft HEAD~1

# Undo all local changes
git reset --hard HEAD

# Fix wrong base branch
git rebase --onto main wrong-base current-branch

# Recover lost commits
git reflog
git cherry-pick [lost-commit-hash]
```

### Build Recovery
```bash
# Clean build
rm -rf out/
gn gen out/Release
autoninja -C out/Release chrome

# Reset dependencies
gclient sync --force
```

### Test Recovery
```bash
# Run specific failing test
./out/Release/unit_tests --gtest_filter="SpecificTest.Method"

# Run with debug output
./out/Release/unit_tests --gtest_filter="*" --logtostderr --v=2
```

## Documentation of Errors

### Error Log Template
```markdown
## Error: [Brief Description]

### Context
- Date/Time: [timestamp]
- Component: [affected component]
- Operation: [what was being attempted]

### Symptoms
- Error message: [exact error text]
- Stack trace: [if available]
- Reproduction steps: [how to reproduce]

### Resolution
- Root cause: [what was wrong]
- Solution applied: [what fixed it]
- Prevention: [how to avoid in future]

### Related Issues
- Similar problems: [links to related issues]
- Documentation updates needed: [what docs to update]
```