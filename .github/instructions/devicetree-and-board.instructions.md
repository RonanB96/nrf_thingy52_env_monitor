---
name: Thingy52 Devicetree and Board Config
description: Use when editing Thingy52 overlays, devicetree sources, or board configuration tied to sensor power and bring-up ordering.
applyTo: "*.overlay,*.dts,*.dtsi,**/*.dts,**/*.dtsi"
---
# Devicetree and Board Rules

- Preserve bring-up ordering dependencies across I2C, GPIO expander initialisation, and sensors.
- Avoid dependency cycles in power routing; keep regulator and supply references simple and acyclic.
- Keep changes board-specific where possible instead of broad global defaults.
- Treat SX1509B-related routing as critical infrastructure for CCS811 control and sensor availability.
- Verify that edits are compatible with the intended Thingy:52 board target.
