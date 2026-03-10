idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES hw_hal protocol comms monitor webui utils freertos esp_system)
