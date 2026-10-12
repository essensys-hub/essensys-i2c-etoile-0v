# Logique pure de la carte N2 : compilée par ESP-IDF (composant) et sur hôte (tests).
set(N2_CORE_SRCS
    src/alarm.c
    src/bus_health.c
    src/cmd_handler.c
    src/debug_mode.c
    src/essensys_proto.c
    src/event_log.c
    src/frame_log.c
    src/i2c_decoder.c
    src/mqtt_cmd.c
    src/n2_board.c
    src/n2_crc.c
    src/n2_events.c
    src/n2_sha256.c
    src/n2_topics.c
    src/ota_policy.c
    src/pin_guard.c
    src/power_monitor.c
    src/siren.c
    src/surge.c
    src/timebase.c
)

if(ESP_PLATFORM)
    idf_component_register(SRCS ${N2_CORE_SRCS} INCLUDE_DIRS include)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
else()
    add_library(n2_core STATIC ${N2_CORE_SRCS})
    target_include_directories(n2_core PUBLIC include)
endif()
