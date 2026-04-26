# Nordic Thingy:52 Copilot Instructions

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
- Run a project build after code or configuration changes.
- Use a pristine build when device tree, Kconfig, or CMake files are changed.

## Detailed Rules

- Use targeted files under `.github/instructions/` for language-specific and subsystem-specific rules.