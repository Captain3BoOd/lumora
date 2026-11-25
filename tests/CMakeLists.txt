include(Dependencies)
lumora_fetch_catch2()
add_executable(lumora_tests core_tests.cpp golden_tests.cpp json_limits_tests.cpp null_backend_tests.cpp package_policy_tests.cpp rhi_tests.cpp)
target_link_libraries(lumora_tests PRIVATE lumora_core Catch2::Catch2WithMain)
lumora_target_warnings(lumora_tests)

# Test shaders are compiled to SPIR-V with glslangValidator into C headers at build time.
find_program(GLSLANG_VALIDATOR glslangValidator)
if(GLSLANG_VALIDATOR)
  set(SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
  file(MAKE_DIRECTORY ${SHADER_OUT})
  foreach(stage vert frag)
    add_custom_command(
      OUTPUT ${SHADER_OUT}/triangle_${stage}_spv.h
      COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.1 --vn triangle_${stage}_spv
              -o ${SHADER_OUT}/triangle_${stage}_spv.h ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.${stage}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.${stage}
      VERBATIM)
    list(APPEND SHADER_HEADERS ${SHADER_OUT}/triangle_${stage}_spv.h)
  endforeach()
  add_custom_target(lumora_test_shaders DEPENDS ${SHADER_HEADERS})
  add_dependencies(lumora_tests lumora_test_shaders)
  target_include_directories(lumora_tests PRIVATE ${SHADER_OUT})
  target_compile_definitions(lumora_tests PRIVATE LUMORA_HAS_TEST_SHADERS=1)
endif()

# Machines without glslangValidator (the Windows runner) can use SPIR-V headers built on Linux CI.
set(LUMORA_PRECOMPILED_SPV_DIR "" CACHE PATH "Directory with triangle_vert_spv.h and triangle_frag_spv.h")
if(NOT GLSLANG_VALIDATOR AND LUMORA_PRECOMPILED_SPV_DIR)
  target_include_directories(lumora_tests PRIVATE ${LUMORA_PRECOMPILED_SPV_DIR})
  target_compile_definitions(lumora_tests PRIVATE LUMORA_HAS_TEST_SHADERS=1)
endif()

# DXIL for the D3D12 test: DXC from the Windows SDK (dxc.exe ships in the SDK bin directory).
if(WIN32)
  file(GLOB DXC_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
  list(SORT DXC_CANDIDATES)
  list(REVERSE DXC_CANDIDATES)
  list(GET DXC_CANDIDATES 0 DXC_EXECUTABLE)
  if(DXC_EXECUTABLE)
    message(STATUS "Lumora: DXC at ${DXC_EXECUTABLE}")
    execute_process(COMMAND ${DXC_EXECUTABLE} --version OUTPUT_VARIABLE DXC_VERSION_TEXT
                    ERROR_VARIABLE DXC_VERSION_TEXT OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Lumora: DXC version: ${DXC_VERSION_TEXT}")
    set(DXIL_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${DXIL_OUT})
    foreach(pair "vs;vs_main;vs_6_0" "ps;ps_main;ps_6_0")
      list(GET pair 0 stage)
      list(GET pair 1 entry)
      list(GET pair 2 profile)
      add_custom_command(
        OUTPUT ${DXIL_OUT}/triangle_${stage}_dxil.h
        COMMAND ${DXC_EXECUTABLE} -T ${profile} -E ${entry} -Fh ${DXIL_OUT}/triangle_${stage}_dxil.h
                -Vn triangle_${stage}_dxil ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.hlsl
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.hlsl
        VERBATIM)
      list(APPEND DXIL_HEADERS ${DXIL_OUT}/triangle_${stage}_dxil.h)
    endforeach()
    add_custom_target(lumora_test_dxil DEPENDS ${DXIL_HEADERS})
    add_dependencies(lumora_tests lumora_test_dxil)
    target_include_directories(lumora_tests PRIVATE ${DXIL_OUT})
    target_compile_definitions(lumora_tests PRIVATE LUMORA_HAS_TEST_DXIL=1)
  endif()
endif()

target_compile_definitions(lumora_tests PRIVATE
  LUMORA_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
  LUMORA_GOLDEN_OUT_DIR="${CMAKE_BINARY_DIR}/golden-out")

add_test(NAME lumora_tests COMMAND lumora_tests)

add_subdirectory(scene)
add_subdirectory(render2d)
add_subdirectory(deflate)
add_subdirectory(settings)
add_subdirectory(image)
add_subdirectory(atlas)
add_subdirectory(anim)
add_subdirectory(particles)
add_subdirectory(color)
add_subdirectory(spline)
add_subdirectory(geom)
add_subdirectory(xform)
add_subdirectory(batch)
add_subdirectory(grid)
add_subdirectory(pathfind)
add_subdirectory(textlayout)
add_subdirectory(raster)
add_subdirectory(composite)
add_subdirectory(noise)
add_subdirectory(resample)
add_subdirectory(sdf)
add_subdirectory(spatial)
add_subdirectory(tess)
add_subdirectory(clip)
add_subdirectory(stroke)
add_subdirectory(flatten)
add_subdirectory(outline)
add_subdirectory(vecdraw)
add_subdirectory(ttf)
add_subdirectory(nodetree)
add_subdirectory(glb)
if(LUMORA_ENABLE_LUA)
  add_subdirectory(script)
endif()
