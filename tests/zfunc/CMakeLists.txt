# Tests for the zfunc module (core/zfunc). Pure CPU. Owned by the zfunc contributor.
add_executable(lumora_zfunc_tests zfunc_tests.cpp)
target_link_libraries(lumora_zfunc_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_zfunc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/zfunc)
lumora_target_warnings(lumora_zfunc_tests)
add_test(NAME lumora_zfunc_tests COMMAND lumora_zfunc_tests)
