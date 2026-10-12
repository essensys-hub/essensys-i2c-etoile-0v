idf_component_register(
    SRCS
        app_main.c
        n2_console.c
        n2_hal.c
        n2_net.c
        n2_ota.c
        n2_prov.c
        n2_sniffer.c
        n2_store.c
    INCLUDE_DIRS .
    REQUIRES
        n2_core
        driver
        esp_timer
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        mqtt
        esp_https_ota
        esp_http_client
        app_update
        esp_partition
        console
        lwip
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Werror)
