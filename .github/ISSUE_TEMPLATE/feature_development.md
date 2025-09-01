---
name: Feature Development (AI Agents)
about: Template for AI coding agents to implement new features
title: '[FEATURE] '
labels: ['enhancement', 'ai-agent']
assignees: ''
---

## ⚙️ Pre-Development Setup

**Environment Setup**: Follow [DEVELOPMENT_SETUP.md](../.github/DEVELOPMENT_SETUP.md) if not already configured.

**Verify Environment**:

```bash
source .venv/bin/activate
west config -l  # Should show board and cmake-args configured
west build app --cmake-only  # Should complete without errors
```

## 📋 Feature Requirements

**Feature Description**:
<!-- Describe the feature to be implemented -->

**Acceptance Criteria**:
<!-- List specific requirements that must be met -->

- [ ] Requirement 1
- [ ] Requirement 2
- [ ] Requirement 3

**Files Likely to be Modified**:
<!-- AI Agents: Use semantic_search, file_search, and grep_search to identify relevant files -->

- [ ] `app/src/` - Core implementation files
- [ ] `app/include/` - Header files
- [ ] `app/prj.conf` - Configuration (if needed)
- [ ] `app/boards/thingy52.conf` - Board-specific config (if needed)
- [ ] `app/boards/thingy52.overlay` - Device tree (if hardware changes)

## 🔍 Implementation Strategy

**Research Phase** (Use these tools before coding):

```bash
# 1. Find related code patterns
semantic_search "relevant keywords for your feature"

# 2. Locate existing implementations
file_search "**/*{sensor,ble,service}*"

# 3. Search for similar patterns
grep_search "pattern|related|keywords" --isRegexp true

# 4. Examine key files
read_file /path/to/relevant/files
```

**Architecture Review**:

- [ ] Read [copilot-instructions.md](../.github/copilot-instructions.md) for project patterns
- [ ] Understand sensor management flow in `sensor_manager.c`
- [ ] Review BLE service architecture (`ess_service.c`, `ble_battery_service.c`)
- [ ] Check power management implications (SX1509B dependencies)

## 🛠️ Development Workflow

### Phase 1: Analysis & Planning

- [ ] **Search existing patterns**: Use `semantic_search` and `grep_search` to understand current implementations
- [ ] **Identify integration points**: How does this feature connect to existing services?
- [ ] **Check hardware dependencies**: Does this require SX1509B GPIO expander changes?
- [ ] **Review device tree**: Are device tree modifications needed?

### Phase 2: Implementation

- [ ] **Follow coding patterns**: Match existing code style in `sensor_manager.c` and BLE services
- [ ] **Use error handling**: Follow Zephyr logging patterns (LOG_ERR, LOG_WRN, LOG_INF, LOG_DBG)
- [ ] **Implement power optimization**: Follow one-shot sensor reading pattern if applicable
- [ ] **Update headers**: Add function declarations to appropriate header files

### Phase 3: Integration

- [ ] **Update main.c**: Add initialization calls if needed
- [ ] **Configure services**: Update BLE services if exposing new data
- [ ] **Test sensor flow**: Ensure integration with existing sensor management
- [ ] **Verify device tree**: Check initialization priorities if hardware changes

## ✅ Testing & Validation Protocol

### Mandatory Pre-PR Checks

```bash
# 1. Lint all modified files
get_errors /path/to/modified/files

# 2. Build verification (quick syntax check)
west build app --cmake-only

# 3. Full pristine build
west build -p always app

# 4. Verify memory usage is reasonable
# Check build output for flash/RAM usage
```

### Hardware Testing (if applicable)

```bash
# Flash and test on device
west flash
west attach  # Monitor serial output

# Check GPIO states if hardware modified
# Look for "board_print_pin_states()" output
```

### Integration Testing

- [ ] **Sensor readings**: Verify sensor data is accurate
- [ ] **BLE functionality**: Test with nRF Connect mobile app
- [ ] **Power consumption**: Check that low-power optimizations still work
- [ ] **Error handling**: Test error cases and recovery

## 📝 Pull Request Preparation

### Code Quality Checklist

- [ ] **No lint errors**: `get_errors` returns clean results
- [ ] **Builds successfully**: Both `--cmake-only` and full build pass
- [ ] **Follows patterns**: Code matches existing style and patterns
- [ ] **Proper logging**: Uses appropriate LOG levels
- [ ] **Documentation**: Functions have proper doc comments

### PR Description Template

```markdown
## Feature Implementation: [Feature Name]

### Changes Made
- [ ] Core implementation in `app/src/[files]`
- [ ] Header updates in `app/include/[files]`
- [ ] Configuration changes (if any)
- [ ] Device tree modifications (if any)

### Testing Performed
- [ ] Build verification: `west build -p always app`
- [ ] Hardware testing: Flash and serial output verification
- [ ] Integration testing: BLE services and sensor readings
- [ ] Power consumption: No regression in low-power optimizations

### Integration Points
- How this integrates with existing sensor_manager
- BLE service changes or additions
- Any hardware/GPIO dependencies

### Memory Impact
Flash: [X]KB / [Total]KB ([%]%)
RAM: [X]KB / [Total]KB ([%]%)
```

## 🚨 Critical Considerations

**Hardware Dependencies**:

- SX1509B GPIO expander initialization order is critical
- Device tree priorities: I2C (50) → SX1509B (60) → sensors (70+)
- Avoid circular regulator dependencies (`&vdd_pwr` not `&ccs_pwr`)

**Power Management**:

- Use one-shot sensor reading patterns
- Follow existing power-down cycles in sensor drivers
- Minimize continuous operation modes

**BLE Service Pattern**:

- On-demand sensor reading for responsive BLE requests
- Fallback to cached data if fresh readings fail
- Standard BLE service UUIDs and characteristics

**Error Handling**:

- Log errors but don't halt execution for non-critical failures
- Graceful degradation when sensors are unavailable
- Clear error messages for debugging

## 📚 Reference Documentation

- [Project Architecture](../.github/copilot-instructions.md)
- [Environment Setup](../.github/DEVELOPMENT_SETUP.md)
- [NCS Documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)
- [Zephyr Device Tree](https://docs.zephyrproject.org/latest/build/dts/index.html)
