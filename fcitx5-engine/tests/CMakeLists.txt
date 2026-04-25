include(Catch)

add_executable(test_context_buffer test_context_buffer.cpp)
target_link_libraries(test_context_buffer PRIVATE
    context_buffer_obj
    Catch2::Catch2WithMain
)

catch_discover_tests(test_context_buffer)
