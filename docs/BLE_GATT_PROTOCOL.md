# AirCube BLE GATT Protocol (v1)

Protocol between an AirCube in **BLE mode** (its default radio mode when not
joined to a Zigbee network) and client apps. The firmware implements this in
`firmware/main/ble_gatt.c`; no BLE client implementation is present here.

All multi-byte fields are **little-endian** (native ESP32 byte order).

## Advertising

| Packet | Contents |
|---|---|
| Advertisement (connectable) | Flags (0x06) + BTHome v2 service data (UUID 0xFCD2), generated inline from the same payload structure as the reference `ble_bthome.c` implementation |
| Scan response | Complete local name `AirCube` + 128-bit AirCube service UUID |

`ble_bthome.c` exists in the source tree as a standalone reference but is **not** part of the
current build (it's not listed in `firmware/main/CMakeLists.txt`). The live BTHome v2 payload is
generated inline by `ble_gatt.c`'s `do_advertise()`. This advertisement is only broadcast while
the device is in BLE mode — it stops once the device joins a Zigbee network.

The separate iOS client is expected to discover the service UUID from the scan
response, but that client and runtime behavior were not verified in this repository.

## Service and characteristics

Base UUID suffix: `-1D0F-4E7C-8E4B-2A3D5F6B7C80`

| UUID prefix | Name | Properties | Size |
|---|---|---|---|
| `A17C0DE0` | AirCube Service | — | — |
| `A17C0DE1` | Device Info | Read | 14 B |
| `A17C0DE2` | Live Data | Read, Notify | 20 B |
| `A17C0DE3` | History Request | Write | 4 B |
| `A17C0DE4` | History Data | Notify | Intended to be <= MTU-3; see MTU caveat below |
| `A17C0DE5` | Brightness | Read, Write, Notify | 1 B |

Firmware requests ATT MTU 256. Actual negotiated MTU is client-dependent and unverified here.

## Device Info (`A17C0DE1`, read)

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | protocol_version (= 1) |
| 1 | u8 | model: 0 = Base, 1 = Pro |
| 2 | u8 | fw_major |
| 3 | u8 | fw_minor |
| 4 | u8 | fw_patch |
| 5 | u8 | reserved (0) |
| 6 | u16 | history_capacity (2016) |
| 8 | u16 | history_entry_count (current valid entries) |
| 10 | u16 | history_window_s (300) |
| 12 | u16 | newest_seq (sequence of newest slot; 0xFFFF if history empty) |

## Live Data (`A17C0DE2`, read + notify)

Notified once per configured sensor readout (default ~1 s; configurable 100-10000 ms). 20 bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | i16 | temperature, °C × 100 |
| 2 | u16 | humidity, % × 100 |
| 4 | u16 | VOC Level (0–500) |
| 6 | u16 | eCO2, ppm (ENS16X estimate) |
| 8 | u16 | eTVOC, ppb |
| 10 | u16 | CO2, ppm (SCD41 NDIR; 0 on Base) |
| 12 | u16 | ambient light, lux × 10 (VCNL4040; 0 on Base) |
| 14 | u8 | AQI-UBA (1–5) |
| 15 | u8 | flags: bit0 = Pro model |
| 16 | u32 | uptime, ms |

## Brightness (`A17C0DE5`, read + write + notify)

Single byte: LED brightness percent, 0–100 (writes above 100 are clamped).
Writes apply immediately and persist to NVS across reboots. In BLE mode, the
device notifies the confirmed value after BLE writes and local button changes
so subscribed clients stay in sync. Zigbee writes occur only in the mutually
exclusive Zigbee mode and therefore cannot notify a connected BLE client.

The hardware button cycles 0 → 10 → 30 → 60 → 100 → 0, snapping to the next
level above the current (possibly BLE-written) percent.

## History sync (streaming, not paged)

1. Client subscribes to **History Data** notifications.
2. Client writes a request to **History Request**:

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | opcode: 0x01 = start stream, 0x02 = abort |
| 1 | u8 | reserved (0) |
| 2 | u16 | after_seq: stream only slots with sequence > this. 0xFFFF = full history |

3. Firmware streams **History Data** notifications back-to-back until caught
   up, then sends a done frame. No per-page round trips.

### History Data frames

Every frame starts with a 4-byte header:

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | frame_type: 0x01 data, 0x02 done, 0x03 error |
| 1 | u8 | slot_count (data frames) / 0 |
| 2 | u16 | data: logical index of first slot in frame · done: total slots sent · error: reason code |

**Data frame** (0x01): header + `slot_count` × 32-byte slots. The intended
calculation chooses the largest n with `4 + 32n <= MTU-3` (5 slots at MTU
185). Current firmware substitutes 59 when the negotiated MTU is smaller,
which can produce a 36-byte notification that exceeds the default 20-byte ATT
payload. Clients should negotiate a larger MTU until this implementation bug
is fixed; default-MTU behavior is unverified.

**Done frame** (0x02): stream complete. The client anchors wall-clock time
now: newest slot = time of done frame; each older slot is `history_window_s`
earlier. Slots have no absolute timestamps (the device has no RTC).

**Error frame** (0x03) reason codes: 1 = busy (another history stream is
active, e.g. over USB serial — retry after a short delay), 2 = bad request.

### Slot binary layout (32 bytes, packed)

Matches `history_slot_t` in `firmware/main/history.h`:

| Offset | Type | Field |
|---|---|---|
| 0 | u16 | sequence (0xFFFF = empty/invalid slot, skip it) |
| 2 | i16 ×3 | temperature avg/min/max, °C × 100 |
| 8 | i16 ×3 | humidity avg/min/max, % × 100 |
| 14 | u16 ×3 | VOC Level avg/min/max |
| 20 | u16 ×3 | CO2 avg/min/max, ppm (true CO2 on Pro, eCO2 on Base) |
| 26 | u16 ×3 | eTVOC avg/min/max, ppb |

### Client sync algorithm

```
info = read Device Info
if info.newest_seq == 0xFFFF: nothing to sync
last = highest sequence cached for this device (or none)
write History Request {0x01, after_seq: last or 0xFFFF}
collect data frames until done frame
store slots keyed by (device_id, sequence)   # idempotent
anchor timestamps: newest slot = now, each older slot -5 min
```

Sequence numbers are monotonic u16 and wrap at 0xFFFE. Firmware derives the
start index from the modular distance between `after_seq` and the newest
sequence; it does not search for an exact matching slot. If the distance is
outside the current entry count it sends full history. Clients should still
deduplicate by sequence and detect gaps because an absent but numerically
plausible cursor can be accepted.

## Concurrency rules

- BLE and paged USB JSON history share one stream lock; the loser gets
  error/busy and retries. The single-character serial `h` CSV dump bypasses
  this lock in the current implementation and can overlap another stream.
- Only one BLE central may connect (NimBLE max connections = 1).
- `clear_history` is serial-only in v1 and is refused while a stream runs.

## Sizing

Full sync payload: 2016 slots × 32 B = 63 KiB, plus frame headers.
Incremental daily payload: 288 slots × 32 B = 9 KiB, plus frame headers.
Transfer time depends on negotiated MTU, connection parameters, and client behavior; it was not measured here.
