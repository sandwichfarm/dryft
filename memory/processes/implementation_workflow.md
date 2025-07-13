# Implementation Workflow

## Overview
Step-by-step process for implementing features from issue selection to PR merge.

## Phase 1: Setup and Planning

### 1.1 Todo Management
```
Start Implementation
  ↓
TodoWrite: Create task breakdown
  ├─ Research and understand requirements
  ├─ Design implementation approach  
  ├─ Create necessary files
  ├─ Write implementation
  ├─ Add tests
  ├─ Update documentation
  └─ Create PR
```

### 1.2 Branch Creation
```bash
# Standard branch naming
git checkout -b issue-[group][number]-[brief-description]

# Examples:
git checkout -b issue-d15-performance-optimization
git checkout -b issue-b7-multi-account-support
```

### 1.3 Research Phase
- Read related LLDDs and specifications
- Check existing codebase for patterns
- Identify files to modify
- Understand dependencies and integration points

## Phase 2: Implementation

### 2.1 File Creation Order
```
Implementation Start
  ↓
Create header files first (.h)
  ↓
Create implementation files (.cc)
  ↓
Update BUILD.gn files
  ↓
Create unit tests
  ↓
Update integration points
```

### 2.2 Code Quality Standards
- Follow existing code patterns
- Add proper error handling
- Include comprehensive logging
- Use appropriate thread safety
- Add performance considerations

### 2.3 Progress Tracking
```
For each major step:
  ↓
TodoWrite: Mark current task in_progress
  ↓
Complete implementation
  ↓
TodoWrite: Mark task completed
  ↓
Move to next task
```

## Phase 3: Testing and Validation

### 3.1 Test Types Required
- **Unit tests**: Test individual components
- **Integration tests**: Test component interactions
- **Performance tests**: Verify performance targets
- **Security tests**: Validate security measures

### 3.2 Testing Checklist
```
Implementation Complete
  ↓
Unit tests pass locally?
  ├─ NO → Fix failing tests
  └─ YES → Continue
       ↓
Integration tests pass?
  ├─ NO → Fix integration issues
  └─ YES → Continue
       ↓
Performance targets met?
  ├─ NO → Optimize implementation
  └─ YES → Continue
       ↓
Security requirements satisfied?
  ├─ NO → Add security measures
  └─ YES → Ready for PR
```

## Phase 4: Documentation

### 4.1 Required Documentation
- Code comments for complex logic
- API documentation for public interfaces
- Update relevant design documents
- Add examples for new features

### 4.2 Documentation Checklist
- [ ] All public methods documented
- [ ] Complex algorithms explained
- [ ] Configuration options documented
- [ ] Integration examples provided

## Decision Points

### When to Refactor vs Continue
- **Refactor**: Code becomes unreadable, performance issues, security concerns
- **Continue**: Minor improvements needed, aesthetic changes, premature optimization

### When to Ask for Help
- Blocked by unclear requirements
- Can't find appropriate implementation pattern
- Performance targets seem impossible
- Security implications unclear

### When Implementation is Complete
- All acceptance criteria met
- All tests passing
- Documentation updated
- Performance targets achieved
- Security requirements satisfied