# Progress Tracker

## Current project status

- Available source snapshot implements firmware `2.0.1`, desktop utilities, PCB/mechanical assets, ZHA/Zigbee2MQTT converters, and a SmartThings Edge driver.
- Git metadata is absent. Branch, commit, remotes, tracked/untracked state, and pre-existing modifications are unknown.
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

- Missing `.git/` prevents repository-aware inventory and diff/status verification.
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
