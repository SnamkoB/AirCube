idf_component_register(SRCS "main.c"
                                "auto_trim.c"
                                "led.c"
                                "led_color_lib.c"
                                "ws2812_control.c"
                                "ens210.c"
                                "ens16x_driver.c"
                                "scd41.c"
                                "sensor_ema.c"
                                "i2c_driver.c"
                                "serial_protocol.c"
                                "button.c"
                       PRIV_REQUIRES spi_flash esp_timer esp_driver_rmt esp_driver_i2c esp_driver_uart esp_driver_usb_serial_jtag vfs esp_pm esp_driver_gpio nvs_flash esp_app_format
                                     espressif__esp-zigbee-lib espressif__esp-zboss-lib openthread
                       INCLUDE_DIRS "")

# Add the network-specific source file
if(CONFIG_AIRCUBE_NETWORK_ZIGBEE)
    target_sources(${COMPONENT_LIB} PRIVATE "zigbee.c")
elseif(CONFIG_AIRCUBE_NETWORK_THREAD)
    target_sources(${COMPONENT_LIB} PRIVATE "thread_network.c")
endif()
