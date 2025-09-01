---
name: Bug Fix (AI Agents)
about: Template for AI coding agents to fix bugs and issues
title: '[BUG] '
labels: ['bug', 'ai-agent']
assignees: ''
---

## 🐛 Bug Fix Template for AI Coding Agents

### Environment Setup

**Prerequisites**: Follow [DEVELOPMENT_SETUP.md](../.github/DEVELOPMENT_SETUP.md) if not already configured.

**Verify Environment**:

```bash
source .venv/bin/activate
west config -l  # Should show board and cmake-args configured
west build app --cmake-only  # Should complete without errors
```

## 📝 Bug Description

**Issue Summary**:
<!-- Describe the bug clearly -->

**Expected Behavior**:
<!-- What should happen -->

**Actual Behavior**:
<!-- What actually happens -->

**Steps to Reproduce**:

1. Step 1
2. Step 2
3. Step 3

**Environment Details**:

- NCS Version: v3.0.2
- Board: Thingy:52 (nrf52832)
- Build Command: `west build app`

## 🔍 Investigation Strategy

### Research Phase (Use these tools systematically)

```bash
# 1. Search for error patterns
grep_search "error_message|failure_pattern" --isRegexp true

# 2. Find related functionality
semantic_search "relevant keywords for the bug area"

# 3. Locate relevant files
file_search "**/*{affected_component}*"

# 4. Examine implementation
read_file /path/to/suspected/files
```

### Common Bug Categories & Investigation

**Build/Compilation Issues**:

- Check device tree compilation: `build/app/zephyr/zephyr.dts`
- Verify West configuration: `west config -l`
- Check for missing dependencies: `pip list` in `.venv`

**Hardware/Sensor Issues**:

- Look for SX1509B initialization errors: `sx1509b@3e init failed`
- Check GPIO state output: `board_print_pin_states()` in serial logs
- Verify sensor initialization order in device tree

**BLE Service Issues**:

- Check service registration in `main.c`
- Verify characteristic definitions in service files
- Test with nRF Connect mobile app

**Power Management Issues**:

- Review sensor power-down cycles
- Check for continuous vs one-shot mode configuration
- Verify regulator dependencies in device tree

## 🛠️ Bug Fix Workflow

### Phase 1: Root Cause Analysis

- [ ] **Reproduce the issue**: Follow steps to reproduce consistently
- [ ] **Check logs**: Look for error messages in build output or serial logs
- [ ] **Identify affected components**: Which modules/services are involved?
- [ ] **Review recent changes**: Use `git log` to check recent modifications

### Phase 2: Investigation

- [ ] **Read related code**: Use `semantic_search` and `read_file` to understand affected areas
- [ ] **Check dependencies**: Verify initialization order and dependencies
- [ ] **Compare working patterns**: Look for similar working implementations
- [ ] **Test hypotheses**: Use `west build --cmake-only` for quick verification

### Phase 3: Implementation

- [ ] **Minimal fix approach**: Make smallest possible change to fix the issue
- [ ] **Follow existing patterns**: Match coding style and error handling patterns
- [ ] **Preserve functionality**: Ensure fix doesn't break existing features
- [ ] **Add safeguards**: Include error handling if appropriate

### Phase 4: Verification

- [ ] **Build test**: Verify fix compiles cleanly
- [ ] **Regression test**: Ensure no new issues introduced
- [ ] **Hardware test**: Flash and verify on actual device (if applicable)
- [ ] **Integration test**: Check all related functionality still works

## ✅ Testing & Validation Protocol

### Mandatory Validation Steps

```bash
# 1. Check for syntax/lint errors
get_errors /path/to/modified/files

# 2. Quick build verification
west build app --cmake-only

# 3. Full pristine build
west build -p always app

# 4. Memory usage check
# Verify flash/RAM usage hasn't significantly increased
```

### Hardware Testing (if applicable)

```bash
# Flash and test
west flash
west attach  # Monitor for error messages

# Check specific functionality
# Test the specific bug scenario
```

### Integration Testing

- [ ] **Original issue**: Verify the original bug is fixed
- [ ] **Related functionality**: Check that related features still work
- [ ] **Error cases**: Test error conditions and recovery
- [ ] **Performance**: Ensure no significant performance regression

## 📝 Fix Documentation

### Code Changes Checklist

- [ ] **Minimal change**: Only modify what's necessary to fix the bug
- [ ] **Clear comments**: Add comments explaining the fix if non-obvious
- [ ] **Error handling**: Include appropriate error handling
- [ ] **Logging**: Add/update log messages if helpful for future debugging

### Pull Request Description

```markdown
## Bug Fix: [Issue Summary]

### Root Cause
- Brief explanation of what was causing the issue

### Solution
- Description of the fix implemented
- Why this approach was chosen

### Changes Made
- [ ] Core fix in `app/src/[file]`
- [ ] Related changes in `app/include/[file]`
- [ ] Configuration updates (if any)

### Testing Performed
- [ ] Build verification: `west build -p always app`
- [ ] Bug reproduction: Original issue no longer occurs
- [ ] Regression testing: Related functionality verified
- [ ] Hardware testing: Flash and device verification (if applicable)

### Files Modified
- `app/src/[file]` - Core bug fix
- `app/include/[file]` - Header updates (if any)

### Memory Impact
Flash: [change] (+/-[X]KB)
RAM: [change] (+/-[X]KB)
```

## 🚨 Critical Areas to Check

### SX1509B GPIO Expander Issues

- Initialization order problems
- Device tree dependency cycles
- Power sequencing issues

### Device Tree Problems

- Incorrect initialization priorities
- Missing or incorrect GPIO configurations
- Regulator dependency issues

### BLE Service Issues

- Service registration order
- Characteristic value encoding/decoding
- Connection handling edge cases

### Sensor Management Issues

- Power management state machine
- I2C communication failures
- One-shot vs continuous mode conflicts

### Build System Issues

- West configuration problems
- Missing CMake arguments
- Python virtual environment issues

## 📚 Debugging Resources

### Serial Output Analysis

```bash
# Capture serial output for analysis
screen -dmS debug_session -L -Logfile debug.log picocom -b 115200 /dev/ttyUSB0

# Reset device and capture startup logs
nrfutil device reset --serial-number [DEVICE_ID]

# Read captured output
read_file debug.log
```

### Common Error Patterns

- `sx1509b@3e init failed: -5`: GPIO expander initialization failure
- `Failed to get sensor data`: Sensor communication or power issue
- `BLE service registration failed`: Service configuration problem
- `Device tree error`: DTS compilation or validation issue

### Reference Documentation

- [Project Architecture](../.github/copilot-instructions.md)
- [Environment Setup](../.github/DEVELOPMENT_SETUP.md)
- [NCS Troubleshooting](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/troubleshooting.html)
- [Zephyr Device Tree](https://docs.zephyrproject.org/latest/build/dts/index.html)
