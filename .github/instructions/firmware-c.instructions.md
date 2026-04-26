---
name: Firmware C and Headers
description: Use when editing application C or header files for sensor, BLE service, board, or power-management logic.
applyTo: "*.c,*.h"
---
# C Firmware Rules

- Preserve the existing Zephyr-style logging and error propagation patterns already used in each module.
- Keep sensor access routed through existing manager/driver layers rather than bypassing established boundaries.
- Maintain low-power intent: avoid introducing continuous sampling or always-on polling unless explicitly requested.
- Keep BLE characteristic behaviour consistent with existing service interfaces and data flow.
- Prefer incremental changes that preserve existing public interfaces unless the task explicitly requires API changes.
