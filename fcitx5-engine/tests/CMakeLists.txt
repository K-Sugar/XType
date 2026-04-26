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
    path_utils_obj
    Catch2::Catch2WithMain
)

catch_discover_tests(test_corpus_collector)

add_executable(test_style_profile test_style_profile.cpp)
target_link_libraries(test_style_profile PRIVATE
    style_profile_obj
    path_utils_obj
    Catch2::Catch2WithMain
)

catch_discover_tests(test_style_profile)

add_executable(test_prompt_builder test_prompt_builder.cpp)
target_link_libraries(test_prompt_builder PRIVATE
    prompt_builder_obj
    style_profile_obj
    path_utils_obj
    Catch2::Catch2WithMain
)

catch_discover_tests(test_prompt_builder)
