# Nordic Thingy:52 Copilot Instructions

> **Cursor users:** equivalent rules live in [`AGENTS.md`](../AGENTS.md) and [`.cursor/rules/`](../.cursor/rules/).

## Scope

- This repository targets Nordic nRF Connect SDK firmware for Thingy:52 environmental monitoring over BLE advertising.
- Keep changes aligned with low-power operation and stable sensor bring-up.
- Treat mesh-related files as legacy unless a task explicitly asks for mesh work.

## Global Expectations

- Keep changes minimal and in-scope for the request.
- Research and abide by best practices for Zephyr, nRF Connect SDK, and embedded C development.
- Follow existing project patterns before introducing new abstractions.
- Prefer durable guidance over task scripts in instruction files.
- Avoid adding static command walkthroughs or hardware-specific one-off values to always-on guidance.

## Validation

- Validate edited files for errors before finishing.
- **A task is not complete until relevant tests have been run and pass.** Do not mark work done after edits alone.
- Run the most specific test available for the change:
  - Firmware or config: `west build` (pristine when device tree, Kconfig, or CMake changed).
  - Python test helpers or clients: `pytest` for the affected module or test file.
  - Hardware-dependent behavior: follow hardware-hil instructions and run the matching HIL/pytest path when hardware is available; otherwise run offline/unit coverage and state what still needs HIL.
- Report what was tested (command and outcome) in the completion summary.
- Use a pristine build when device tree, Kconfig, or CMake files are changed.

## Detailed Rules

- Use targeted files under `.github/instructions/` (Copilot) or `.cursor/rules/` (Cursor) for language-specific and subsystem-specific rules.
- Before any flash, reset, UART capture, BLE connection check, or other live-device/HIL action, consult the hardware-hil instructions (`.github/instructions/hardware-hil.instructions.md` or `.cursor/rules/hardware-hil.mdc`) and `DEVELOPER.md`
- For HIL or live-device tasks, treat device logs as event-driven rather than continuous: expect meaningful UART output at boot and around connection-driven activity, not during idle advertising.
- For UART-based HIL work, start a parallel serial monitor before flash, reset, or connect actions and keep it running while the event is triggered; do not inspect idle logs repeatedly when no new boot or connection event has occurred.
- User documentation in README.md and DEVELOPER.md
