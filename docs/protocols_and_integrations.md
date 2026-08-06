# Protocols and Integrations

## Contract summary

| Boundary | Implementation | Key contract |
|---|---|---|
| USB | `firmware/main/serial_protocol.c` | USB-Serial-JTAG newline-delimited JSON plus `h` CSV shortcut; clients use 115200 conventionally |
| BLE | `firmware/main/ble_gatt.c` | AirCube service, live/history/brightness; inline BTHome v2 advertising |
| Zigbee | `firmware/main/zigbee.c` | endpoint 10, standard measurement/control and custom `0xFC01` |
| ZHA | `zha/aircube.py` | manufacturer/model quirk, common sensor entities and brightness |
| Z2M | `z2m/aircube.mjs`, `.js` | common sensor exposes and brightness |
| SmartThings | `smartthings/aircube-zigbee/` | common sensors; no brightness |

BLE and Zigbee are exclusive boot modes selected by `radio_mode.c`; USB remains available. Detailed BLE binary layouts are in [BLE GATT Protocol](BLE_GATT_PROTOCOL.md).

## USB serial

`serial_protocol_init()` installs the USB-Serial-JTAG driver and its buffers; it does not set a hardware UART baud. Repository clients conventionally open the exposed serial port at 115200. `sensor_task` writes one-line objects containing `model`, `ens210`, `ens16x`, `scd41`, `vcnl4040`, and millisecond uptime `timestamp`. The `ens210` object remains the compatibility container for selected T/RH even on Pro. `ens16x.aqi` is canonical VOC Level; `aqi_s` is deprecated zero; `aqi_uba` is the sensor's 1-5 rating.

Commands recognized by the source:

| Command | Key constraints |
|---|---|
| `get_config` | Returns configured brightness, readout period, auto-dim |
| `set_intensity` | Value 0.0-1.0; persisted brightness through shared setter |
| `set_readout_period` | 100-10000 ms; runtime-only |
| `set_auto_dim` | Pro policy/config; returns full config |
| `get_history_info` | Entries/capacity/slot/window metadata |
| `get_history` | `start`, `count`; practical page max 48 |
| `clear_history` | Erases history unless a stream is detected |
| `h` | Whole-history CSV shortcut |

This is not a general JSON parser: the command must begin compactly with `{"cmd":"..."}`; whitespace/reordered keys can fail. `scripts/aircube_app.py:SerialReaderThread.send_command` intentionally uses compact separators. Logs, live JSON, and command/history output share stdout, so clients must tolerate unrelated lines.

## History contract

`history_slot_t` is packed to 32 bytes: `sequence`, three signed temperature values, three signed humidity values, and three unsigned values each for VOC Level, CO2/eCO2, and TVOC. Temperature/humidity are x100. A five-minute RAM window is summarized to flash; 2016 logical slots represent seven days. Base `c_*`/historical `eco2_*` fields mean ENS16x eCO2; Pro means SCD41 true CO2.

There is no RTC timestamp. USB desktop history anchors newest at fetch time. BLE uses streaming frames and an `after_seq` cursor; see the BLE document. Existing old slots may contain legacy AQI-S in fields now named for VOC Level, an unavoidable persisted-data interpretation caveat.

## BLE and BTHome

`ble_gatt.c` builds the `A17C0DE0-...` service with Device Info (14 B), Live Data (20 B), History Request (4 B), History Data notifications, and one-byte brightness. It advertises connectably with BTHome service data and places name/service UUID in scan response. One unauthenticated central is allowed; preferred MTU is 256.

BTHome carries temperature, humidity, CO2 (true on Pro, eCO2 on Base), and TVOC. Source acknowledges that TVOC ppb is placed in standard object `0x13`, whose normal unit is micrograms/m3, so consumers can mislabel it. `ble_bthome.c` is an obsolete standalone reference absent from the build.

## Zigbee

Endpoint 10 identifies manufacturer `StuckAtPrototype`, model `AirCube`, and firmware build ID. Server clusters:

| Cluster | Data |
|---|---|
| `0x0000` Basic | identity/version |
| `0x0003` Identify | identify service |
| `0x000D` Analog Output | writable configured brightness float, 0-100 |
| `0x0402` Temperature | int16 x100 C |
| `0x0405` Humidity | uint16 x100% |
| `0x0400` Illuminance | Pro only |
| `0x040D` Carbon Dioxide | Pro true CO2, fraction-of-one float |
| `0xFC01` custom | uint16 eCO2 `0x0000`, eTVOC `0x0001`, VOC Level `0x0002` |

The sensor task explicitly reports about every 10 seconds to coordinator address `0x0000`, endpoint 1, in addition to configured reporting. Long press triggers steering; BLE mode sets an NVS request and reboots, while joined Zigbee mode factory-resets/restarts steering. Factory-new timeout/failure and network removal revert to BLE. Rejoin uses exponential backoff and a flap watchdog.

## Integration coverage

| Feature | ZHA modern | ZHA legacy | Z2M | SmartThings |
|---|---|---|---|---|
| Temperature/humidity | Yes | Yes | Yes | Yes |
| eCO2/eTVOC/VOC Level | Yes | No entities | Yes | Yes |
| Brightness | Yes | standard discovery expected | Yes | No |
| Pro true CO2/illuminance | No | No | No | No |

ZHA quirks-v2 creates common custom-cluster sensors and brightness. The exact legacy signature omits Pro-only clusters, so Pro matching is unverified. Z2M 2.x clamps brightness; 1.x does not. Z2M exposes eTVOC as `voc` and VOC Level as `aqi`. SmartThings maps eCO2 to the ordinary CO2 capability, which can imply a true measurement, and its source comment incorrectly describes VOC Level as a 1-5 index.

Any protocol change must update firmware, `scripts/aircube_app.py`, relevant legacy scripts, all three Zigbee integration families, detailed docs, and compatibility fixtures. Preserve compact serial command formatting unless the firmware parser is deliberately replaced.
