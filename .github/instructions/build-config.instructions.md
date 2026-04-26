---
name: Build and Config Files
description: Use when editing CMake, Kconfig, west manifest, project config, or board .conf files.
applyTo: CMakeLists.txt,Kconfig.*,prj.conf,*.conf,west.yml,*.cmake"
---
# Build and Configuration Rules

- Keep configuration changes narrowly scoped to the requested board or feature.
- Use board-level .conf files only for board-specific differences; put shared behaviour in base project config (for example, `prj.conf`) or shared Kconfig.
- Preserve compatibility with the repository's existing west workspace and nRF Connect SDK setup.
- Re-check build assumptions after config changes rather than relying on previous build state.
- Use pristine rebuild validation when changing Kconfig, devicetree, or CMake-related behaviour.
