# Fuzz targets for the parsers that read untrusted input. Fixed layout; the contributor fills in the
# fuzz_*.cpp files, adds seed files under corpus/<target>/ and documents it in docs/fuzzing.md.
#  * Every target also builds as a replay executable (replay_main.cpp) and runs its corpus under ctest on every
#    compiler, so the corpus is a regression test.
#  * With Clang and -DLUMORA_BUILD_FUZZERS=ON each target additionally builds as a libFuzzer binary
#    (fuzz_<name>) with ASan and UBSan, for real fuzzing runs; those are not part of ctest.
option(LUMORA_BUILD_FUZZERS "Build libFuzzer binaries (Clang only)" OFF)
set(LUMORA_FUZZ_TARGETS json manifest scene settings zip inflate ppm png animclip atlas glb)
foreach(t ${LUMORA_FUZZ_TARGETS})
  add_executable(fuzz_${t}_replay fuzz_${t}.cpp replay_main.cpp)
  target_link_libraries(fuzz_${t}_replay PRIVATE lumora_core)
  target_include_directories(fuzz_${t}_replay PRIVATE
    ${CMAKE_SOURCE_DIR}/core/scene ${CMAKE_SOURCE_DIR}/core/package ${CMAKE_SOURCE_DIR}/core/render2d ${CMAKE_SOURCE_DIR}/core/image ${CMAKE_SOURCE_DIR}/core/anim ${CMAKE_SOURCE_DIR}/core/glb)
  add_test(NAME fuzz_${t}_replay COMMAND fuzz_${t}_replay ${CMAKE_CURRENT_SOURCE_DIR}/corpus/${t})
  if(LUMORA_BUILD_FUZZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_${t} fuzz_${t}.cpp)
    target_link_libraries(fuzz_${t} PRIVATE lumora_core)
    target_include_directories(fuzz_${t} PRIVATE
      ${CMAKE_SOURCE_DIR}/core/scene ${CMAKE_SOURCE_DIR}/core/package ${CMAKE_SOURCE_DIR}/core/render2d ${CMAKE_SOURCE_DIR}/core/image ${CMAKE_SOURCE_DIR}/core/anim ${CMAKE_SOURCE_DIR}/core/glb)
    target_compile_options(fuzz_${t} PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_${t} PRIVATE -fsanitize=fuzzer,address,undefined)
  endif()
endforeach()
