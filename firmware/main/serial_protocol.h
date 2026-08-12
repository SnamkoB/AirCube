//
// Serial Protocol Header
// JSON-based bidirectional communication over USB-Serial-JTAG
//

#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

void serial_protocol_init(void);

void serial_send_sensor_data(uint8_t ens210_status, float temperature_c, float humidity,
                             const char* model, int co2_ppm);

void serial_process_commands(void);

#endif // SERIAL_PROTOCOL_H
