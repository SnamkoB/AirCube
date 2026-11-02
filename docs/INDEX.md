# AirCube-TinieTronics Documentation Index

## Project summary

AirCube-TinieTronics is a filesystem snapshot of an AirCube fork: an ESP32-H2 air-quality monitor with ENS210 and ENS16x sensors, optional Pro SCD41/VCNL4040 sensors, WS2812 status LEDs, USB serial, mutually exclusive BLE or Zigbee operation, desktop Python tools, and Home Assistant/SmartThings integrations. The files in this workspace are authoritative; upstream AirCube material is context only.

**Comprehensive verification point:** commit **e9b6b38** ("New FW release with support for SCD4x, more data to HA and auto calibration"), re-verified via Git on 2026-08-16. Documentation origin was verified against the 2026-08-05 filesystem snapshot whose firmware version is `2.0.1` (`firmware/version.txt`). Some links below target documents deleted in `e9b6b38`; see [known issues](known_issues.md).

## Start here for AI agents

1. Read [AI agent instructions](AI_agent_instructions.md), then [progress tracker](progress_tracker.md).
   Also follow [repository agent guidance](../AGENTS.md) and the [development workflow](development_workflow.md).
2. Read [architecture](architecture.md), then choose a focused document from the table below.
3. Identify every relevant project skill. Read each selected `SKILL.md` completely before following it, including referenced resources.
4. Confirm Git state and re-check source paths before changing behavior; Git metadata was restored on 2026-08-16.
5. Update durable documentation and the progress tracker when understanding or implementation changes.

## Documentation

| Document | Purpose |
|---|---|
| [AI agent instructions](AI_agent_instructions.md) | Mandatory startup, source-of-truth, validation, and preservation rules |
| [Progress tracker](progress_tracker.md) | Current status, blockers, discoveries, and recent work |
| [Architecture](architecture.md) | System components, startup, tasks, data flow, variants, and boundaries |
| [Repository map](repository_map.md) | Directory/file ownership, generated artifacts, and navigation |
| [Build and test](build_and_test.md) | Dependencies, build/run/flash commands, validation status, and gaps |
| [Development workflow](development_workflow.md) | Version numbers, feature validation, hardware flashing, documentation, and GitHub delivery |
| [Protocols and integrations](protocols_and_integrations.md) | USB JSON, BLE, Zigbee, ZHA, Z2M, and SmartThings contracts |
| [Hardware and tools](hardware_and_tools.md) | PCB/mechanical assets, desktop utilities, and Base/Pro evidence |
| [Known issues](known_issues.md) | Verified defects, risks, stale comments/docs, and unresolved questions |
| [BLE GATT Protocol](BLE_GATT_PROTOCOL.md) | Detailed BLE v1 binary service and history framing |

Existing user/developer guides remain authoritative for their intended workflows, subject to current-source verification: [product README](../README.md), [contributing/build guide](../CONTRIBUTING.md), [firmware update](../FIRMWARE_UPDATE.md), [assembly](../ASSEMBLY.md), [Home Assistant](../HOME_ASSISTANT.md), [SmartThings setup](../SMARTTHINGS.md), [SmartThings directory README](../smartthings/README.md), and [firmware README](../firmware/README.md).

## Topic lookup

| Question/topic | Read | Primary source |
|---|---|---|
| Firmware entry point and runtime | [Architecture](architecture.md) | `firmware/main/main.c` (`app_main`, `sensor_task`, `command_task`) |
| Sensor drivers and model detection | [Architecture](architecture.md) | `ens210.c`, `ens16x_driver.c`, `scd41.c`, `vcnl4040.c`, `device_model.c` |
| LED, button, auto-dim | [Architecture](architecture.md) | `led.c`, `led_color_lib.c`, `ws2812_control.c`, `button.c`, `auto_dim.c` |
| History/storage | [Protocols and integrations](protocols_and_integrations.md) | `history.c`, `history.h`, `partitions_zigbee.csv` |
| USB serial commands/schema | [Protocols and integrations](protocols_and_integrations.md) | `serial_protocol.c`, `serial_protocol.h` |
| BLE/BTHome/radio selection | [BLE GATT Protocol](BLE_GATT_PROTOCOL.md), [Protocols](protocols_and_integrations.md) | `ble_gatt.c`, `radio_mode.c`; `ble_bthome.c` is not built |
| Zigbee endpoint/clusters/pairing | [Protocols and integrations](protocols_and_integrations.md) | `zigbee.c`, `radio_mode.c` |
| Home Assistant and SmartThings | [Protocols and integrations](protocols_and_integrations.md) | `zha/`, `z2m/`, `smartthings/` |
| Build, flash, dependencies, tests | [Build and test](build_and_test.md) | `firmware/CMakeLists.txt`, `idf_component.yml`, `sdkconfig.defaults`, `scripts/requirements.txt` |
| Desktop tools | [Hardware and tools](hardware_and_tools.md) | `scripts/` |
| PCB and manufacturing files | [Hardware and tools](hardware_and_tools.md) | `kicad/` |
| Mechanical/assembly files | [Hardware and tools](hardware_and_tools.md) | `mechanical/`, `ASSEMBLY.md` |
| Current work, blockers, discoveries | [Progress tracker](progress_tracker.md) | `docs/progress_tracker.md` |
| Risks and discrepancies | [Known issues](known_issues.md) | Referenced implementation paths |
| Original documentation task | [Repository map](repository_map.md) | `prompts/Start.txt` |

## Project skills

Project-local skills use the standard `.agents/skills/` convention; no prior skill convention existed in this snapshot.

| Skill | Activate when |
|---|---|
| [documentation-maintenance](../.agents/skills/documentation-maintenance/SKILL.md) | Repository knowledge, behavior, commands, architecture, or docs change |
| [firmware-build-validation](../.agents/skills/firmware-build-validation/SKILL.md) | After any product code change, or when building, validating, or packaging ESP32-H2 firmware |
| [skill-maintenance](../.agents/skills/skill-maintenance/SKILL.md) | Creating, changing, consolidating, or validating project skills |

## Coverage and archives

Every available top-level directory is represented in [repository map](repository_map.md): `.agents`, `.vscode`, `docs`, `firmware`, `kicad`, `mechanical`, `prompts`, `scripts`, `smartthings`, `z2m`, and `zha`; generated `output/` is also mapped although it may be absent before a clean publication. There is currently no `docs/progress_archive/`; create it only when old tracker entries need archiving, then link every archive here and from the tracker.

Update this index whenever documentation or skills are added, removed, moved, renamed, split, or archived.
