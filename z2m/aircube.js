/**
 * Zigbee2MQTT External Converter for AirCube (Z2M 1.x)
 *
 * Place this file in your Zigbee2MQTT data directory and reference it
 * in configuration.yaml:
 *
 *   external_converters:
 *     - aircube.js
 *
 * Custom cluster 0xFC01 attributes:
 *   0x0000 = eco2  (uint16, ppm)
 *   0x0001 = etvoc (uint16, ppb)
 *   0x0002 = aqi   (uint16, TVOC-derived VOC Level, 0-500)
 *
 * Standard clusters:
 *   0x0402 = temperature (auto-discovered)
 *   0x0405 = humidity (auto-discovered)
 *   0x040D = true CO2 from SCD4x (Carbon Dioxide Measurement)
 *   0x000D = brightness (Analog Output, writable)
 */

const {temperature, humidity, co2} = require('zigbee-herdsman-converters/lib/modernExtend');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;

const CUSTOM_CLUSTER_ID = 0xFC01;
const ATTR_ECO2  = 0x0000;
const ATTR_ETVOC = 0x0001;
const ATTR_AQI   = 0x0002;

const fzAirCubeAirQuality = {
    cluster: CUSTOM_CLUSTER_ID,
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const result = {};
        if (msg.data.hasOwnProperty(ATTR_ECO2)) {
            result.eco2 = msg.data[ATTR_ECO2];
        }
        if (msg.data.hasOwnProperty(ATTR_ETVOC)) {
            result.voc = msg.data[ATTR_ETVOC];
        }
        if (msg.data.hasOwnProperty(ATTR_AQI)) {
            result.aqi = msg.data[ATTR_AQI];
        }
        return result;
    },
};

const fzAirCubeBrightness = {
    cluster: 'genAnalogOutput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        if (msg.data.hasOwnProperty('presentValue')) {
            return {brightness: Math.round(msg.data.presentValue)};
        }
    },
};

const tzAirCubeBrightness = {
    key: ['brightness'],
    convertSet: async (entity, key, value, meta) => {
        await entity.write('genAnalogOutput', {presentValue: value});
        return {state: {brightness: value}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('genAnalogOutput', ['presentValue']);
    },
};

const definition = {
    zigbeeModel: ['AirCube'],
    model: 'AirCube',
    vendor: 'Bart&Matthijs',
    description: 'AirCube air quality monitor',
    extend: [
        temperature(),
        humidity(),
        co2(),
    ],
    fromZigbee: [fzAirCubeAirQuality, fzAirCubeBrightness],
    toZigbee: [tzAirCubeBrightness],
    exposes: [
        e.numeric('eco2', exposes.access.STATE)
            .withUnit('ppm')
            .withDescription('Equivalent CO2')
            .withValueMin(400)
            .withValueMax(8192),
        e.numeric('voc', exposes.access.STATE)
            .withUnit('ppb')
            .withDescription('tVOC')
            .withValueMin(0)
            .withValueMax(65535),
        e.numeric('aqi', exposes.access.STATE)
            .withUnit('')
            .withDescription('VOC Level (TVOC)')
            .withValueMin(0)
            .withValueMax(500),
        e.numeric('brightness', exposes.access.ALL)
            .withDescription('Brightness')
            .withValueMin(0)
            .withValueMax(100),
    ],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(10);
        await endpoint.bind('msTemperatureMeasurement', coordinatorEndpoint);
        await endpoint.bind('msRelativeHumidity', coordinatorEndpoint);
        await endpoint.bind('msCarbonDioxideMeasurement', coordinatorEndpoint);
        await endpoint.configureReporting('msTemperatureMeasurement', [{
            attribute: 'measuredValue', minimumReportInterval: 1,
            maximumReportInterval: 60, reportableChange: 50,
        }]);
        await endpoint.configureReporting('msRelativeHumidity', [{
            attribute: 'measuredValue', minimumReportInterval: 1,
            maximumReportInterval: 60, reportableChange: 100,
        }]);
        await endpoint.configureReporting('msCarbonDioxideMeasurement', [{
            attribute: 'measuredValue', minimumReportInterval: 1,
            maximumReportInterval: 60, reportableChange: 50,
        }]);
        await endpoint.bind('genAnalogOutput', coordinatorEndpoint);
        await endpoint.configureReporting('genAnalogOutput', [{
            attribute: 'presentValue', minimumReportInterval: 1,
            maximumReportInterval: 60, reportableChange: 5,
        }]);
    },
};

module.exports = definition;
