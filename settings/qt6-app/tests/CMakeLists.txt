find_package(Catch2 3 REQUIRED)

add_executable(xtype-settings-tests
    test_config_store.cpp
    ${CMAKE_SOURCE_DIR}/src/config_store.cpp
)

target_include_directories(xtype-settings-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/../   # for build_info.h
)

target_compile_definitions(xtype-settings-tests PRIVATE
    XTYPE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/data/fixtures"
)

target_link_libraries(xtype-settings-tests PRIVATE
    Qt6::Core
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(xtype-settings-tests)
