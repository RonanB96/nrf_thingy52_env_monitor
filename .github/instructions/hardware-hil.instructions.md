---
name: Hardware-in-the-loop and device interaction
description: Use whenever a task involves hardware-in-the-loop, UART, BLE connection checks, flashing, boot-log capture, or any live device interaction, including when code edits are in unrelated source files but the next step is hardware validation.
---
# HIL and Device Interaction Rules

- For this project, treat device logs as event-driven. Expect meaningful serial output during boot and connection-driven activity; do not expect idle advertising to emit continuous logs.
- When using UART, always run a parallel serial monitor before any flash, reset, power-cycle, or BLE connection action that is meant to produce logs.
- Keep the serial monitor running while the event is triggered. Starting the monitor after flash, reset, or connect is too late for boot logs and can miss short connection-related output.
- Use separate terminals or equivalent parallel execution: one terminal for the serial monitor, one terminal for flash, reset, connect, or test actions.
- Do not keep rereading the same idle UART session when no new boot or connection event has occurred. If nothing new has happened on the device, gather a new event first.
- If a task depends on UART evidence and no parallel serial monitor is active yet, set up the monitor before diagnosing from logs.
- Prefer project documentation for exact procedures. Use DEVELOPER.md for the boot-log capture workflow and hardware verification flow.