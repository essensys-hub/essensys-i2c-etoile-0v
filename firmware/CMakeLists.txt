# Firmware du module ESP32-C3 de la carte N2 (espion I2C passif, surveillance,
# alarme, surtensions). Build : idf.py set-target esp32c3 && idf.py build
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/components)
# Version d'application (anti-retour arrière en release : CONFIG_BOOTLOADER_APP_SECURE_VERSION).
set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(n2_esp32_spy_alarm)
