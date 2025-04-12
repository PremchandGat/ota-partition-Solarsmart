if(IDF_VERSION_MAJOR GREATER_EQUAL 4)
    idf_component_register(SRC_DIRS src
        INCLUDE_DIRS src
        REQUIRES log nvs_flash mdns wpa_supplicant lwip app_update esp_http_server
        EMBED_FILES )
else()
    set(COMPONENT_SRCDIRS src)
    set(COMPONENT_ADD_INCLUDEDIRS src)
    set(COMPONENT_REQUIRES log nvs_flash mdns wpa_supplicant lwip app_update esp_http_server)
    set(COMPONENT_EMBED_FILES )
    register_component()
endif()
