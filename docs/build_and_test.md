# Build, Run, and Validation

## Status at last review

Static inspection was completed on 2026-08-05. The firmware was subsequently built successfully for ESP32-H2 with ESP-IDF v6.0.2, `esp-zboss-lib` 1.6.4, and `esp-zigbee-lib` 1.6.7. `idf.py merge-bin` also succeeded. Git metadata is available again (last verified commit `e9b6b38`); the repository still has no automated tests or CI and no hardware was flashed or tested.

## Firmware

| Requirement | Repository evidence |
|---|---|
| Target | ESP32-H2, 2 MB flash (`firmware/sdkconfig.defaults`) |
| Framework | ESP-IDF `>=5.0.0` (`firmware/main/idf_component.yml`); docs recommend 5.3+ |
| Zigbee deps | `esp-zboss-lib ==1.6.4`, `esp-zigbee-lib ==1.6.7` |
| Version | `2.0.1` (`firmware/version.txt`) |
| Partitioning | custom `firmware/partitions_zigbee.csv`; factory image only, no OTA slots |

Verified build output from ESP-IDF v6.0.2:

- `firmware/build/AirCube.bin`: `0x7ec40` bytes, flashed at `0x10000`; app partition had 70% free (2026-08-16 rebuild after the SCD4x firmware release).
- `firmware/build/bootloader/bootloader.bin`: `0x5570` bytes, flashed at `0x0`; 33% free.
- `firmware/build/partition_table/partition-table.bin`: flashed at `0x8000`.
- `firmware/build/merged-binary.bin`: `0x8ec40` bytes, complete raw image for offset `0x0`.
- Current clean-build result: no project compiler or linker warnings. The unused `startup_animation()` and `ws2812_rmt_encode()` remnants were removed before the 2026-08-05 publication.
- ESP-IDF v6.0.2 emits three SDK-owned Kconfig notes for invalid `default 0` boolean syntax (`BT_NIMBLE_MESH_PROVISIONER`, `FATFS_PRINT_LLI`, `FATFS_PRINT_FLOAT`); the parser explicitly treats them as disabled. They are external SDK diagnostics, not project warnings.
- ESP-IDF's source-version probe prints `fatal: not a git repository` because this filesystem snapshot has no `.git`; CMake still resolves application version `2.0.1` from `firmware/version.txt` and completes successfully.
- Published image (2026-08-17 recovery build): `output/firmware/AirCube_firmware_v2.0.1.bin`, 584768 bytes, SHA-256 `CFE141AD02F592414B4EC29A42274374B517D341BE747C14EB2BBAA8555EE103`. Firmware source exactly matches commit `533dcb6`; only `zha/aircube.py` differs, using integer `divisor=100` for ENS210 display scaling. The 2026-08-16 image had the same size but an earlier compile timestamp/hash (`BC9D5ACD5F8A033AAD0A7938F0B9569F0B4BD6E8CC9E1330B35344505CA4E3B1`).

The first v5.3.4 attempt failed before compiling project code because EIM extracted the Windows RISC-V GCC toolchain without `cc1.exe`. EIM repair reproduced that toolchain problem. This is an installation issue, not evidence that the source is incompatible with v5.3.4.

After product code changes, activate and follow the complete [firmware-build-validation skill](../.agents/skills/firmware-build-validation/SKILL.md). Final merged images are published only after warning/error review to `output/firmware/AirCube_firmware_v<version>.bin`; build and merge logs belong beside the artifact.

From `firmware/` in an exported ESP-IDF shell:

```bash
idf.py set-target esp32h2
idf.py build
idf.py -p PORT flash monitor
```

Useful diagnostics: `idf.py menuconfig`, `idf.py fullclean`, and `idf.py -p PORT monitor`. Do not run `fullclean` in a workspace with valuable local build output unless intended. `AirCube Configuration -> Zigbee TX power` controls `CONFIG_AIRCUBE_ZB_TX_POWER_DBM` (-24 to 20 dBm, default 20).

Before accepting product code changes:

1. Run relevant checks for every changed language/integration, then follow the firmware build skill even when firmware itself was not edited.
2. Confirm managed dependencies resolve to the intended pinned Zigbee versions; source comments warn that `1.6.8` caused ESP32-H2 PHY startup hangs.
3. Build from a clean configuration and inspect the complete output for warnings/errors, partition use, and binary size. Repair actionable findings and rebuild.
4. Generate and hash-check the merged binary, then copy it to the stable `output/firmware/` location.
5. Test Base and Pro startup, USB output/commands, LED/button, history persistence, and both radio boot paths on hardware.
6. For Zigbee changes, test new join, rejoin, removal/fallback, explicit reports, writes, and the affected hub integrations.
7. For BLE changes, test advertising, a default-MTU connection, negotiated larger MTU, live notifications, brightness, full/incremental history, busy/abort, and disconnect.

## Desktop Python tools

Requirements in `scripts/requirements.txt` are unpinned lower bounds: `pyserial>=3.5`, `matplotlib>=3.3`, `PyQt6>=6.4`. Python `install.py` requires 3.8+, but there is no lockfile or tested matrix.

From `scripts/`:

```bash
python install.py
.venv\Scripts\python.exe aircube_app.py
```

On Linux/macOS, use `.venv/bin/python aircube_app.py`. `install.bat` and `install.sh` wrap the installer. Options include `--with-build`, `--no-venv`, and `--venv-dir DIR`.

Standalone executable path:

```bash
python install.py --with-build
.venv\Scripts\python.exe build_exe.py
```

`build_exe.py` invokes PyInstaller on `aircube_app.py` and expects `scripts/dist/AirCube.exe` on Windows. It does not consume checked-in `aircube.spec`, sign output, or smoke-test the executable.

## Integrations

- ZHA has no local package/test harness. At minimum, syntax-compile `zha/aircube.py`, then load it in supported Home Assistant/zigpy versions and pair both descriptors.
- Z2M converters require matching platform generations (`aircube.mjs` for 2.x, `aircube.js` for older CommonJS environments). Validate load plus representative reports/writes in the actual Zigbee2MQTT version.
- SmartThings packaging uses `smartthings edge:drivers:package .` from `smartthings/aircube-zigbee/`; this calls external services and may require interactive authentication. Do not run it as a local syntax check or expose credentials.

## Current test gaps

No unit, integration, host firmware, Python, converter, quirk, Lua, hardware-in-loop, link-check, lint, or CI tests are present. `firmware/sdkconfig.ci` is empty. High-value first tests are:

- Pure VOC Level band interpolation and CO2 severity mapping.
- Serial command parser boundaries and compact producer fixtures.
- History ring wrap, reboot scan, clear/stream/append concurrency, sequence wrap, and interrupted writes.
- Desktop live parser, timestamp reset/unit boundary, history pagination/busy retry, and CSV schema handling.
- ZHA Base/Pro fingerprints, Z2M report/write fixtures, SmartThings custom attribute semantics.

## Validation record conventions

Record exact command, working directory, result, tool/version, and any omitted checks in `docs/progress_tracker.md`. Syntax success does not prove hardware or platform compatibility. A command copied from an existing guide is "documented," not "verified," until run in the current checkout.
