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
    Catch2::Catch2WithMain
)

catch_discover_tests(test_corpus_collector)
