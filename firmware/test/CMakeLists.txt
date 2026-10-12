# Tests hôtes de la logique firmware N2 (aucune dépendance ESP-IDF).
#   cmake -S firmware/test -B build-host && cmake --build build-host && ctest --test-dir build-host
cmake_minimum_required(VERSION 3.16)
project(n2_firmware_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror -Wshadow -Wconversion -Wno-sign-conversion -g -O1)
if(NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_subdirectory(${FW_DIR}/components/n2_core ${CMAKE_BINARY_DIR}/n2_core)

enable_testing()

set(TRACES ${CMAKE_CURRENT_SOURCE_DIR}/traces)

function(n2_test name)
    add_executable(${name} ${name}.c ${ARGN})
    target_link_libraries(${name} PRIVATE n2_core)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(${name} PRIVATE TRACES_DIR="${TRACES}")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

n2_test(test_decoder trace_util.c)
n2_test(test_proto_log)
n2_test(test_health trace_util.c)
n2_test(test_alarm)
n2_test(test_evlog)
n2_test(test_security)

# Revue automatique (7.2) : aucune API d'écriture sur le bus Essensys.
add_test(NAME check_no_bus_write COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/check_no_bus_write.sh ${FW_DIR})
# Dépôt public : aucun secret, aucune donnée privée dans firmware/.
add_test(NAME check_confidentiality COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/check_confidentiality.sh ${FW_DIR})

# Les traces versionnées doivent correspondre au générateur.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME traces_up_to_date
             COMMAND sh -c "'${Python3_EXECUTABLE}' '${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_traces.py' '${CMAKE_BINARY_DIR}/traces' && diff -r '${CMAKE_BINARY_DIR}/traces' '${TRACES}'")
endif()
