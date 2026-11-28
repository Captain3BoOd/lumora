# Tests for the binmod module (core/binmod). Pure CPU. Owned by the binmod contributor.
add_executable(lumora_binmod_tests binmod_tests.cpp)
target_link_libraries(lumora_binmod_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_binmod_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/binmod)
lumora_target_warnings(lumora_binmod_tests)
add_test(NAME lumora_binmod_tests COMMAND lumora_binmod_tests)
