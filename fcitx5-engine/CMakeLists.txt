cmake_minimum_required(VERSION 3.19)
project(xtype-fcitx5 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Fcitx5Core 5.0 REQUIRED)
find_package(CURL REQUIRED)

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
endif()

# context_buffer compiled once; reused by the addon and the test binary
add_library(context_buffer_obj OBJECT src/context_buffer.cpp)
target_include_directories(context_buffer_obj PUBLIC src)

add_library(xtype-fcitx5 SHARED
    src/xtype.cpp
    src/inference_client.cpp
)
target_include_directories(xtype-fcitx5 PRIVATE src)
target_link_libraries(xtype-fcitx5 PRIVATE
    context_buffer_obj
    Fcitx5::Core
    CURL::libcurl
)

set(ADDON_LIB "libxtype-fcitx5")
configure_file(data/xtype.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/data/xtype.conf @ONLY)

install(TARGETS xtype-fcitx5
    LIBRARY DESTINATION "${FCITX_INSTALL_ADDONDIR}"
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/data/xtype.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
)
install(FILES data/xtype-im.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
    RENAME xtype.conf
)

if(BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
