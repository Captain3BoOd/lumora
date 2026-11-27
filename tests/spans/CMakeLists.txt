# Tests for the spans module (core/spans). Pure CPU. Written by the maintainer agent.
add_executable(lumora_spans_tests spans_tests.cpp)
target_link_libraries(lumora_spans_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_spans_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/spans)
lumora_target_warnings(lumora_spans_tests)
add_test(NAME lumora_spans_tests COMMAND lumora_spans_tests)
