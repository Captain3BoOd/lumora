# Tests for the node tree validator (core/nodetree). Pure CPU. Owned by the agent.
add_executable(lumora_nodetree_tests nodetree_tests.cpp)
target_link_libraries(lumora_nodetree_tests PRIVATE lumora_core Catch2::Catch2WithMain)
target_include_directories(lumora_nodetree_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../core/nodetree)
lumora_target_warnings(lumora_nodetree_tests)
add_test(NAME lumora_nodetree_tests COMMAND lumora_nodetree_tests)
