# Repository Map

## Fast navigation

This map covers every top-level directory in the available 2026-08-05 filesystem snapshot. Because `.git` is absent, "tracked" status cannot be verified. Human-maintained source was read; opaque/generated artifacts were catalogued without claiming unsupported deep inspection.

| Path | Ownership and key contents | Artifact status |
|---|---|---|
| `.agents/` | Project-local documentation, skill maintenance, and mandatory firmware build-validation procedures | Human-maintained agent skills added 2026-08-05 |
| `.vscode/` | Workspace ESP-IDF selection (`settings.json`, currently v6.0.2) | Local/editor configuration |
| `docs/` | AI navigation, architecture, protocols, build, hardware/tools, issues; existing BLE protocol | Human-maintained docs |
| `firmware/` | ESP-IDF ESP32-H2 project; application in `main/` | Source/config; local ignored `build/` outputs currently exist |
| `kicad/` | KiCad project, BOM/placement, Gerbers, drills, STEP, archives/backups | Editable source plus generated/manufacturing/binary artifacts |
| `mechanical/` | Base/Pro enclosure STEP/STL/3MF and assembly JPEGs | CAD/mesh/image artifacts; native authoring source unknown |
| `output/` | Generated build logs and validated merged firmware under `output/firmware/` | Created by the build skill after a clean result; ignored by Git and may be absent beforehand |
| `prompts/` | Original documentation task (`Start.txt`) | Process documentation |
| `scripts/` | Desktop GUI, logger, visualizer, replay, installers, PyInstaller, sample CSV | Python source/config/sample data |
| `smartthings/` | Edge driver package, profile, fingerprint, Lua, channel config | Integration source/config |
| `z2m/` | Zigbee2MQTT 1.x CommonJS and 2.x ESM converters | Integration source |
| `zha/` | Home Assistant ZHA quirk with v2 and legacy fallback | Integration source |

Root guides: `README.md`, `CONTRIBUTING.md`, `ASSEMBLY.md`, `FIRMWARE_UPDATE.md`, `HOME_ASSISTANT.md`, `SMARTTHINGS.md`; license is Apache 2.0 in `LICENSE`. `.gitignore` documents local/generated exclusions. `eim_config.toml` is local EIM configuration for the broken v5.3.4 installation and contains machine-specific absolute paths; it is not the verified v6.0.2 build definition.

## Firmware map

Build authority is `firmware/main/CMakeLists.txt`, not directory presence.

| Files | Purpose |
|---|---|
| `main.c` | `app_main`, sensor and serial command tasks, VOC mapping, LED display loop |
| `device_model.*` | Runtime Base/Pro state (`aircube_model_detect`) |
| `i2c_driver.*` | Shared I2C master bus/device handles |
| `ens210.*`, `ens16x_driver.*` | Base T/RH and common gas sensor drivers |
| `scd41.*`, `vcnl4040.*` | Optional Pro CO2/T/RH and ambient light drivers |
| `led.*`, `led_color_lib.*`, `ws2812_control.*` | Thread-safe LED state, hue math, RMT output |
| `button.*`, `auto_dim.*` | Brightness/pairing input and Pro lux policy |
| `history.*` | Flash-backed five-minute summary ring |
| `serial_protocol.*` | USB-Serial-JTAG JSON/CSV commands and output |
| `radio_mode.*` | NVS mode/join/pair flags and reboot transitions |
| `ble_gatt.*` | Built GATT server, BTHome advertisement, BLE history |
| `zigbee.*` | Endpoint 10, clusters, reports, pairing/rejoin |
| `ble_bthome.*` | Standalone reference; **not built** |
| `environmental.*` | Placeholder; **not built** |

Project configuration: top-level `CMakeLists.txt`, `sdkconfig.defaults`, empty `sdkconfig.ci`, `partitions_zigbee.csv`, `version.txt` (`2.0.1`), component `idf_component.yml`, and `main/Kconfig.projbuild`.

## Desktop and integration map

- `scripts/aircube_app.py`: PyQt6 live serial GUI and paged on-device history export.
- `scripts/aircube_logger.py`: legacy fixed-port nine-column CSV logger.
- `scripts/aircube_data_visualizer.py`: legacy live plot of `sensor_log.csv`.
- `scripts/aircube_replay_script.py`: legacy timed replay.
- `scripts/install.py`, `.bat`, `.sh`: environment/install flow.
- `scripts/build_exe.py`, `aircube.spec`: PyInstaller path; spec appears generated and is not used by `build_exe.py`.
- `scripts/sensor_log.csv`: 542-row recorded legacy-schema sample, not a comprehensive protocol fixture.
- `zha/aircube.py`: modern quirks-v2 entities plus limited classic fallback.
- `z2m/aircube.mjs`: Z2M 2.x ESM converter; `aircube.js`: legacy CommonJS converter.
- `smartthings/aircube-zigbee/`: Edge package (`config.yml`, `fingerprints.yml`, profile, `src/init.lua`).

## Hardware/manufacturing map

Editable KiCad source is `AirCube.kicad_pro`, `.kicad_sch`, and `.kicad_pcb`. `AirCube.kicad_prl` is user/UI state. BOM and placement CSVs, `gerbers/`, drills, Gerber job, and `AirCube.step` are generated outputs. `AirCube_v1.0.zip`, `AirCube_v1.1.zip`, and `AirCube-backups/*.zip` are opaque archives. `fp-info-cache` and `desktop.ini` are generated/local metadata.

`mechanical/base/` contains bottom/top STEP and air-wall 3MF. `mechanical/pro/` contains bottom STEP plus top, air wall, light wall, and button STL. `mechanical/guide_images/` contains all Base A-G and Pro A-H JPEGs referenced by `ASSEMBLY.md`.

## Absent infrastructure

No `AGENTS.md`, `CLAUDE.md`, `.claude/`, `.github/`, CI workflow, test directory/config, Dockerfile, automated release script, or release firmware binary was found in the available snapshot. The build generated ignored `firmware/dependencies.lock` and local binaries under `firmware/build/`; `.agents/` contains the project skills linked by the index.

## Ignore-rule notes

`.gitignore` excludes ESP-IDF build/state, Python environments/cache, PyInstaller output, and most CSVs with exceptions for partition/KiCad/sample CSVs. Stale rules target nonexistent `hardware/racer` paths and do not cover current KiCad backups/cache; without Git metadata, historical tracking cannot be determined.
