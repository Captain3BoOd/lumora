# Tests for the fenwick module (core/fenwick). Pure CPU. Owned by the fenwick contributor.
add_executable(lumora_fenwick_tests fenwick_tests.cpp)
target_link_libraries(lumora_fenwick_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_fenwick_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/fenwick)
lumora_target_warnings(lumora_fenwick_tests)
add_test(NAME lumora_fenwick_tests COMMAND lumora_fenwick_tests)
