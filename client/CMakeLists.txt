cmake_minimum_required(VERSION 3.16)
project(IoTDeviceManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt Components for QML
set(QT_COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Charts
    Network
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${QT_COMPONENTS})
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QT_COMPONENTS})

# C++ Sources (backend only)
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp")

add_executable(${PROJECT_NAME}
    src/main/main_qml.cpp
    ${SOURCES}
    ${HEADERS}
    qml/qml.qrc
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "IoT Device Manager"
    MACOSX_BUNDLE_BUNDLE_VERSION "2.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "2.0"
    MACOSX_BUNDLE_IDENTIFIER "com.e2iot.device-manager"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.e2iot.device-manager"
)
