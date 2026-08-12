/**
 * @file thread_network.h
 * @brief OpenThread networking for AirCube
 *
 * Implements a Thread Sleepy End Device that sends sensor data over
 * the Thread mesh network. Uses OpenThread with a simple UDP-based
 * data reporting protocol.
 *
 * API mirrors zigbee.h so main.c and button.c can use a common
 * interface guarded by Kconfig (AIRUBE_NETWORK_THREAD).
 */

#ifndef THREAD_NETWORK_H
#define THREAD_NETWORK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void thread_network_init(void);

void thread_network_update_sensors(float temp_c, float humidity, int eco2,
                                   int etvoc, int aqi, float co2_ppm, float lux,
                                   float ens210_temp_c, float ens210_humidity);

bool thread_network_is_connected(void);

void thread_network_start_commissioning(void);

bool thread_network_is_commissioning(void);

void thread_network_report_brightness(void);

#ifdef __cplusplus
}
#endif

#endif // THREAD_NETWORK_H
