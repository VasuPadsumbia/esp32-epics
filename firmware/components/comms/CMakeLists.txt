idf_component_register(SRCS "comms.c" INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_event esp_netif lwip nvs_flash protocol utils monitor)
