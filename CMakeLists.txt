cmake_minimum_required(VERSION 3.16)
project(iot-broker C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)
add_compile_definitions(_GNU_SOURCE)

find_package(PkgConfig REQUIRED)

# Third-party libraries
pkg_check_modules(LIBEVENT REQUIRED libevent)
pkg_check_modules(CJSON REQUIRED libcjson)

find_package(Threads REQUIRED)

file(GLOB_RECURSE IOT_BROKER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(iot-broker ${IOT_BROKER_SOURCES})

target_include_directories(iot-broker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common
    ${CMAKE_CURRENT_SOURCE_DIR}/include/server
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mqtt
    ${CMAKE_CURRENT_SOURCE_DIR}/include/business
    ${CMAKE_CURRENT_SOURCE_DIR}/include/api

    # libevent
    ${LIBEVENT_INCLUDE_DIRS}

    # cJSON
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(iot-broker PRIVATE
    ${LIBEVENT_LIBRARIES}

    # cJSON
    ${CJSON_LIBRARIES}

    Threads::Threads
)

target_link_directories(iot-broker PRIVATE
    ${LIBEVENT_LIBRARY_DIRS}
    ${CJSON_LIBRARY_DIRS}
)

target_link_options(iot-broker PRIVATE
    ${LIBEVENT_LDFLAGS_OTHER}
    ${CJSON_LDFLAGS_OTHER}
)

# ---------------------------------------------------------------------------
# Convenience targets (mirror the old Makefile run/test goals)
# ---------------------------------------------------------------------------

# iot-broker --port 1883 --workers 4
add_custom_target(run
    COMMAND iot-broker --port 1883 --workers 4
    DEPENDS iot-broker
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running iot-broker on :1883 with 4 workers"
    VERBATIM)

# C unit test: topic matching
add_executable(test_mqtt_topic test/unit/test_mqtt_topic.c src/mqtt/mqtt_topic.c)
target_include_directories(test_mqtt_topic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/mqtt)

add_custom_target(test_mqtt_topic_run
    COMMAND test_mqtt_topic
    DEPENDS test_mqtt_topic
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running MQTT topic matching unit tests"
    VERBATIM)

# Python integration / regression tests
add_custom_target(test_integration
    COMMAND python3 test/integration/test_connect_flow.py
    DEPENDS iot-broker
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Phase 2 MQTT integration tests"
    VERBATIM)

add_custom_target(test_pub_sub
    COMMAND python3 test/integration/test_pub_sub.py
    DEPENDS iot-broker
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Phase 3 PUB/SUB integration tests"
    VERBATIM)

add_custom_target(test_phase4
    COMMAND python3 test/integration/test_phase4.py
    DEPENDS iot-broker
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Phase 4 keepalive / QoS1 / Will tests"
    VERBATIM)

# Cross-platform bench
add_custom_target(bench
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/test/bench/benchtool.py
            --broker $<TARGET_FILE:iot-broker>
            --port 65183
            --workers 4
            --clients 500
            --threshold 2.0
    DEPENDS iot-broker
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running bench: 500 clients against iot-broker on :65183"
    VERBATIM)

add_custom_target(bench_external
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/test/bench/benchtool.py
            --no-restart
            --port 65183
            --clients 500
            --threshold 2.0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running bench against an already-running broker on :65183"
    VERBATIM)
