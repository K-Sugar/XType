include(Catch)

add_executable(test_context_buffer test_context_buffer.cpp)
target_link_libraries(test_context_buffer PRIVATE
    context_buffer_obj
    Catch2::Catch2WithMain
)

catch_discover_tests(test_context_buffer)

add_executable(test_corpus_collector test_corpus_collector.cpp)
target_link_libraries(test_corpus_collector PRIVATE
    corpus_collector_obj
    embed_client_obj
    style_profile_obj
    path_utils_obj
    CURL::libcurl
    Catch2::Catch2WithMain
)

catch_discover_tests(test_corpus_collector)

add_executable(test_style_profile test_style_profile.cpp)
target_link_libraries(test_style_profile PRIVATE
    embed_client_obj
    style_profile_obj
    path_utils_obj
    CURL::libcurl
    Catch2::Catch2WithMain
)

catch_discover_tests(test_style_profile)

add_executable(test_prompt_builder test_prompt_builder.cpp)
target_link_libraries(test_prompt_builder PRIVATE
    prompt_builder_obj
    embed_client_obj
    style_profile_obj
    path_utils_obj
    CURL::libcurl
    Catch2::Catch2WithMain
)

catch_discover_tests(test_prompt_builder)

add_executable(test_embed_client test_embed_client.cpp)
target_link_libraries(test_embed_client PRIVATE
    embed_client_obj
    style_profile_obj
    path_utils_obj
    CURL::libcurl
    Catch2::Catch2WithMain
)

catch_discover_tests(test_embed_client)

add_executable(test_config_loader test_config_loader.cpp)
target_link_libraries(test_config_loader PRIVATE
    config_loader_obj
    path_utils_obj
    Catch2::Catch2WithMain
)

catch_discover_tests(test_config_loader
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(test_phrase_blocklist test_phrase_blocklist.cpp)
target_include_directories(test_phrase_blocklist PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_phrase_blocklist PRIVATE
    Catch2::Catch2WithMain
)
catch_discover_tests(test_phrase_blocklist)

add_executable(test_xtype_helpers test_xtype_helpers.cpp)
target_include_directories(test_xtype_helpers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_xtype_helpers PRIVATE
    Catch2::Catch2WithMain
)
catch_discover_tests(test_xtype_helpers)

add_executable(test_inference_client test_inference_client.cpp)
target_link_libraries(test_inference_client PRIVATE
    Catch2::Catch2WithMain
)
catch_discover_tests(test_inference_client)
