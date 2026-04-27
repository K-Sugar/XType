cmake_minimum_required(VERSION 3.21)
project(xtype-settings VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.7)
qt_policy(SET QTP0004 NEW)

find_package(KF6WindowSystem QUIET)
if(KF6WindowSystem_FOUND)
    add_compile_definitions(HAVE_KF6_WINDOW_SYSTEM)
endif()

configure_file(src/build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/build_info.h @ONLY)

qt_add_executable(xtype-settings
    src/main.cpp
    # TUs added in U1b (config_store, engine_probe, …) — list grows then.
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(xtype-settings
    URI XType.Settings
    VERSION 1.0
    QML_FILES
        qml/App.qml
        qml/Theme.qml
        qml/primitives/XEngineDot.qml
        qml/primitives/XGrain.qml
        qml/primitives/XMiniBar.qml
        qml/primitives/XSidebar.qml
        qml/primitives/XTitleBar.qml
    RESOURCES
        data/icons/xtype.svg
        data/fonts/Inter-Variable.ttf
        data/fonts/JetBrainsMono-Variable.ttf
)

target_include_directories(xtype-settings PRIVATE
    src ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(xtype-settings PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    $<$<TARGET_EXISTS:KF6::WindowSystem>:KF6::WindowSystem>
)

install(TARGETS xtype-settings RUNTIME DESTINATION bin)
install(FILES data/xtype-settings.desktop DESTINATION share/applications)
install(FILES data/icons/xtype.svg
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME xtype.svg)
