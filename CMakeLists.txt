cmake_minimum_required(VERSION 3.20)
project(hlang CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Ubuntu's libzstd-dev has no CMake package, but LLVM's exports reference
# zstd::libzstd_shared; provide the imported target ourselves. Prefer the
# static archive so hcc stays self-contained.
set(ZLIB_USE_STATIC_LIBS ON)  # prefer libz.a for a self-contained hcc
find_library(ZSTD_STATIC libzstd.a)
find_library(ZSTD_LIB zstd REQUIRED)
if(NOT TARGET zstd::libzstd_shared)
  add_library(zstd::libzstd_shared STATIC IMPORTED)
  if(ZSTD_STATIC)
    set_target_properties(zstd::libzstd_shared PROPERTIES IMPORTED_LOCATION "${ZSTD_STATIC}")
  else()
    set_target_properties(zstd::libzstd_shared PROPERTIES IMPORTED_LOCATION "${ZSTD_LIB}")
  endif()
endif()

find_package(LLVM REQUIRED CONFIG HINTS /usr/lib/llvm-21/lib/cmake/llvm)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")

# ---- runtime library (linked into AOT executables, and into hcc for JIT) ----
file(GLOB_RECURSE HCRT_SOURCES CONFIGURE_DEPENDS runtime/*.c)
add_library(hcrt STATIC ${HCRT_SOURCES})
target_compile_options(hcrt PRIVATE -O2 -fno-strict-aliasing -Wall)
set_property(TARGET hcrt PROPERTY POSITION_INDEPENDENT_CODE ON)

# ---- the compiler ----
file(GLOB_RECURSE HCC_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(hcc ${HCC_SOURCES})
target_include_directories(hcc PRIVATE ${LLVM_INCLUDE_DIRS} src)
target_compile_definitions(hcc PRIVATE ${LLVM_DEFINITIONS_LIST})
target_compile_options(hcc PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter)

llvm_map_components_to_libnames(LLVM_LIBS
  core orcjit native nativecodegen passes support target
  x86asmparser x86codegen
)
target_link_libraries(hcc PRIVATE ${LLVM_LIBS} hcrt)
# Self-contained binary: static LLVM (component archives), static
# libstdc++/libgcc, static zlib if available.
target_link_options(hcc PRIVATE -static-libstdc++ -static-libgcc)
find_library(Z_STATIC libz.a)
if(Z_STATIC)
  target_link_libraries(hcc PRIVATE ${Z_STATIC})
endif()

# Export the runtime archive next to hcc so AOT links can find it.
add_custom_command(TARGET hcc POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:hcrt> $<TARGET_FILE_DIR:hcc>/libhcrt.a)

# Embed lib/ (HolyC prelude) path for development; hcc also searches
# relative to its own binary for installed/self-contained use.
target_compile_definitions(hcc PRIVATE HCC_DEV_LIB_DIR="${CMAKE_SOURCE_DIR}/lib")
add_custom_target(prelude ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/lib
  COMMENT "Syncing HolyC prelude")

# ---- tests (ctest: one process per test, run in parallel via `ctest -j`) ----
enable_testing()
set(RUN_ONE ${CMAKE_SOURCE_DIR}/tests/run_one.sh)

# each add_test runs run_one.sh with HCC pointed at the freshly built hcc;
# exit code 77 means "not applicable for this mode" -> reported as skipped.
function(hc_add_test tname mode file)
  add_test(NAME ${tname}
           COMMAND ${CMAKE_COMMAND} -E env HCC=$<TARGET_FILE:hcc>
                   sh ${RUN_ONE} ${mode} ${file})
  set_tests_properties(${tname} PROPERTIES SKIP_RETURN_CODE 77)
endfunction()

file(GLOB CASE_TESTS  CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/cases/*.HC)
file(GLOB ERROR_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/errors/*.HC)
file(GLOB FE_TESTS    CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/frontend/*.HC)

foreach(hc ${CASE_TESTS})
  get_filename_component(name ${hc} NAME_WE)
  hc_add_test(case.${name}.jit jit ${hc})   # golden output under JIT
  hc_add_test(case.${name}.aot aot ${hc})   # ...and under AOT
endforeach()
foreach(hc ${ERROR_TESTS})
  get_filename_component(name ${hc} NAME_WE)
  hc_add_test(error.${name} error ${hc})    # must fail with the //ERR: message
endforeach()
foreach(hc ${FE_TESTS})
  get_filename_component(name ${hc} NAME_WE)
  get_filename_component(dir ${hc} DIRECTORY)
  # register a dump test only when its golden actually exists
  if(EXISTS ${dir}/${name}.tokens)
    hc_add_test(frontend.${name}.tokens tokens ${hc})
  endif()
  if(EXISTS ${dir}/${name}.ast)
    hc_add_test(frontend.${name}.ast ast ${hc})
  endif()
endforeach()

# `ninja check` builds hcc + prelude, then runs the suite in parallel.
include(ProcessorCount)
ProcessorCount(NPROC)
if(NPROC EQUAL 0)
  set(NPROC 4)
endif()
add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -j ${NPROC}
  DEPENDS hcc prelude
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL)
