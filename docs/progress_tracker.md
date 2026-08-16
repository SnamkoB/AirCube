# Progress Tracker

## Current project status

- Available source snapshot implements firmware `2.0.1`, desktop utilities, PCB/mechanical assets, ZHA/Zigbee2MQTT converters, and a SmartThings Edge driver.
- Git metadata is available again: last verified commit `e9b6b38` ("New FW release with support for SCD4x, more data to HA and auto calibration"); remotes and full tracked-state verification remain outstanding.
- No automated test suite or CI workflow is present. Firmware builds successfully with ESP-IDF v6.0.2; hardware/platform behavior remains unverified.

## Current documentation status

- Comprehensive repository documentation was created and source-checked on 2026-08-05 against the available filesystem snapshot.
- `docs/INDEX.md` is the navigation authority. Existing root guides were preserved and linked, not replaced.
- Required documentation and skill-maintenance skills exist. No progress archive is needed yet.

## Next recommended actions

1. Restore or obtain a real Git checkout, record branch/commit/remotes/dirty state, run `git ls-files`, and replace the index's unavailable verification point.
2. Flash and smoke-test the v6.0.2 build on Base and Pro hardware; decide whether v6.0.2 becomes the documented project baseline.
3. Add host-testable fixtures for VOC mapping, serial parsing, history indexing, and client protocol compatibility.
4. Validate Base and Pro hardware, especially partial optional-sensor failures, BLE history at default MTU, Zigbee reports, and auto-dim thresholds.
5. Reconcile user-facing documentation defaults/version language and expose Pro Zigbee entities only after platform testing.

## Active blockers and unresolved questions

- `docs/INDEX.md` still links to three documents deleted in commit `e9b6b38` (`protocols_and_integrations.md`, `hardware_and_tools.md`, `BLE_GATT_PROTOCOL.md`); recorded in [known issues](known_issues.md) and pending a documentation reconciliation pass.
- The available KiCad design has no SCD41 or VCNL4040; Pro electrical source/provenance is unknown.
- No AirCube hardware or platform runtimes were available for firmware, BLE, Zigbee, ZHA, Z2M, or SmartThings validation.
- ESP-IDF v5.3.4's EIM-installed Windows compiler is corrupt (`cc1.exe` missing); v6.0.2 builds successfully, but minimum-version compatibility remains unverified.
- Binary ZIP, STEP, STL, 3MF, Gerber, and JPEG assets were catalogued but not deeply inspected.

## Recent important discoveries

- BLE and Zigbee are mutually exclusive per boot; `ble_gatt.c` owns current BTHome advertising while `ble_bthome.c` is not built.
- History is seven days of five-minute, 32-byte summaries despite stale source comments mentioning 10 minutes or 14 days.
- All supplied Zigbee integrations omit Pro true CO2 (`0x040D`) and illuminance (`0x0400`).
- Pro detection succeeds if either SCD41 or VCNL4040 responds, which can route all Pro data through an absent SCD41.
- Serial parsing is deliberately compact/prefix-based rather than general JSON; producer formatting is part of the practical contract.
- See [known issues](known_issues.md) for the complete prioritized list.

## Recent work log

### 2026-08-16 - OpenCode SCD4x data-ready fix and first hardware smoke test

- **Objective:** Fix regression reported from field testing: no CO2 data after the data-ready read change (`connected:false`, 0 ppm forever).
- **Files examined or changed:** `firmware/main/scd41.c` (data-ready mask + status transition log); `firmware/main/ens210.c` (first-sample rate-gate fix). Datasheets consulted: Sensirion SCD4x v1.5 (July 2023), ScioSense ENS210 v6 (2025-11-18).
- **Work completed:** Root cause: `get_data_ready_status` (0xE4B8) signals ready when the **least significant 11 bits** of the response word are non-zero (datasheet §3.8.2; idle value 0x8000) — the code tested only bit 0, which is not set on this unit. Changed the test to `(status & 0x07FF) != 0` and added a one-time status-transition log. Also fixed the ENS210 first-sample gate so the initial sample is taken immediately instead of waiting 5 s. Flashed to COM3 and smoke-tested.
- **Validation performed:** ESP-IDF v6.0.2 clean build and `merge-bin` (zero warnings); flash to COM3 with verified hashes; 25 s serial capture.
- **Important discoveries:** Observed SCD40 data-ready values: `0x8000` = not ready, `0x8006` = ready. CO2 recovered (514/519/505 ppm), `connected:true`, ~5 s update cadence. ENS210 single-shot samples every 5 s with CRC-7 validation passing. App image `0x7ec40` bytes with 70% partition headroom; merged image `0x8ec40` bytes.
- **Decisions and rationale:** Followed the datasheet's 11-bit rule instead of hardcoding the observed 0x8006 pattern; kept the status-transition log at INFO (one line per change) as permanent diagnostics.
- **Artifact:** `output/firmware/AirCube_firmware_v2.0.1.bin`, 584768 bytes; SHA-256 `BC9D5ACD5F8A033AAD0A7938F0B9569F0B4BD6E8CC9E1330B35344505CA4E3B1`; merged image is ready for flash offset `0x0`.
- **Remaining work:** Long-run soak, HA/Zigbee verification of ENS210 reporting, FRC button behavior test in HA.
- **Documentation updated:** `docs/build_and_test.md`, `docs/progress_tracker.md`.

### 2026-08-16 - OpenCode ENS210 datasheet-compliant single-shot readout

- **Objective:** Make the ENS210 follow the datasheet-recommended usage and align its sample rate with the SCD40 (~5 s).
- **Files examined or changed:** `firmware/main/ens210.c` (rewritten readout path); datasheet `ENS210 SC-000897-DS-12 Version 6 (2025-11-18)` consulted; `firmware/main/main.c` unchanged.
- **Work completed:** Replaced the continuous-mode (SENS_RUN=0b11, low power off) read-every-loop-tick behavior with the datasheet's recommended single-shot flow: trigger `0x03` via SENS_START, poll SENS_STAT (T_STAT/H_STAT) for completion (~122-130 ms typical, 250 ms timeout), read T_VAL+H_VAL in one 6-byte transaction, and validate the valid flag plus CRC-7 (poly 0x89, init 0x7F) before publishing. SENS_RUN is set to single-shot and SYS_CTRL LOW_POWER=1 so the device returns to standby between conversions (less self-heating). `ens210_read_envir()` rate-limits to 5 s so the sensor is sampled at the same cadence as the SCD4x; between samples the last validated values are kept.
- **Validation performed:** ESP-IDF v6.0.2 clean build (`fullclean`, `set-target esp32h2`, `build`) and `merge-bin`; complete log review; dependency lock verified (ZBOSS 1.6.4, Zigbee 1.6.7, IDF 6.0.2); published artifact size/hash checks.
- **Important discoveries:** ENS210 has no SCD40-style data-ready command; its completion status is SENS_STAT (0x24) and its fresh-data marker is the VALID bit in T_VAL/H_VAL. Datasheet timings: T+RH single-shot 122-130 ms, continuous 225-238 ms. Clean build has no project warnings; only the three known external Kconfig notes remain. App image `0x7ebb0` bytes with 70% partition headroom; merged image `0x8ebb0` bytes.
- **Decisions and rationale:** Single-shot with low power enabled per the datasheet example; 5 s cadence matches the SCD4x read rate. The conversion blocks the sensor task for at most ~140 ms once per 5 s, which is acceptable for the WDT.
- **Artifact:** `output/firmware/AirCube_firmware_v2.0.1.bin`, 584624 bytes; SHA-256 `95B5A94D3343FFB26ACE8A54093C9CAD85D4D21ADF33FA7C1D2D42974F662B79`; merged image is ready for flash offset `0x0`.
- **Remaining work:** Hardware test to confirm conversion timing, CRC-7 matching on the real sensor, and 5 s sample cadence.
- **Documentation updated:** `docs/architecture.md`, `docs/build_and_test.md`, `docs/known_issues.md`, `docs/progress_tracker.md`.

### 2026-08-16 - OpenCode SCD40 data-ready read handshake

- **Objective:** Change how the SCD40 is read: keep the sensor's natural `start_periodic_measurement()` cadence (~5 s) and only read when a new sample is available, using the datasheet data-ready flag instead of time-gated polling.
- **Files examined or changed:** `firmware/main/scd41.c` (new `SCD41_CMD_GET_DATA_READY_STATUS` 0xE4B8, `scd41_data_ready()`, reworked `scd41_poll()`, removed unused `scd41_last_*_word` stores); `firmware/main/main.c` and `scd41.h` unchanged.
- **Work completed:** `scd41_poll()` now rate-limits status polls to at most 1/s, checks the data-ready flag, and issues `read_measurement` only when the sensor reports a fresh sample (the flag clears on read, so each ~5 s sample is consumed exactly once). The firmware therefore reads at the sensor's default 5 s cadence; reading less frequently (e.g., 10 s) would also be safe because every read returns the newest buffered sample.
- **Validation performed:** ESP-IDF v6.0.2 clean build (`fullclean`, `set-target esp32h2`, `build`) and `merge-bin`; complete log review; dependency lock verified (ZBOSS 1.6.4, Zigbee 1.6.7, IDF 6.0.2); published artifact size/hash checks.
- **Important discoveries:** Clean build has no project warnings; only the three known external Kconfig notes remain. App image `0x7e8d0` bytes with 70% partition headroom; merged image `0x8e8d0` bytes.
- **Decisions and rationale:** Kept the 5 s natural cadence (user's preferred option) rather than a 10 s read period; the data-ready handshake makes both safe. Kept the `sensor_task` loop and serial JSON cadence unchanged.
- **Artifact:** `output/firmware/AirCube_firmware_v2.0.1.bin`, 583888 bytes; SHA-256 `B1423B0620A956AF8120D95095D36BA2199DC7BBA0429F4A07F234F164C52D4D`; merged image is ready for flash offset `0x0`.
- **Remaining work:** Hardware test to confirm ~5 s sample cadence and flag clearing behavior on the target SCD40/SCD41.
- **Documentation updated:** `docs/architecture.md`, `docs/build_and_test.md`, `docs/progress_tracker.md`.

### 2026-08-16 - OpenCode Zigbee calibration/ENS210 reporting fixes

- **Objective:** Answer field-test questions and fix two Zigbee behaviors: the FRC Calibrate button sticking enabled in Home Assistant, and ENS210 temperature/humidity attributes being delivered only once.
- **Files examined or changed:** `firmware/main/zigbee.c` (FRC trigger reset, ENS210 reporting configuration, ENS210 one-shot reports), `firmware/main/scd41.c` (read-only), `firmware/main/main.c` (read-only).
- **Work completed:** FRC is a one-shot SCD4x forced recalibration to 400 ppm triggered on the write edge; the firmware never cleared the attribute, so HA kept showing it on. The attribute is now reset to 0 right after `scd41_perform_frc(400)`. ENS210 attrs `0x0006`/`0x0007` had no reporting configuration and were not included in the periodic report set; they now have 60 s max/delta-based reporting (0.5 °C / 1 %RH) and are included in the sensor-task report cycle, matching the SCD4x temp/RH cadence. Confirmed the SCD40 readout default is `sensor_readout_period_ms = 1000` in `firmware/main/main.c`, with Zigbee attribute updates every 10 s.
- **Validation performed:** ESP-IDF v6.0.2 clean build (`fullclean`, `set-target esp32h2`, `build`) and `merge-bin`; complete log review; dependency lock verified (ZBOSS 1.6.4, Zigbee 1.6.7, IDF 6.0.2); published artifact size/hash checks.
- **Important discoveries:** Clean build has no project warnings; only the three known external Kconfig notes remain. App image is `0x7e870` bytes with 70% partition headroom; merged image `0x8e870` bytes. The 2026-08-05 documented sizes are superseded by the SCD4x firmware release. Index links to three deleted docs surfaced as a pre-existing inconsistency.
- **Decisions and rationale:** Reset the trigger attribute locally instead of changing HA button semantics; kept ENS210 deltas identical to the standard temp/RH clusters so both sensors present comparable cadence.
- **Artifact:** `output/firmware/AirCube_firmware_v2.0.1.bin`, 583792 bytes; SHA-256 `AA5BB800F674E97C470ED6AAB28E876A622282C04EFD606D9ECC6A14496E8675`; merged image is ready for flash offset `0x0`.
- **Remaining work:** Hardware flashing and HA/Zigbee runtime verification of the FRC reset and ENS210 reports were not performed.
- **Documentation updated:** `docs/build_and_test.md`, `docs/known_issues.md`, `docs/progress_tracker.md`.

### 2026-08-05 - OpenCode clean firmware publication

- **Objective:** Build and publish firmware according to the firmware-build-validation skill.
- **Files examined or changed:** Removed unreferenced `startup_animation()` from `firmware/main/main.c` and `ws2812_rmt_encode()` from `firmware/main/ws2812_control.c`; refined the skill's deprecation diagnostic pattern; updated build documentation and this tracker.
- **Work completed:** Ran an initial clean build, repaired both project warnings by removing dead code, repeated the full clean build, generated a fresh merged image, and published `output/firmware/AirCube_firmware_v2.0.1.bin`.
- **Validation performed:** ESP-IDF v6.0.2 `fullclean`, `set-target esp32h2`, `build`, and `merge-bin`; complete build/merge log review; dependency lock verification; destination existence/nonempty/size/SHA-256 checks.
- **Important discoveries:** Final build has no project compiler/linker warnings. ESP-IDF emits three external Kconfig notes whose invalid boolean defaults are treated as disabled, plus an expected Git version-probe fatal message because `.git` is absent; neither blocks the successful build. Pinned components remain ZBOSS 1.6.4 and Zigbee 1.6.7.
- **Decisions and rationale:** Removed dead static functions rather than suppressing warnings. Narrowed automated deprecation matching because an SDK directory named `deprecated` caused a false positive.
- **Artifact:** 945200 bytes; SHA-256 `1986FA18A6C2E4339B20B5A46F20180447B80A3B28670F0734BAF1D3ED64EE01`; merged image is ready for flash offset `0x0`.
- **Remaining work:** Hardware flashing and Base/Pro runtime smoke tests were not performed.
- **Documentation/skills updated:** `docs/build_and_test.md`, `docs/progress_tracker.md`, `firmware-build-validation/SKILL.md`, and its Windows reference.

### 2026-08-05 - OpenCode firmware build skill task

- **Objective:** Preserve a mandatory post-code-change firmware build, warning review/repair, merge, and artifact publication workflow.
- **Files changed:** Added `.agents/skills/firmware-build-validation/SKILL.md` and `references/windows-build.md`; updated the index, repository map, build documentation, progress tracker, and `.gitignore`.
- **Work completed:** Defined ESP-IDF v6.0.2/EIM commands, clean-build policy, complete-log inspection, warning/error repair loop, dependency checks, merged-image generation, hash verification, and stable `output/firmware/AirCube_firmware_v<version>.bin` publication.
- **Validation performed:** Skill frontmatter/name/path and first-100-line review; cross-link and output-path review.
- **Important discoveries:** A stable artifact must only be replaced after successful build, merge, warning review, and hash comparison to prevent stale failed-build output from appearing current.
- **Decisions and rationale:** The skill triggers after any product code change, while still requiring language/integration-specific checks because firmware compilation cannot validate all repository clients.
- **Remaining work:** Future code tasks must activate the skill; existing unused-function warnings remain actionable until repaired or explicitly justified.
- **Documentation/skills updated:** `docs/INDEX.md`, `docs/build_and_test.md`, `docs/repository_map.md`, `docs/progress_tracker.md`, the new skill, and its Windows command reference.

### 2026-08-05 - OpenCode firmware build task

- **Objective:** Install/activate ESP-IDF and build flashable AirCube firmware.
- **Files examined or generated:** `firmware/sdkconfig`, `dependencies.lock`, and ignored `firmware/build/` outputs; no product source was edited.
- **Work completed:** Installed official EIM CLI and ESP-IDF v5.3.4; diagnosed its corrupt RISC-V compiler extraction; built successfully using the existing complete ESP-IDF v6.0.2 installation; generated app, bootloader, partition, ELF/map, and merged images.
- **Validation performed:** `idf.py set-target esp32h2`, `idf.py build`, and `idf.py merge-bin`; dependency lock confirms IDF 6.0.2, ZBOSS 1.6.4, and Zigbee 1.6.7.
- **Important discoveries:** v5.3.4 EIM extraction lacks `cc1.exe`; v6.0.2 compiles successfully with two unused-function warnings. `AirCube.bin` is `0xd6c30` bytes with 48% app-partition space free; merged image is `0xe6c30` bytes.
- **Decisions and rationale:** Used v6.0.2 only after repairing v5.3.4 reproduced the external toolchain defect. Kept the repository's pinned Zigbee libraries.
- **Remaining work:** Flash and hardware-test; establish a supported ESP-IDF version policy; optionally remove or repair the broken v5.3.4 installation.
- **Documentation updated:** `docs/build_and_test.md`, `docs/progress_tracker.md`, and `docs/known_issues.md`.

### 2026-08-05 - OpenCode repository documentation task

- **Objective:** Inventory and preserve repository architecture, workflows, protocols, constraints, issues, and maintenance procedures.
- **Files examined:** All available root documentation; all firmware, scripts, ZHA, Z2M, SmartThings, build/configuration files; readable KiCad/BOM/placement/Gerber metadata; mechanical/archive/image inventory; `.gitignore`; `prompts/Start.txt`.
- **Files changed:** Added the documentation linked from `docs/INDEX.md` and two skills under `.agents/skills/`; corrected unsupported/inaccurate statements in existing `docs/BLE_GATT_PROTOCOL.md`. Product implementation files and root user guides were not changed.
- **Work completed:** Mapped every top-level directory; verified firmware paths/symbols and cross-component contracts; catalogued generated/opaque artifacts; documented build/test status and known risks.
- **Validation performed:** Filesystem inventory and static source comparison; path/link/frontmatter/first-100-lines review; JavaScript syntax checks; tool availability checks. Exact results are below.
- **Important discoveries:** Git metadata absent; firmware version 2.0.1; exclusive radio mode; absent tests/CI; absent Pro PCB source; integration and concurrency gaps summarized above.
- **Decisions and rationale:** Added focused documents rather than expanding the 431-line `CONTRIBUTING.md`; preserved user-facing docs to avoid losing useful setup material; marked static findings separately from runtime verification.
- **Remaining work:** Restore Git context, compile, perform hardware/platform tests, and address prioritized issues deliberately rather than as undocumented cleanup.
- **Documentation/skills updated:** `docs/INDEX.md`, `AI_agent_instructions.md`, `architecture.md`, `repository_map.md`, `build_and_test.md`, `protocols_and_integrations.md`, `hardware_and_tools.md`, `known_issues.md`, `progress_tracker.md`, and both project skills.

## Validation record

Commands run from the repository root unless noted:

| Command/check | Result |
|---|---|
| `git status --short --branch`, `git rev-parse --show-toplevel`, `git rev-parse HEAD`, `git remote -v`, `git ls-files` | Failed: no `.git` repository. Branch, commit, remotes, tracked inventory, dirty state, and diff remain unavailable. |
| `git status --short` and `git log --oneline -3` (2026-08-16) | Passed: Git metadata restored; HEAD `e9b6b38`, working tree modified only in `firmware/main/zigbee.c`. |
| EIM v6.0.2 clean build (2026-08-16) | Passed: `fullclean`, `set-target esp32h2`, `build`; no project warnings/errors; app `0x7ec40` bytes with 70% free; three external Kconfig notes unchanged. |
| EIM v6.0.2 `merge-bin` (2026-08-16) | Passed: `firmware/build/merged-binary.bin`, `0x8ec40` bytes, ready for flash offset `0x0`. |
| Flash to COM3 (2026-08-16) | Passed: all three images written and `Hash of data verified`; hard reset OK. |
| Serial smoke test on COM3 (2026-08-16) | Passed: ENS210 single-shot samples every 5 s with CRC-7 OK; SCD40 data-ready transitions `0x8000`→`0x8006`; CO2 514/519/505 ppm; `connected:true`. |
| Final publication (2026-08-16) | Passed: `output/firmware/AirCube_firmware_v2.0.1.bin`, 584768 bytes; source/destination SHA-256 both `BC9D5ACD5F8A033AAD0A7938F0B9569F0B4BD6E8CC9E1330B35344505CA4E3B1`. |
| Filesystem inventory plus static review of all available human-maintained content | Passed. Binary/generated/CAD artifacts were catalogued, not deeply inspected. |
| Documentation validation agent: all `docs/*.md` and both skills | Passed after corrections: all relative links/paths resolve; index covers all docs/skills/top-level directories; first-100-lines requirements pass; frontmatter is valid and names match directories. External URLs were not fetched. |
| Fetch/read current Agent Skills specification at `https://agentskills.io/specification` | Passed: both skills satisfy required `SKILL.md`, YAML `name`/`description`, naming, length, and directory-name rules. The initially attempted raw GitHub `specification.md` URL returned 404; the published specification URL succeeded. |
| `node --version` | Passed: `v24.16.0`. |
| `node --check "z2m/aircube.js"` and `node --check "z2m/aircube.mjs"` | Passed with no syntax errors/output. Runtime Z2M compatibility not tested. |
| `python --version` and in-memory Python syntax compilation | Failed: Python is not installed/available through the current shell. Python/ZHA syntax and desktop app were not tested. |
| `idf.py --version` (from `firmware/`) | Failed: `idf.py` is not recognized. Firmware was not configured, compiled, flashed, or hardware-tested. |
| `cmake --version` | Passed: CMake 3.28.1 is installed; this alone does not build ESP-IDF firmware. |
| `smartthings --version` (from driver directory) | Failed: SmartThings CLI is not recognized. Driver packaging/platform behavior was not tested. |
| EIM v5.3.4 `idf.py set-target esp32h2` / build | Failed during compiler sanity check: extracted GCC could not execute missing `cc1`; EIM repair reproduced the issue. |
| EIM v6.0.2 `idf.py set-target esp32h2` then `idf.py build` | Passed: ESP32-H2 build completed with pinned ZBOSS 1.6.4 and Zigbee 1.6.7; two unused-function warnings. |
| EIM v6.0.2 `idf.py merge-bin` | Passed: produced `firmware/build/merged-binary.bin`, `0xe6c30` bytes, for flash offset `0x0`. |
| Final clean EIM v6.0.2 build after warning repairs | Passed: no project compiler/linker warnings; app `0xd6c30` bytes with `0xc93d0` bytes (48%) free. Three reviewed ESP-IDF Kconfig notes remain external. |
| Final `idf.py merge-bin` and output publication | Passed after the final clean rebuild: `output/firmware/AirCube_firmware_v2.0.1.bin`, 945200 bytes; source/destination SHA-256 both `1986FA18A6C2E4339B20B5A46F20180447B80A3B28670F0734BAF1D3ED64EE01`. |

No AirCube hardware, BLE client, Zigbee coordinator, Home Assistant, Zigbee2MQTT, or SmartThings runtime was available. No progress archives exist; future archives belong under `docs/progress_archive/` and must be linked here and from the index.
