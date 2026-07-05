cmake_minimum_required(VERSION 3.20)
project(hlang CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Ubuntu's libzstd-dev has no CMake package, but LLVM's exports reference
# zstd::libzstd_shared; provide the imported target ourselves.
find_library(ZSTD_LIB zstd REQUIRED)
if(NOT TARGET zstd::libzstd_shared)
  add_library(zstd::libzstd_shared SHARED IMPORTED)
  set_target_properties(zstd::libzstd_shared PROPERTIES IMPORTED_LOCATION "${ZSTD_LIB}")
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

# Export the runtime archive next to hcc so AOT links can find it.
add_custom_command(TARGET hcc POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:hcrt> $<TARGET_FILE_DIR:hcc>/libhcrt.a)

# Embed lib/ (HolyC prelude) path for development; hcc also searches
# relative to its own binary for installed/self-contained use.
target_compile_definitions(hcc PRIVATE HCC_DEV_LIB_DIR="${CMAKE_SOURCE_DIR}/lib")
add_custom_command(TARGET hcc POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib $<TARGET_FILE_DIR:hcc>/lib)

# ---- tests ----
enable_testing()
add_custom_target(check
  COMMAND ${CMAKE_COMMAND} -E env HCC=$<TARGET_FILE:hcc>
          ${CMAKE_SOURCE_DIR}/tests/run_tests.sh
  DEPENDS hcc
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
