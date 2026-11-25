cmake_minimum_required(VERSION 3.21)
project(Clipify VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2 Sql Widgets)
qt_standard_project_setup()

qt_add_executable(clipify WIN32
    src/main.cpp
    src/ClipboardModule.cpp
    src/ClipboardModule.h
    src/DesktopShortcut.cpp
    src/DesktopShortcut.h
)
qt_add_qml_module(clipify
    URI Clipify
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml qml/HistoryPopup.qml qml/HistoryAction.qml
)
target_link_libraries(clipify PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Widgets)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
    target_compile_definitions(clipify PRIVATE CLIPIFY_X11)
    target_link_libraries(clipify PRIVATE X11::X11 Qt6::DBus)
endif()
