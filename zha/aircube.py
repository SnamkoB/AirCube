"""Bart&Matthijs AirCube air quality monitor quirk for ZHA.

Sensors:
  Temperature  - SCD4x (with software trim applied)
  Humidity     - SCD4x
  CO2          - SCD4x (true NDIR)
  Brightness   - Analog Output (writable 0-100)

Calibration controls (custom cluster 0xFC01):
  FRC Calibrate  - write 1 to trigger forced recalibration to 400 ppm
  Temp Trim      - SCD4x software temperature offset in millidegree C (±5.000 °C)
  Pressure       - ambient pressure in hPa (700-1100)

ENS210 comparison readings are logged to serial only.
"""

from zigpy.quirks import CustomCluster
from zigpy.zcl.foundation import ZCLAttributeDef
import zigpy.types as t


class AirQualityCluster(CustomCluster):
    """AirCube custom cluster (0xFC01) — calibration attributes."""

    cluster_id = 0xFC01
    name = "AirCube Calibration"
    ep_attribute = "aircube_calibration"

    class AttributeDefs(CustomCluster.AttributeDefs):
        # ENS16X attributes (not exposed — sensor disabled)
        eco2 = ZCLAttributeDef(
            id=0x0000, type=t.uint16_t, is_manufacturer_specific=False
        )
        etvoc = ZCLAttributeDef(
            id=0x0001, type=t.uint16_t, is_manufacturer_specific=False
        )
        aqi = ZCLAttributeDef(
            id=0x0002, type=t.uint16_t, is_manufacturer_specific=False
        )
        # Calibration attributes
        frc_trigger = ZCLAttributeDef(
            id=0x0003, type=t.uint8_t, is_manufacturer_specific=False
        )
        temp_trim = ZCLAttributeDef(
            id=0x0004, type=t.int16s, is_manufacturer_specific=False
        )
        pressure = ZCLAttributeDef(
            id=0x0005, type=t.uint16_t, is_manufacturer_specific=False
        )
        ens210_temp = ZCLAttributeDef(
            id=0x0006, type=t.int16s, is_manufacturer_specific=False
        )
        ens210_humidity = ZCLAttributeDef(
            id=0x0007, type=t.uint16_t, is_manufacturer_specific=False
        )


ANALOG_OUTPUT_CLUSTER_ID = 0x000D

try:
    from zigpy.quirks.v2 import QuirkBuilder
    from zigpy.quirks.v2.homeassistant import EntityType

    try:
        from zigpy.quirks.v2.homeassistant.sensor import (
            SensorDeviceClass,
            SensorStateClass,
        )
    except ImportError:
        from homeassistant.components.sensor import (
            SensorDeviceClass,
            SensorStateClass,
        )

    _HAS_QUIRKS_V2 = True
except ImportError:
    _HAS_QUIRKS_V2 = False

if _HAS_QUIRKS_V2:
    (
        QuirkBuilder("Bart&Matthijs", "AirCube")
        .replaces(AirQualityCluster, endpoint_id=10)
        .number(
            "present_value",
            ANALOG_OUTPUT_CLUSTER_ID,
            endpoint_id=10,
            min_value=0,
            max_value=100,
            step=1,
            mode="slider",
            entity_type=EntityType.STANDARD,
            translation_key="brightness",
            fallback_name="Brightness",
        )
        .switch(
            AirQualityCluster.AttributeDefs.frc_trigger.name,
            AirQualityCluster.cluster_id,
            endpoint_id=10,
            entity_type=EntityType.STANDARD,
            translation_key="frc_trigger",
            fallback_name="FRC Calibrate",
        )
        .number(
            AirQualityCluster.AttributeDefs.temp_trim.name,
            AirQualityCluster.cluster_id,
            endpoint_id=10,
            min_value=-5000,
            max_value=5000,
            step=10,
            entity_type=EntityType.STANDARD,
            translation_key="temp_trim",
            fallback_name="Temp Trim (m°C)",
        )
        .number(
            AirQualityCluster.AttributeDefs.pressure.name,
            AirQualityCluster.cluster_id,
            endpoint_id=10,
            min_value=700,
            max_value=1100,
            step=1,
            entity_type=EntityType.STANDARD,
            translation_key="pressure",
            fallback_name="Pressure (hPa)",
        )
        .sensor(
            AirQualityCluster.AttributeDefs.ens210_temp.name,
            AirQualityCluster.cluster_id,
            endpoint_id=10,
            multiplier=0.01,
            unit="°C",
            state_class=SensorStateClass.MEASUREMENT,
            device_class=SensorDeviceClass.TEMPERATURE,
            translation_key="ens210_temp",
            fallback_name="ENS210 Temperature",
        )
        .sensor(
            AirQualityCluster.AttributeDefs.ens210_humidity.name,
            AirQualityCluster.cluster_id,
            endpoint_id=10,
            multiplier=0.01,
            unit="%",
            state_class=SensorStateClass.MEASUREMENT,
            device_class=SensorDeviceClass.HUMIDITY,
            translation_key="ens210_humidity",
            fallback_name="ENS210 Humidity",
        )
        .add_to_registry()
    )
else:
    from zigpy.quirks import CustomDevice
    from zigpy.profiles import zha
    from zigpy.zcl.clusters.general import AnalogOutput, Basic, Identify
    from zigpy.zcl.clusters.measurement import (
        CarbonDioxideMeasurement,
        RelativeHumidity,
        TemperatureMeasurement,
    )
    from zhaquirks.const import (
        DEVICE_TYPE, ENDPOINTS, INPUT_CLUSTERS,
        MODELS_INFO, OUTPUT_CLUSTERS, PROFILE_ID,
    )

    class AirCube(CustomDevice):
        signature = {
            MODELS_INFO: [("Bart&Matthijs", "AirCube")],
            ENDPOINTS: {
                10: {
                    PROFILE_ID: zha.PROFILE_ID,
                    DEVICE_TYPE: zha.DeviceType.TEMPERATURE_SENSOR,
                    INPUT_CLUSTERS: [
                        Basic.cluster_id,
                        Identify.cluster_id,
                        AnalogOutput.cluster_id,
                        TemperatureMeasurement.cluster_id,
                        RelativeHumidity.cluster_id,
                        CarbonDioxideMeasurement.cluster_id,
                        AirQualityCluster.cluster_id,
                    ],
                    OUTPUT_CLUSTERS: [],
                },
            },
        }
        replacement = {
            ENDPOINTS: {
                10: {
                    PROFILE_ID: zha.PROFILE_ID,
                    DEVICE_TYPE: zha.DeviceType.TEMPERATURE_SENSOR,
                    INPUT_CLUSTERS: [
                        Basic,
                        Identify,
                        AnalogOutput,
                        TemperatureMeasurement,
                        RelativeHumidity,
                        CarbonDioxideMeasurement,
                        AirQualityCluster,
                    ],
                    OUTPUT_CLUSTERS: [],
                },
            },
        }
