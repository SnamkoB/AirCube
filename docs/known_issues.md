# Known Issues and Unverified Areas

## Read first

These are static source findings as of the 2026-08-05 filesystem snapshot, not hardware reproduction results. Prioritize data corruption, deadlock/reset, protocol compatibility, and misleading measurements. Confirm against a Git-backed current checkout before fixing; do not bundle unrelated behavior changes into documentation work.

## High-impact verified source risks

| Area | Finding | Evidence/impact |
|---|---|---|
| Model detection | Pro if **either** optional sensor responds; Pro acquisition then relies on SCD41 | `device_model.c`, `main.c`; VCNL-only/failing SCD41 can yield zero T/RH/CO2 and bad compensation |
| History streaming | Stream ownership does not freeze appends/logical index movement; clear flag checks are not consistently mutex-protected | `history.c`; full ring can duplicate/skip or race clear |
| History durability | Slots have no CRC/commit marker beyond sequence not `0xFFFF` | interrupted flash write may be accepted after reboot |
| BLE history MTU | Code substitutes 59 for too-small negotiated MTU and may send 36 B despite 20 B default payload | `ble_gatt.c`; history may fail unless central negotiates larger MTU |
| Zigbee callback | Brightness write callback can call reporting and reacquire Zigbee lock | `zigbee.c`, `button.c`; possible two-second stall/reentrancy issue |
| Serial stream | Live JSON, logs, responses, and history share stdout across tasks | clients can receive interleaved/non-JSON lines |
| Serial parser | Prefix/substrings rather than JSON parser; negative history values cast to uint16; concatenated commands can merge | `serial_protocol.c`; malformed requests can behave unexpectedly |
| Watchdog | Readout allows 10 seconds while `sensor_task` registers with task WDT | configuration-dependent reset risk |
| Integration semantics | All hub integrations omit Pro true CO2/lux; SmartThings labels eCO2 as CO2 | `zha/`, `z2m/`, `smartthings/`; missing/misleading entities |

## Additional implementation risks

- `history_stream_release()` and some stream-active checks are outside the history mutex; the `h` CSV shortcut bypasses device-wide stream ownership.
- BLE live data is written field-by-field and read/notified from another context without a snapshot lock. A second START may queue behind an active history stream instead of returning busy.
- Zigbee explicit reports are hard-coded to coordinator endpoint 1 and mark custom-range cluster traffic non-manufacturer-specific; behavior across coordinators is unverified.
- Zigbee sensor values cast to `uint16_t` without general clamping. Basic model ID does not distinguish Base/Pro.
- ENS210 CRC is not checked. Reported Base temperature applies -2 C while raw uncorrected temperature feeds ENS16x compensation.
- ENS16x presence/read failures generally become zero; enum usage/status logging is inconsistent.
- SCD41 variant is logged but not rejected, despite comments implying confirmed SCD41.
- Auto-dim accepts problematic negative/non-finite serial thresholds before persistence conversion. Effective night policy turns configured 10/30/60 percent presets off and only dims the highest preset to `night_dim_pct`; verify intent.
- `app_main` logs automatic light sleep enabled while configuration sets it false.

## Desktop/integration risks

- Main app timestamp units are decided per sample using `timestamp > 1000`; a connection spanning that boundary can jump units. Reset/rollover is not handled.
- History `busy` responses abort rather than retry; pages trust response `count`; sequence gaps/wrap/concurrent mutation are not validated.
- UI can show connected before `serial.Serial` succeeds. CSV append does not validate existing header/schema.
- Legacy visualizer treats millisecond timestamps as seconds-like values; replay/visualizer disagree on timestamp priority. Legacy tools omit Pro fields.
- ZHA legacy signature omits Pro clusters and may not match Pro. Older zigpy API compatibility is unverified.
- Z2M 1.x does not clamp brightness writes; both variants rely on firmware reports for custom cluster and match model `AirCube` without manufacturer restriction.
- SmartThings source comments call custom VOC Level a 1-5 index; actual value is canonical 0-500. Refresh does not explicitly read custom attributes.
- Dependencies are lower-bound/unlocked; PyInstaller output is unsigned and not smoke-tested.

## Documentation/source inconsistencies

- `history.c` comments mention 14 days and some history comments mention 10-minute windows; constants implement seven days and five minutes.
- Existing auto-dim prose in `CONTRIBUTING.md` says defaults 5/15 lux; source defaults are 2/10 lux.
- `README.md` uses firmware 1.5.x "current" language while `firmware/version.txt` is 2.0.1.
- `CONTRIBUTING.md` project tree omits `z2m/aircube.mjs`; firmware README's issue link targets `CONTRIBUTING.md` rather than an issue tracker.
- `ble_bthome.c` and old comments describe an obsolete coexistence architecture; current build is exclusive-radio and BTHome is inline in `ble_gatt.c`.
- Current KiCad/BOM has no Pro SCD41/VCNL4040 hardware source despite Pro firmware/enclosure support.

## Unknown/unverified

- Branch, commit, remotes, tracked status, and original dirty state: no `.git` directory.
- Minimum supported ESP-IDF version remains unverified. ESP-IDF v6.0.2 builds successfully; the v5.3.4 attempt was blocked by a corrupt EIM compiler extraction rather than source compilation.
- Hardware behavior for Base/Pro, radio transitions, flash interruption, and partial sensor failure.
- Runtime compatibility across Home Assistant/zigpy, Z2M, SmartThings, mobile BLE clients, and coordinators.
- Contents/provenance of ZIP archives and geometry/image correctness of CAD/media files.

When resolving an item, add a regression test where feasible, update the owning technical document, and record validation plus decision rationale in the progress tracker.
