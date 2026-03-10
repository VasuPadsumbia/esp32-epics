idf_component_register(SRCS "webui.c" INCLUDE_DIRS "."
    REQUIRES esp_http_server json monitor hw_hal protocol utils
    EMBED_FILES "index.html")
