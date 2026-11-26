# Tests for the glb loader (core/glb). Pure CPU. Owned by the project.
add_executable(lumora_glb_tests glb_tests.cpp)
target_link_libraries(lumora_glb_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_glb_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/glb)
lumora_target_warnings(lumora_glb_tests)
add_test(NAME lumora_glb_tests COMMAND lumora_glb_tests)
