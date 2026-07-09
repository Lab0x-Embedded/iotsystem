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

# MySQL client library
pkg_check_modules(MYSQL REQUIRED mysqlclient)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/data

    # libevent
    ${LIBEVENT_INCLUDE_DIRS}

    # cJSON
    ${CJSON_INCLUDE_DIRS}

    # MySQL
    ${MYSQL_INCLUDE_DIRS}
)

target_link_libraries(iot-broker PRIVATE
    ${LIBEVENT_LIBRARIES}

    # cJSON
    ${CJSON_LIBRARIES}

    # MySQL
    ${MYSQL_LIBRARIES}

    Threads::Threads
)

target_link_directories(iot-broker PRIVATE
    ${LIBEVENT_LIBRARY_DIRS}
    ${CJSON_LIBRARY_DIRS}
    ${MYSQL_LIBRARY_DIRS}
)

target_link_options(iot-broker PRIVATE
    ${LIBEVENT_LDFLAGS_OTHER}
    ${CJSON_LDFLAGS_OTHER}
    ${MYSQL_LDFLAGS_OTHER}
)
