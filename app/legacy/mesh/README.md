# Legacy Bluetooth Mesh build (archived)

This directory holds the previous Bluetooth Mesh implementation of the
environmental sensor. It is **not part of the active build** — the
production firmware uses the BLE GATT advertiser under `app/src/` instead.

These files are kept for historical reference and to make a future restore
straightforward, not for ongoing maintenance.

## Contents

- `main_mesh.c` — mesh-mode `main()` (replaced by `app/src/main.c`).
- `model_handler_env.c` + `model_handler.h` — Bluetooth Mesh sensor / battery
  models.
- `mesh.conf`, `thingy52_mesh.conf` — Kconfig fragments for a mesh build.

## Restoring

If you need to rebuild as a mesh device:

1. Move `main_mesh.c` and `model_handler_env.c` back to `app/src/`,
   `model_handler.h` back to `app/include/`, and the two `.conf` files to
   `app/` and `app/boards/` respectively.
2. Swap `src/main.c` for `src/main_mesh.c` in `app/CMakeLists.txt` and add
   `model_handler_env.c`.
3. Apply the mesh Kconfig fragment via `west build -- -DEXTRA_CONF_FILE=mesh.conf`.

See `BLE_MESH_CONFIG.md` at the repo root for the historical configuration
notes.
