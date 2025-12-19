idf_component_register(
    SRCS "epics_esp32.c"
    PRIV_REQUIRES esp_driver_usb_serial_jtag driver esp_timer esp_adc
    INCLUDE_DIRS "."
)
