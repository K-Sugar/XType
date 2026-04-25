cmake_minimum_required(VERSION 3.19)
project(xtype-fcitx5 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Fcitx5Core 5.0 REQUIRED)
find_package(CURL REQUIRED)

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
endif()

add_library(xtype-fcitx5 SHARED
    src/xtype.cpp
    src/context_buffer.cpp
    src/inference_client.cpp
)

target_include_directories(xtype-fcitx5 PRIVATE src)

target_link_libraries(xtype-fcitx5 PRIVATE
    Fcitx5::Core
    CURL::libcurl
)

fcitx5_translate_desktop_file(data/xtype-addon.conf.in data/xtype-addon.conf)

install(TARGETS xtype-fcitx5
    LIBRARY DESTINATION "${FCITX_INSTALL_ADDONDIR}"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/data/xtype-addon.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
)
install(FILES data/xtype.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
