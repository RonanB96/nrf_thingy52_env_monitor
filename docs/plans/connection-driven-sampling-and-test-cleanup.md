# Plan: connection-driven sampling + test/HIL cleanup

Status: implemented and verified on hardware (2026-05-09).
Owner: firmware.
Scope: `app/src/`, `app/include/`, `app/tests/`, `app/tests/hardware/`.

## Verification summary (2026-05-09)

| Gate | Command | Result |
|---|---|---|
| Firmware build | `west build -p always app -d app/build` | clean — FLASH 269748 B / 480 KB (54.88 %), RAM 43144 B / 64 KB (65.83 %) |
| Formatting | `clang-format --dry-run --Werror` over `app/src`, `app/include`, `app/tests/unit`, `app/tests/hardware/src` | pass (after one fix-up pass on migrated unit tests) |
| Static analysis | `CodeChecker analyze` + `parse` against `app/build/app/compile_commands.json`, project filter | 0 reports across 12 application TUs (clangsa + clang-tidy) |
| Unit tests | `twister -T app/tests/unit -p native_sim` | 2/2 suites, 17/17 cases — `battery_service` 9/9, `sensor_manager_policy` 8/8 |
| HIL on Thingy:52 | `twister ... --device-serial "$(scripts/resolve_serial_port.sh)" --west-flash="--runner=jlink"` | 1/1 suite, 8/8 cases — hts221 (read + DRDY trigger), lps22hb (read + DRDY GPIO IRQ), ccs811 (I2C presence), battery (vbatt ADC), power_rails (MPU/MIC off) |
| Boot smoke | `scripts/verify_hardware.sh` | pass — `Starting BLE Environmental Monitor`, `sensor_manager: Sensor manager initialized`, `ble_advertiser: Legacy advertising started successfully` all observed |

## Status by part

| Part | Title | Status |
|---|---|---|
| 1 | Connection-driven sampling (env on-connect, CCS811 always-on) | done — `sensor_manager_arm/_on_connected/_on_disconnected`, env-then-AQ on connect for fresh CCS811 compensation |
| 2 | Test suite migration to per-suite `ZTEST_SUITE` projects | done — `app/tests/unit/{battery_service,sensor_manager_policy}` with own CMake/prj/testcase, FFF mocks; old monolithic layout removed |
| 3 | Header / impl parity audit | done — guards normalised to `_H_`, no orphan declarations |
| 4 | Explicit init order with arm guard | done — drivers → settings → sensor_manager → BAS → ESS (registers cb) → `sensor_manager_arm()` → adv start |
| 5 | Dead code removal + mesh archival | done — `sensor_config.h` / `power_config.h` deleted, mesh files moved to `app/legacy/mesh/` |
| 6 | ESS metadata constants | already in place (per-characteristic structs) |
| 7 | Explicit BLE TX power 0 dBm | done — `CONFIG_BT_CTLR_TX_PWR_0=y` in `prj.conf` |
| 8 | Low-power budget doc | done — [docs/low_power.md](../low_power.md) records only verifiable configured operating points; numerical current/battery estimates intentionally absent until either local datasheets+schematic are checked in for citation or PPK captures are taken |
| 9 | Smaller cleanups (log demote, mutex on cb write, header guards, Kconfig trim) | done |

## Outstanding

- Add primary sources to the repo so the per-state current table in
  [docs/low_power.md](../low_power.md) can be filled with cited values:
  nRF52832 PS, HTS221, LPS22HB, CCS811, SX1509B datasheets and the Thingy:52
  PCA20020 schematic. Without those, no numerical current claims can be made.
- Capture average / peak current with a Power Profiler Kit II in the states
  listed in [docs/low_power.md](../low_power.md) and attach the raw `.ppk`.

---

## Original plan (kept for traceability)

## Goals

1. Stop wasting energy sampling sensors when no GATT client is connected.
   Only HTS221, LPS22HB and the battery monitor stop on disconnect.
   CCS811 stays powered (its baseline / conditioning state must not be lost).
2. Bring the unit-test and HIL-test suites into line with Zephyr / nRF Connect SDK
   recommended patterns (one test image per twister entry, ztest fixtures and
   `ZTEST_SUITE` over ad-hoc `setup/teardown`, no duplicated production code,
   shared test helpers extracted).

## Non-goals

- No change to BLE SIG wire formats or characteristic UUIDs.
- No change to CCS811 conditioning / baseline persistence behaviour.
- No new sensors, no mesh work.

---

## Part 1 — Connection-driven sampling

### Current behaviour (evidence)

- [app/src/main.c](app/src/main.c#L97) starts a periodic work item
  unconditionally: `sensor_manager_start_periodic(CONFIG_SENSOR_ENV_INTERVAL_SEC * MS_PER_SEC);`
- [app/src/sensor_manager.c](app/src/sensor_manager.c#L67) re-arms itself every
  `CONFIG_SENSOR_ENV_INTERVAL_SEC` (default 900 s) regardless of connection state.
- [app/src/ble_advertiser.c](app/src/ble_advertiser.c#L108) handles connect /
  disconnect but only toggles advertising — sampling is independent.
- HTS221 and LPS22HB already power-gate per read
  ([sensor_hts221_driver.c](app/src/sensor_hts221_driver.c),
   [sensor_lps22hb_driver.c](app/src/sensor_lps22hb_driver.c)).
- CCS811 uses a 20 min conditioning + 24 h baseline persistence
  ([sensor_ccs811_driver.c](app/src/sensor_ccs811_driver.c)) — must keep running
  when disconnected.

### Target behaviour

| State | HTS221 / LPS22HB | Battery ADC | CCS811 |
|-------|------------------|-------------|--------|
| Disconnected (advertising only) | not sampled | not sampled | sampled at its existing cadence |
| Connected | sampled on a fast cadence (e.g. ESS update interval) | sampled at slow cadence | sampled as today |
| Just connected | one immediate sample of all sensors before first notify | one immediate read | use latest cached value |

Notes:
- “Fast cadence on connect” must respect ESS Measurement Period (Finding 9 of
  the architecture review). Default to `CONFIG_SENSOR_ENV_INTERVAL_SEC` for now;
  add a separate `CONFIG_SENSOR_ENV_CONNECTED_INTERVAL_SEC` only if needed.
- Battery: keep the GPIO charge-state interrupt active in both states (it is
  edge-driven and effectively zero-cost). Only the periodic ADC sample is gated.
- CCS811 conditioning / baseline NVS save loop must remain unchanged.

### Design

Introduce two work items in `sensor_manager.c`:

1. `env_work` — temp/humidity/pressure/battery. Started on `connected`,
   stopped on `disconnected`.
2. `aq_work` — CCS811 air quality. Started in `sensor_manager_init()`,
   never stopped.

Replace the single `sensor_work` + `valid_mask`-driven omnibus update with two
narrow work handlers that share a common "publish" helper (avoid duplicating
the LOG_INF / callback / ESS-notify path).

### API changes

In [app/include/sensor_manager.h](app/include/sensor_manager.h):

- Add `int sensor_manager_on_connected(void);`  — kicks an immediate env
  sample and starts the env work item.
- Add `void sensor_manager_on_disconnected(void);` — cancels the env work item
  (CCS811 / aq_work untouched).
- Deprecate `sensor_manager_start_periodic()` for env data; keep it for AQ
  bring-up or rename to `sensor_manager_start_air_quality()`.

In [app/src/ble_advertiser.c](app/src/ble_advertiser.c):

- `ble_connected()` calls `sensor_manager_on_connected()`.
- `ble_disconnected()` calls `sensor_manager_on_disconnected()` before
  re-starting advertising.

In [app/src/main.c](app/src/main.c):

- Remove the unconditional `sensor_manager_start_periodic(...)` for env data.
- Start AQ work explicitly (`sensor_manager_start_air_quality(...)`).
- Document init order: drivers → callbacks registered → AQ started → BLE up.

### Edge cases / risks

- **First-notify timing.** GATT subscribe may arrive after `connected`. The
  immediate-on-connect sample plus the existing ESS callback path must both
  still trigger a notify when the client subscribes. Verify by adding a ztest
  for "subscribe after connect → first notify carries fresh data".
- **Multiple clients.** Connection count must drive start/stop, not the last
  callback. Use a small counter (`bt_conn_foreach(BT_CONN_TYPE_LE, …)` or an
  internal `connected_count`).
- **Battery while charging.** Keep the GPIO ISR active; it is the source of
  truth for charge state regardless of sampling.
- **CCS811 environmental compensation.** Today CCS811 is fed temp/humidity
  from the most recent HTS221 sample. With env sampling stopped between
  connections, compensation values go stale. Acceptable: CCS811 compensation
  is a slow drift correction; document this trade-off in the Kconfig help
  text and in `sensor_manager.c` header comment.

### Verification

- ztest: `sensor_manager_on_connected/disconnected` toggles env work,
  leaves AQ work alone (use Zephyr fake time / `k_sleep` boundary).
- ztest: connected→disconnected→connected within < env interval triggers
  exactly one extra env sample (the on-connect immediate one).
- HIL: measure idle current on Thingy52 (advertising only) before/after; expect
  reduction proportional to (env-interval / CCS811-interval).
- Build: `west build -p always app -d app/build` clean.

### Acceptance

- [ ] No env-sensor I2C traffic while disconnected (confirmed via HIL logs).
- [ ] CCS811 baseline NVS save still occurs every 24 h disconnected.
- [ ] First ESS notify after subscribe contains a sample taken ≤ 2 s old.
- [ ] All existing ztests still pass.

---

## Part 2 — Test suite cleanup (Zephyr / NCS conventions)

### Issues observed

1. **One binary, four "suites".** [app/tests/CMakeLists.txt](app/tests/CMakeLists.txt)
   compiles every `test_*.c` into a single `app` target. The four entries in
   [app/tests/testcase.yaml](app/tests/testcase.yaml) (`…tests.basic`,
   `…tests.sensor_manager`, `…tests.ble_services`, `…tests.mocks`,
   `…tests.sensor_manager_policy`) all run the same image — they only differ in
   tags. Twister convention is one project (one CMakeLists, own `prj.conf`)
   per `testcase.yaml` entry.
2. **Ad-hoc fixtures.** Tests use hand-written `setup/teardown` functions
   instead of `ZTEST_SUITE(name, predicate, setup, before, after, teardown)`
   with a typed fixture. Per-test state leaks between suites because mocks are
   global.
3. **Production code linked into tests.** `../src/ble_battery_service.c` is
   pulled into the test image
   ([app/tests/CMakeLists.txt](app/tests/CMakeLists.txt#L23)). That conflates
   "unit test" with "integration test" and forces the prj.conf to disable BT,
   sensor, GPIO, I2C, SPI to avoid link errors.
4. **Mock duplication.** `mock_battery_service.c` reimplements
   `battery_service_*` symbols, but production `battery_service.c` is *also*
   linkable; symbol clashes are avoided only by careful CMakeLists ordering.
   No clear "fakes for unit tests" vs "stubs for HIL" split.
5. **HIL tests duplicate driver setup.**
   [app/tests/hardware/src/](app/tests/hardware/src) — every `test_hil_*.c`
   re-derives device handles, power-up sequences, timing constants. A shared
   `hil_helpers.[ch]` is missing.
6. **No `Kconfig` / `_CONFIG.test` overlays.** Twister extra_configs are used
   to flip `CONFIG_ZTEST_VERBOSE_OUTPUT` etc. in line with NCS convention they
   should live in per-suite overlays (`overlay-verbose.conf`).
7. **README in `app/tests/`** duplicates `DEVELOPER.md` test section.

### Target structure

```
app/tests/
  unit/                              # native_sim, fully mocked, no Zephyr drivers
    sensor_manager/
      CMakeLists.txt
      prj.conf
      testcase.yaml
      src/main.c                     # ZTEST_SUITE registrations
    ess_service/
      ...
    battery_service/
      ...
    sensor_manager_policy/
      ...
  integration/                       # native_sim + fake drivers / Zephyr emul
    ble_services/
      ...
  hardware/                          # HIL on real Thingy52
    common/
      hil_helpers.c
      hil_helpers.h
    ccs811/
      ...
    hts221/
      ...
    lps22hb/
      ...
    battery/
      ...
    power_rails/
      ...
  mocks/                             # shared between unit suites only
    mock_i2c.[ch]
    mock_gpio.[ch]
    mock_sensor.[ch]
    mock_bluetooth.[ch]
```

Each leaf has its own `CMakeLists.txt`, `prj.conf`, `testcase.yaml`. Twister
discovers them naturally with `-T app/tests`.

### Conventions to enforce

- `ZTEST_SUITE(name, predicate, setup, before, after, teardown)` with a
  per-suite fixture struct. Use `ZTEST_F(name, test_name)` to access the fixture.
- `before` resets mock state; `after` asserts no leaked expectations.
- One mock = one translation unit, exposes a `mock_X_reset(void)` and a state
  struct. No mock owns its own globals across suites.
- Production source files are **not** added to test `target_sources`. If a unit
  test needs production code, extract the unit under test to a header-visible
  function and add only that file.
- HIL helpers (`hil_helpers.[ch]`): power-rail bring-up, device-ready waits,
  current-measurement entry points.
- `extra_configs` in `testcase.yaml` only for tags / verbosity; functional
  config goes in `overlay-*.conf`.
- Replace [app/tests/README.md](app/tests/README.md) cross-link with a single
  source of truth in [DEVELOPER.md](DEVELOPER.md).

### Migration steps

1. Carve out `app/tests/unit/sensor_manager/` from current
   `test_sensor_manager.c` + relevant mocks. Convert to `ZTEST_SUITE` with a
   fixture. Build green on `native_sim`.
2. Repeat for `ess_service`, `battery_service`, `sensor_manager_policy`.
3. Move `test_main.c` (framework smoke) into `unit/framework/` or delete if
   redundant once suites are real.
4. Promote `mock_battery_service.c` into a fixture-owned fake under
   `unit/ess_service/` (it is only used there). Delete the global file.
5. Refactor HIL: extract `hil_helpers.[ch]`; collapse duplicated setup in each
   `test_hil_*.c` to one call. Split into per-sensor folders with own
   `testcase.yaml`.
6. Audit production code that was linked into tests; revert each link by
   either (a) adding a real seam in production or (b) writing a focused fake.
7. Remove the old monolithic `app/tests/CMakeLists.txt` and top-level
   `testcase.yaml` once every test has migrated.
8. CI: run `twister -T app/tests -p native_sim --inline-logs`. HIL job
   continues to run only against `app/tests/hardware/`.

### Code-duplication targets to eliminate

- I2C transaction setup repeated in `test_sensor_manager.c`,
  `test_ess_service.c` → extract `mock_i2c_program_default_devices()` helper.
- Sensor "device ready" boilerplate → fixture `before` hook.
- HIL power-up delay / SX1509B reset / vdd_pwr enable → `hil_helpers.c`.
- Logging-format strings used in both production sensor_manager and tests →
  keep production-only.

### Verification

- `west build app/tests/unit/sensor_manager -d build/unit_sm -b native_sim -p`
  per suite — each builds in isolation.
- `twister -T app/tests -p native_sim` reports the same number of ztests as
  before (no silent drops) plus the new on-connect ztests from Part 1.
- `clang-tidy` / CodeChecker passes (existing tasks
  `Code Checker (CodeChecker)`).
- Coverage report shows ≥ existing line coverage for production modules; no
  regression.

### Acceptance

- [ ] Each `testcase.yaml` entry maps to a unique CMake project.
- [ ] No production `.c` files appear in test `target_sources`.
- [ ] All suites use `ZTEST_SUITE` with a typed fixture; no global mock state.
- [ ] HIL helpers shared via `hil_helpers.[ch]`; no duplicated power-up code.
- [ ] `twister -T app/tests -p native_sim --inline-logs` is green.
- [ ] Test count ≥ pre-migration count.

---

---

## Part 3 — Header / implementation parity audit

(Architecture review Finding 1.)

### Issue

[app/include/sensor_manager.h](app/include/sensor_manager.h) declares public
symbols (`sensor_manager_get_temperature`, `sensor_manager_is_ccs811_ready`,
`sensor_manager_update_air_quality_for_ble`, and possibly others) whose
definitions are not visibly present in
[app/src/sensor_manager.c](app/src/sensor_manager.c). Either the header is
ahead of the implementation (link-time orphan declarations) or the
implementations exist but are scattered. Both are bad for review and for
LTO/dead-code elimination.

### Steps

1. Generate the symbol list from each public header in `app/include/`:
   `nm` over the build, plus a grep of declarations.
2. For each declared symbol, confirm a definition exists. For each defined
   public symbol, confirm a declaration exists.
3. Remove orphan declarations; add missing declarations; move any
   inadvertently-public helpers back to `static` in the .c.
4. Add `clang-tidy` rule (or a small CI script) that fails if a public
   declaration has no matching definition.

### Acceptance

- [ ] Every declaration in `app/include/*.h` has exactly one matching
  definition in `app/src/`.
- [ ] No `static` helper is duplicated as a public declaration.
- [ ] CI gate prevents regression.

---

## Part 4 — Explicit init order with guard

(Architecture review Finding 2.)

### Issue

[app/src/main.c](app/src/main.c) hand-orders init: ESS callback registration
must happen before periodic work starts, otherwise the first sample fires
into a null callback and the first ESS notify is lost. Reordering the file
silently breaks this.

### Steps

1. Introduce two phases in `sensor_manager.c`: `_init()` (drivers + state)
   and `_arm()` (registers callbacks-driven path and starts work). `_arm()`
   refuses to run unless `_init()` succeeded *and* at least one callback is
   registered, returning `-EINVAL` otherwise.
2. Replace any current `start_periodic` call site with the explicit
   `_arm()` after callbacks are registered (combines naturally with Part 1
   AQ-only periodic work).
3. Add ztest: calling `_arm()` before registering callbacks returns
   `-EINVAL`; calling after succeeds.

### Acceptance

- [ ] `sensor_manager_arm()` returns error if pre-conditions are not met.
- [ ] `main.c` documents the phased order in a top-of-file comment.
- [ ] ztest covers the pre-condition failure path.

---

## Part 5 — Dead code & legacy mesh archival

(Architecture review Findings 5, 6, 15.)

### Steps

1. Delete unused headers:
   - [app/include/sensor_config.h](app/include/sensor_config.h) (unused
     `SENSOR_SAMPLING_INTERVAL_SEC`).
   - [app/include/power_config.h](app/include/power_config.h) (mesh-only).
   Confirm zero references via `grep_search` before deletion.
2. Move legacy mesh sources:
   - [app/src/main_mesh.c](app/src/main_mesh.c)
   - [app/src/model_handler_env.c](app/src/model_handler_env.c)
   - [app/boards/thingy52_mesh.conf](app/boards/thingy52_mesh.conf)
   - [app/mesh.conf](app/mesh.conf)
   to `app/legacy/mesh/` with a `README.md` explaining archival status and
   restore steps. Ensure CMake / `west.yml` / `sample.yaml` paths still
   match (they should, since these files are not in `target_sources`).
3. Update [app/sample.yaml](app/sample.yaml): replace the "Bluetooth Mesh
   sensor sample" description with the active BLE ESS description; correct
   `sample.name` and `sample.description`.
4. Update [BLE_MESH_CONFIG.md](BLE_MESH_CONFIG.md) to reflect new paths.

### Acceptance

- [ ] `grep` returns zero hits for `sensor_config.h` / `power_config.h`.
- [ ] `app/src/` and `app/include/` contain only active code.
- [ ] `west build -p always app` is unaffected.
- [ ] `sample.yaml` matches built firmware.

---

## Part 6 — Promote ESS metadata to named constants

(Architecture review Finding 9.)

### Issue

[app/src/ess_service.c](app/src/ess_service.c#L153) hardcodes per-characteristic
SIG metadata (sampling function, measurement period, update interval,
uncertainty) inline in the GATT-service definition. Hard to review for spec
compliance.

### Steps

1. Define `static const struct ess_meas_metadata` per characteristic at file
   scope, citing the SIG ESS spec section for each value.
2. Replace inline literals in the descriptor read paths with field reads.
3. Where a value is also a Kconfig (e.g. measurement period derived from
   `CONFIG_SENSOR_ENV_INTERVAL_SEC`), wire it through.
4. Adjust ztests in Part 2 (ess_service suite) to assert each descriptor
   returns the metadata struct's value.

### Acceptance

- [ ] No inline numeric SIG metadata in `BT_GATT_SERVICE_DEFINE`.
- [ ] One named struct per characteristic, with a comment citing the spec.
- [ ] ztest validates all descriptor reads.

---

## Part 7 — Explicit BLE TX power + documented power budget

(Architecture review Findings 10, 14.)

### Steps

1. Set TX power deliberately in [app/prj.conf](app/prj.conf):
   `CONFIG_BT_CTLR_TX_PWR_0` (or `_MINUS_4`) chosen against measured
   advertising current. Document the choice in a comment and in
   `docs/low_power.md` (new file, Part 8).
2. Add Kconfig help text noting the trade-off (range vs current).

### Acceptance

- [ ] `CONFIG_BT_CTLR_TX_PWR_*` set and commented.
- [ ] Documented current measurement at chosen TX power.

---

## Part 8 — Centralised low-power budget doc

(Architecture review Finding 10.)

### Steps

1. Create `docs/low_power.md` with:
   - Advertising interval + TX power.
   - Connected vs disconnected sampling cadence (links to Part 1).
   - Per-sensor on-time and current (HTS221, LPS22HB, CCS811, ADC).
   - CCS811 conditioning + 24 h baseline write cost.
   - Estimated battery life on 1000 mAh cell, with measurement methodology.
2. Add a top-of-file comment in
   [app/src/sensor_manager.c](app/src/sensor_manager.c) summarising the
   budget and pointing to `docs/low_power.md`.
3. Cross-link from [README.md](README.md) and [DEVELOPER.md](DEVELOPER.md).

### Acceptance

- [ ] `docs/low_power.md` exists and is current.
- [ ] Sensor manager file header references it.

---

## Part 9 — Smaller cleanups

(Architecture review Findings 7, 8, 11, 12, 13.)

### Steps

1. **Sensor registration table** (Finding 7). Introduce
   `static const struct sensor_drv { const char *name; int (*init)(void);
   int (*read)(struct sensor_data *); } drivers[];` in `sensor_manager.c`.
   Iterate it in init / read instead of hard-coded calls. Keeps the file
   open-closed for future sensors and simplifies tests.
2. **Mutex-guard callback registration** (Finding 8). Wrap the assignment
   to `_callback` with `sensor_manager_mutex` and document the contract
   in the header.
3. **DT-aware twister run** (Finding 11). Add a twister entry that loads
   `app/boards/thingy52.overlay` under `native_sim` (or a `posix`-friendly
   subset) so SX1509B-hog / CCS811 vin-supply regressions surface in CI.
   If full DT replay is impractical, add a HIL smoke test in
   `app/tests/hardware/power_rails/` instead.
4. **Header guard convention** (Finding 12). Standardise on
   `MODULE_NAME_H_` across `app/include/`. One sweeping pass + clang-tidy
   check.
5. **Demote per-cycle LOG_INF** (Finding 13). Change the per-sample
   summary in [app/src/sensor_manager.c](app/src/sensor_manager.c#L294)
   to `LOG_DBG`, or gate via `CONFIG_SENSOR_MANAGER_LOG_SAMPLES`
   (default `n` for production).

### Acceptance

- [ ] `sensor_manager.c` uses a driver table.
- [ ] `_callback` write is mutex-protected; header documents the contract.
- [ ] CI has at least one DT-bringup gate (twister or HIL).
- [ ] All public headers use `MODULE_NAME_H_` guards.
- [ ] No per-sample `LOG_INF` in default build.

---

## Sequencing

1. **Part 1** — connection-driven sampling, behind
   `CONFIG_SENSOR_SAMPLE_ON_CONNECTION=y`. Reviewable, reversible.
2. **Part 4** — explicit init-order guard. Required before Part 1 lands so
   the new connect/disconnect flow has a verified pre-condition.
3. **Part 3** — header/impl parity audit. Cleans `sensor_manager.h` before
   Part 2 builds new tests against it.
4. **Part 2 step 1** — migrate one unit suite as a template.
5. **Part 5** — dead code + mesh archival. Independent, safe to land any time
   after Part 3.
6. **Part 6** — ESS metadata constants. Pairs with the new ess_service unit
   suite in Part 2.
7. **Part 2 remainder** — migrate the rest of the unit suites and HIL.
8. **Part 9** — smaller cleanups, in any order.
9. **Part 7 + Part 8** — TX power + low-power doc. Land last so the doc can
   cite real measurements taken with all prior changes in place.
10. Delete the `CONFIG_SENSOR_SAMPLE_ON_CONNECTION` feature flag once Part 1
    has been stable for one release.
