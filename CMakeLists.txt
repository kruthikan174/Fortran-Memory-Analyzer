cmake_minimum_required(VERSION 3.15)
project(FortranCommonAnalyzer CXX)

# ─────────────────────────────────────────────────────────────────────────────
# Compiler settings
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings - treat your code seriously
if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # -Wno-unused-parameter: common in early development
    add_compile_options(-Wno-unused-parameter)
endif()

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")

# ─────────────────────────────────────────────────────────────────────────────
# Include paths (all headers are in include/)
# ─────────────────────────────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ─────────────────────────────────────────────────────────────────────────────
# Target 1: collector  (single-file processor)
# ─────────────────────────────────────────────────────────────────────────────
add_executable(collector
    src/collector.cpp
)

# filesystem linkage (needed on older GCC)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(collector stdc++fs)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Target 2: batch_collect  (directory-level batch processor)
# ─────────────────────────────────────────────────────────────────────────────
add_executable(batch_collect
    src/batch_collect.cpp
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(batch_collect stdc++fs)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Install targets
# ─────────────────────────────────────────────────────────────────────────────
install(TARGETS collector batch_collect DESTINATION bin)

# ─────────────────────────────────────────────────────────────────────────────
# Print usage hint after build
# ─────────────────────────────────────────────────────────────────────────────
add_custom_target(usage
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Build complete ==="
    COMMAND ${CMAKE_COMMAND} -E echo "Usage:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./collector <file.f90> [output.json]"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./batch_collect <input_dir/> <output_dir/>"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Env override:"
    COMMAND ${CMAKE_COMMAND} -E echo "  FLANG_PATH=/usr/bin/flang-18 ./collector file.f90"
)
