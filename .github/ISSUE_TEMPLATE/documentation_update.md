---
name: Documentation Update (AI Agents)
about: Template for AI coding agents to update or create documentation
title: '[DOCS] '
labels: ['documentation', 'ai-agent']
assignees: ''
---

## 📚 Documentation Update Template for AI Agents

### Environment Setup

**Prerequisites**: Follow [DEVELOPMENT_SETUP.md](../.github/DEVELOPMENT_SETUP.md) if not already configured.

**No build environment needed for documentation-only changes.**

## 📝 Documentation Task

**Type of Documentation Update**:

- [ ] API Documentation (code comments, headers)
- [ ] User Guide (README, setup instructions)
- [ ] Developer Guide (architecture, workflows)
- [ ] Configuration Guide (device tree, Kconfig)
- [ ] Troubleshooting Guide (common issues, solutions)
- [ ] GitHub Templates (issue templates, PR templates)

**Scope**:

- [ ] New documentation creation
- [ ] Existing documentation update
- [ ] Documentation restructuring
- [ ] Markdown formatting fixes

## 🎯 Documentation Objectives

**Primary Goal**:
<!-- What needs to be documented or updated? -->

**Target Audience**:

- [ ] End users (device setup, usage)
- [ ] Developers (code contribution)
- [ ] AI coding agents (workflow templates)
- [ ] Hardware engineers (device tree, power)
- [ ] Maintainers (release processes)

**Success Criteria**:

- [ ] Information is accurate and up-to-date
- [ ] Content is clear and well-structured
- [ ] Formatting follows project standards
- [ ] Links and references work correctly

## 🔍 Research & Analysis Phase

### Content Audit

```bash
# Find existing documentation
file_search "**/*.md"
file_search "**/README*"
file_search "**/*.rst"

# Search for relevant content
semantic_search "topic keywords"
grep_search "specific terms|patterns" --isRegexp true

# Check current documentation structure
list_dir /path/to/docs/directory
```

### Information Gathering

- [ ] **Read existing documentation**: Understand current content and structure
- [ ] **Review related code**: Ensure documentation matches implementation
- [ ] **Check for outdated information**: Verify accuracy of existing content
- [ ] **Identify gaps**: What information is missing or incomplete?

### Reference Material Analysis

```bash
# Find code examples and patterns
semantic_search "implementation examples"

# Check configuration files for documentation
read_file app/prj.conf
read_file boards/thingy52.conf
read_file boards/thingy52.overlay

# Review headers for API documentation
read_file app/include/[relevant_headers]
```

## ✍️ Content Creation Workflow

### Phase 1: Planning

- [ ] **Outline structure**: Plan sections, headings, and flow
- [ ] **Identify examples**: Code samples, configurations, commands
- [ ] **Plan formatting**: Decide on markdown style, code blocks, tables
- [ ] **Reference gathering**: Collect links, citations, related docs

### Phase 2: Content Development

- [ ] **Write clear headings**: Use descriptive, hierarchical structure
- [ ] **Include practical examples**: Real commands, code snippets, configurations
- [ ] **Add context**: Explain why, not just how
- [ ] **Cross-reference**: Link to related documentation and code

### Phase 3: Quality Assurance

- [ ] **Accuracy check**: Verify all technical information is correct
- [ ] **Completeness check**: Ensure all necessary information is included
- [ ] **Clarity check**: Review for readability and understanding
- [ ] **Consistency check**: Match project style and terminology

## 📋 Documentation Standards

### Markdown Formatting

**File Structure**:

```markdown
# Main Title (H1)

## Section Title (H2)

### Subsection (H3)

Content with proper spacing around headings and lists.

- List items with consistent formatting
- Nested items properly indented

```bash
# Code blocks with language specification
command --with-arguments
```

**Note**: Remember proper spacing around fenced code blocks.

```markdown
Example content
```

**Style Guidelines**:

- Use British English spelling
- Include table of contents for long documents
- Use descriptive link text (not "click here")
- Include prerequisites and assumptions
- Provide both quick start and detailed instructions

### Code Documentation

**API Documentation**:

```c
/**
 * @brief Brief description of function
 *
 * @param param1 Description of parameter
 * @param param2 Description of parameter
 * @return Description of return value
 *
 * @note Important notes or warnings
 * @example
 * // Example usage
 * result = function_name(arg1, arg2);
 */
```

**Configuration Documentation**:

```yaml
# boards/thingy52.conf
# Purpose: Thingy:52 specific sensor configurations
CONFIG_SENSOR_POWER_MGMT=y  # Enable power management for sensors
```

### Command Documentation

**Format**:

```bash
# Purpose: Brief description of what command does
west build app                    # Build application
west build -p always app         # Pristine build (recommended for config changes)
west flash                       # Flash to connected device
```

## 🛠️ Common Documentation Tasks

### API Reference Updates

- [ ] **Header file documentation**: Update function/struct comments
- [ ] **Parameter descriptions**: Document all parameters and return values
- [ ] **Usage examples**: Include practical code examples
- [ ] **Error conditions**: Document possible failure modes

### User Guide Creation/Updates

- [ ] **Prerequisites section**: System requirements, dependencies
- [ ] **Step-by-step instructions**: Clear, numbered procedures
- [ ] **Screenshots/diagrams**: Visual aids where helpful
- [ ] **Troubleshooting section**: Common issues and solutions

### Developer Documentation

- [ ] **Architecture overview**: System design and component interaction
- [ ] **Build instructions**: Environment setup and build commands
- [ ] **Contributing guidelines**: Code style, PR process, testing
- [ ] **Development workflows**: Common development tasks

### Configuration Guides

- [ ] **Device tree documentation**: Hardware configuration explanations
- [ ] **Kconfig options**: Configuration parameter descriptions
- [ ] **Board-specific notes**: Hardware-specific considerations
- [ ] **Power management**: Power consumption and optimisation

## ✅ Validation & Testing

### Content Verification

- [ ] **Technical accuracy**: All commands, code, and configurations are correct
- [ ] **Link validation**: All internal and external links work
- [ ] **Example testing**: Code examples compile and run successfully
- [ ] **Procedure testing**: Step-by-step instructions actually work

### Formatting Validation

```bash
# Check markdown formatting
get_errors /path/to/documentation/files

# Verify no broken links
grep_search "\[.*\]\(.*\)" --isRegexp true
```

### Integration Testing

- [ ] **Navigation testing**: Table of contents and cross-references work
- [ ] **Mobile/web rendering**: Documentation displays correctly
- [ ] **Search functionality**: Key terms are discoverable
- [ ] **Print formatting**: Documentation prints well if needed

## 📝 Pull Request Documentation

### Documentation Change Description

```markdown
## Documentation Update: [Brief Summary]

### Changes Made
- [ ] New documentation: `[file path]` - [purpose]
- [ ] Updated documentation: `[file path]` - [changes]
- [ ] Formatting fixes: `[file path]` - [improvements]
- [ ] Link updates: Updated broken/outdated links

### Content Overview
- **Scope**: [What topics/areas covered]
- **Audience**: [Who this documentation serves]
- **Format**: [Markdown/code comments/etc.]

### Validation Performed
- [ ] Technical accuracy: All examples and commands verified
- [ ] Formatting check: Markdown linting passed
- [ ] Link validation: All links tested and working
- [ ] Cross-reference check: Related docs updated consistently

### Files Added/Modified
- `[path]` - [description of changes]
- `[path]` - [description of changes]

### Review Notes
- **Priority**: [High/Medium/Low]
- **Review focus**: [What reviewers should pay attention to]
- **Testing notes**: [Any special testing requirements]
```

## 📚 Documentation Templates

### README Structure

```markdown
# Project Name

Brief project description and purpose.

## Quick Start

Minimal steps to get running.

## Prerequisites

System requirements and dependencies.

## Installation

Detailed setup instructions.

## Usage

Basic usage examples and common tasks.

## Configuration

Configuration options and customisation.

## Troubleshooting

Common issues and solutions.

## Contributing

How to contribute to the project.

## License

License information.
```

### API Documentation Template

```markdown
# Module Name API

## Overview

Purpose and scope of the module.

## Functions

### function_name()

**Purpose**: Brief description

**Syntax**:
```c
return_type function_name(param_type param1, param_type param2);
```

**Parameters**:

- `param1`: Description of parameter
- `param2`: Description of parameter

**Returns**: Description of return value

**Example**:

```c
// Example usage
result = function_name(value1, value2);
```

**Notes**: Important notes or warnings

```markdown
Example content
```

## 🚨 Common Documentation Pitfalls

### Content Issues

- **Outdated information**: Always verify current accuracy
- **Missing context**: Explain assumptions and prerequisites
- **Too technical/not technical enough**: Match audience needs
- **Incomplete examples**: Provide full, working examples

### Formatting Issues

- **Inconsistent markdown**: Follow project markdown standards
- **Missing language tags**: Specify language for code blocks
- **Poor heading hierarchy**: Use logical H1/H2/H3 structure
- **Broken links**: Test all links before submitting

### Structure Issues

- **No table of contents**: Include TOC for long documents
- **Poor navigation**: Provide clear cross-references
- **Missing prerequisites**: Always document requirements
- **No troubleshooting**: Include common issues and solutions

## 📖 Reference Resources

### Project Documentation

- [Project Architecture](../.github/copilot-instructions.md)
- [Development Setup](../.github/DEVELOPMENT_SETUP.md)
- [Existing Documentation Files](../../README.md)

### Style Guides

- [GitHub Markdown Guide](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax)
- [Nordic Documentation Style](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/doc_styleguide.html)
- [Zephyr Documentation](https://docs.zephyrproject.org/latest/contribute/documentation/index.html)

### Technical References

- [NCS Documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)
- [Zephyr Project Documentation](https://docs.zephyrproject.org/latest/index.html)
- [Thingy:52 Hardware Guide](https://docs.nordicsemi.com/bundle/ug_thingy52/page/UG/thingy52/intro.html)
