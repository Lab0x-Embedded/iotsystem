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

# QTP0001: Qt 6.5+ QML 模块默认资源前缀统一为 ':/qt/qml/'
# 第三方库(QtShadcn)自己没设，会在 configure 时刷 author 警告；在 add_subdirectory
# 之前设置好，策略会向下继承到子目录。
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
endif()

# QtShadcn UI 组件库 (third_party submodule, 作为子项目不编 showcase)
add_subdirectory(third_party/qtshadcn)

# 设置 macOS 图标
set(MACOSX_BUNDLE_ICON_FILE logo.icns)
set(app_icon_macos "${CMAKE_SOURCE_DIR}/qml/assets/logo.icns")
set_source_files_properties(${app_icon_macos} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")

qt_add_executable(IoTDeviceManager MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    ${app_icon_macos}
)

set_target_properties(IoTDeviceManager PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "IoTDeviceManager"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.e2iot.devicemanager"
    MACOSX_BUNDLE_BUNDLE_VERSION "2.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "2.0"
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
# 编译期注入 QtShadcn QML 模块路径，main_qml.cpp 中 addImportPath 使用
target_compile_definitions(IoTDeviceManager PRIVATE
    QTSHADCN_IMPORT_PATH="${CMAKE_BINARY_DIR}/third_party/qtshadcn/src"
)
