# Tests for the linrec module (core/linrec). Pure CPU. Maintainer module.
add_executable(lumora_linrec_tests linrec_tests.cpp)
target_link_libraries(lumora_linrec_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_linrec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/linrec)
lumora_target_warnings(lumora_linrec_tests)
add_test(NAME lumora_linrec_tests COMMAND lumora_linrec_tests)
