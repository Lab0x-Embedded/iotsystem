cmake_minimum_required(VERSION 3.24)
project(IoTDeviceManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt Components for QML (Svg for QtShadcn IconRegistry)
set(QT_COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Charts
    Network
    Svg
)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})
find_package(Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})

# C++ Sources (backend only)
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h" "src/*.hpp")

# QtShadcn UI 组件库 (third_party submodule, 作为子项目不编 showcase)
add_subdirectory(third_party/qtshadcn)

# 设置 macOS 图标
set(MACOSX_BUNDLE_ICON_FILE logo.icns)
set(app_icon_macos "${CMAKE_SOURCE_DIR}/qml/assets/logo.icns")
set_source_files_properties(${app_icon_macos} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")

qt_add_executable(IoTDeviceManager
    ${SOURCES}
    ${HEADERS}
    ${app_icon_macos}
)

# QML module (existing qrc-based approach)
target_sources(IoTDeviceManager PRIVATE qml/qml.qrc)

target_include_directories(IoTDeviceManager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(IoTDeviceManager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Svg
    QtShadcn
)

# QML import path so `import QtShadcn` resolves at runtime
set_target_properties(IoTDeviceManager PROPERTIES
    QT_QML_IMPORT_PATH "${CMAKE_BINARY_DIR}/third_party/qtshadcn/src"
)
