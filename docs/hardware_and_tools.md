# Hardware and Desktop Tools

## Hardware summary

The readable KiCad source describes a 45.1 x 45.1 mm, two-layer ESP32-H2 board with ENS161, ENS210, three RGB LEDs, USB-C, button circuitry, 3.3 V/1.8 V regulation, and protection. Firmware and enclosures define Base and Pro products, but the current KiCad schematic/BOM contains no SCD41 or VCNL4040. Pro wiring/source is therefore unknown in this snapshot.

| Area | Authoritative paths | Verification level |
|---|---|---|
| PCB design | `kicad/AirCube.kicad_pro`, `.kicad_sch`, `.kicad_pcb` | Readable source statically inspected |
| BOM/placement | `kicad/AirCube v1.0 BOM.csv`, top/bottom position CSVs | Parsed as text |
| Manufacturing | `kicad/gerbers/`, drills, job file, release ZIPs | Catalogued; job metadata read; archives not opened |
| Board/mechanical exchange | `kicad/AirCube.step`, `mechanical/**/*.{step,stl,3mf}` | Catalogued, geometry not deeply inspected |
| Assembly media | `mechanical/guide_images/*.jpg` | References checked; image content not deeply inspected |

Key board evidence: ESP32-H2-MINI-1 (`U7`), ENS161 (`U2`), ENS210 (`U10`), LEDs 1-3, USB-C `P1`, switches `S2/S3`, AP2205 3.3 V and AP2120 1.8 V regulators. All 43 fitted placements are on top; bottom placement output contains only its header. Named nets include power, I2C, USB data, CP_EN, LED chain, USB CC/VCONN/shield, and selected MCU GPIOs.

The BOM's AP2120 row has an extra `EOL` field beyond the CSV header. Treat manufacturing outputs as versioned exports, not automatically regenerated truth. KiCad ZIP backups, release archives, footprint cache, `.kicad_prl`, and `desktop.ini` are generated/local artifacts.

## Mechanical variants

Base: bottom/top STEP and air-wall 3MF under `mechanical/base/`. Pro: bottom STEP plus top, air wall, light wall, and button-cap STL under `mechanical/pro/`. `ASSEMBLY.md` references all 15 guide images (Base A-G, Pro A-H) and requires three M2 x 5 mm self-tapping screws.

No native parametric enclosure authoring files were found, so editability/provenance of exchange/mesh assets is unverified.

## Desktop tools

### Main application

`scripts/aircube_app.py` (`AirCubeApp`, `SerialReaderThread`) is a PyQt6/Matplotlib/PySerial application. It selects a serial port, parses newline text for live Base/Pro JSON, plots values, optionally logs a 14-column CSV, pages on-device history in groups of 48, and exports inferred-time history CSV. Pro UI fields are true CO2 and lux. Firmware brightness/config controls are not surfaced.

The app expects `model`, `ens210`, `ens16x`, and `timestamp`; Pro objects are optional. It treats `ens16x.aqi` as VOC Level. History `c_*` changes semantic meaning by model. Existing CSV files are appended without schema checks.

### Legacy utilities

- `aircube_logger.py`: hard-coded `COM33` serial logger using the original nine columns.
- `aircube_data_visualizer.py`: reloads `sensor_log.csv` each second and plots Base-era fields.
- `aircube_replay_script.py`: sorts and replays legacy CSV at 5x speed.
- `sensor_log.csv`: recorded 542-row nine-column sample; not a complete protocol fixture.

These utilities omit Pro fields and have inconsistent timestamp heuristics. Prefer the main app for current functionality; retain legacy scripts until their users/compatibility requirements are known.

## Installation and packaging

`scripts/install.py` creates/reuses `.venv`, upgrades packaging tools, installs unpinned requirements, and import-checks serial/Matplotlib/PyQt6. `install.bat` and `install.sh` are wrappers. `--with-build` adds PyInstaller.

`build_exe.py` creates a one-file/windowed `AirCube` executable and auto-installs PyInstaller if absent. It has no icon in the current tree, signing, release metadata, checksum, or launch smoke test. `aircube.spec` mirrors generated options but is not invoked by the helper, so it is not clearly authoritative.

See [build and test](build_and_test.md) for commands and [known issues](known_issues.md) before changing timestamp, CSV, connection, or history behavior.
