cmake_minimum_required(VERSION 3.20)
project(voidrancho VERSION 0.1.0 LANGUAGES C CXX)

# Void Rancho — the database layer for Void Core applications (okf/index.md).
# A library: C++20 inside, Void Core's C ABI below, SQLite vendored.
#
#   cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build
#
# Siblings are built from their own repositories beside this one, never copied:
#   ../VoidCore  — linked (the dispatcher, the tag grammar, the §6.1 codec)
#   ../VoidMaiz  — its graph analysis (Leiden) as the target voidmaiz_graph,
#                  defined once per build by cmake/voidmaiz_graph.cmake. Where
#                  graph analytics lives is open (okf/design/boundaries.md §3);
#                  until it is settled Rancho uses Maiz's, it does not move it.
#
# Built as a SUBDIRECTORY of a host that also adds Void Maiz (Void Hormiga, once
# it moves its store), in either order: one voidcore target, one voidmaiz_graph,
# and Rancho's tests stay out of the parent's ctest unless RANCHO_BUILD_TESTS.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(_rancho_top_level ON)
else()
  set(_rancho_top_level OFF)
endif()
option(RANCHO_BUILD_TESTS "Build and register Void Rancho's tests (default: only when built on its own)" ${_rancho_top_level})

# The timings in okf/ were measured RelWithDebInfo; an unoptimized build runs
# the tests about four times slower.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# MinGW: the C++ runtime linked in, so a binary never picks up a different
# toolchain's libstdc++ from PATH (msys2's mingw64 before ucrt64, say). Void
# Core does the same for libgcc.
if(MINGW)
  add_link_options(-static-libgcc -static-libstdc++)
endif()

set(VOIDCORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore" CACHE PATH "Void Core's repository")
set(VOIDMAIZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidMaiz" CACHE PATH "Void Maiz's repository (for Leiden)")

# EITHER SPELLING OF THE ROOT. VOIDCORE_ROOT is one cache variable the family
# reads two ways: here it is Core's repository, while Void Maiz and Void Squid
# mean its core/ directory. In a superproject the first definition wins, so
# accept both — as a normal variable, shadowing the cache in this directory only.
if(EXISTS "${VOIDCORE_ROOT}/include/voidcore.h" AND NOT EXISTS "${VOIDCORE_ROOT}/core/CMakeLists.txt")
  get_filename_component(VOIDCORE_ROOT "${VOIDCORE_ROOT}/.." ABSOLUTE) # the core/ spelling
endif()
# A parent's voidcore target (Void Maiz wraps its prebuilt as one) answers the
# question the check asks: one process, one Core.
if(NOT TARGET voidcore AND NOT EXISTS "${VOIDCORE_ROOT}/core/CMakeLists.txt")
  message(FATAL_ERROR "Void Core not found at ${VOIDCORE_ROOT} — clone it beside this repository or set VOIDCORE_ROOT")
endif()
if(NOT TARGET voidcore)
  add_subdirectory("${VOIDCORE_ROOT}/core" voidcore EXCLUDE_FROM_ALL)
endif()

# Vendored SQLite (public domain — vendor/sqlite/LICENSE.md), the same build
# options as Void Hormiga's, so a store either one writes the other opens.
add_library(rancho_sqlite3 STATIC vendor/sqlite/sqlite3.c)
target_include_directories(rancho_sqlite3 PUBLIC vendor/sqlite)
target_compile_definitions(rancho_sqlite3 PRIVATE
  SQLITE_OMIT_LOAD_EXTENSION # no runtime extension loading (and no libdl)
  SQLITE_DQS=0               # reject double-quoted string literals (hygiene)
)
set_target_properties(rancho_sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(RANCHO_SOURCES
  src/json_stream.cpp
  src/core.cpp
  src/media.cpp
  src/store.cpp
  src/working_set.cpp
  src/plan.cpp
  src/matrix.cpp
  src/linalg.cpp
  src/segments.cpp
  src/capi.cpp
)

# Leiden: Void Maiz's voidmaiz_graph target, never a second copy of it. Compiled
# into our own archive (as 0.1.0 did), maiz::graph::* reached a host linking
# both of us twice, and the linker kept whichever archive it met first. The
# fragment defines the target once per build, in any add order; a parent that
# already has it needs no path at all. The graph.cpp fallback is for a Void Maiz
# older than the fragment (2026-10-31).
set(RANCHO_HAVE_LEIDEN 0)
set(RANCHO_LEIDEN_FROM_SOURCE OFF)
if(NOT TARGET voidmaiz_graph AND EXISTS "${VOIDMAIZ_ROOT}/cmake/voidmaiz_graph.cmake")
  include("${VOIDMAIZ_ROOT}/cmake/voidmaiz_graph.cmake")
endif()
if(TARGET voidmaiz_graph)
  set(RANCHO_HAVE_LEIDEN 1)
  message(STATUS "Void Rancho: clustering by Void Maiz's Leiden (target voidmaiz_graph)")
elseif(EXISTS "${VOIDMAIZ_ROOT}/src/graph/graph.cpp")
  list(APPEND RANCHO_SOURCES "${VOIDMAIZ_ROOT}/src/graph/graph.cpp")
  set(RANCHO_HAVE_LEIDEN 1)
  set(RANCHO_LEIDEN_FROM_SOURCE ON)
  message(STATUS "Void Rancho: clustering by Void Maiz's Leiden, compiled in place (an older Void Maiz: ${VOIDMAIZ_ROOT})")
else()
  message(WARNING "Void Maiz not found at ${VOIDMAIZ_ROOT}: Store::cluster will report it is unavailable")
endif()

add_library(voidrancho STATIC ${RANCHO_SOURCES})
target_include_directories(voidrancho
  PUBLIC  include
  PRIVATE vendor/json)
if(TARGET voidmaiz_graph)
  target_link_libraries(voidrancho PUBLIC voidmaiz_graph) # and voidmaiz/graph.hpp's include dir
elseif(RANCHO_LEIDEN_FROM_SOURCE)
  target_include_directories(voidrancho PUBLIC "${VOIDMAIZ_ROOT}/include")
endif()
target_compile_definitions(voidrancho PRIVATE RANCHO_HAVE_LEIDEN=${RANCHO_HAVE_LEIDEN})
target_link_libraries(voidrancho PUBLIC voidcore rancho_sqlite3)
if(WIN32)
  target_link_libraries(voidrancho PUBLIC psapi) # rancho/memory.hpp
endif()
if(NOT MSVC)
  target_compile_options(voidrancho PRIVATE -Wall -Wextra)
endif()

# ── tests ────────────────────────────────────────────────────────────────────
# Each is one executable, one function per promise, named for what it promises.
# On by default only when Rancho is the top-level project: a host that adds us
# as a subdirectory keeps its ctest to its own tests unless it asks for ours.
if(RANCHO_BUILD_TESTS)
  enable_testing()
  foreach(t store_test query_test working_set_test plan_test matrix_test media_test segments_test capi_test)
    add_executable(rancho_${t} tests/${t}.cpp)
    target_link_libraries(rancho_${t} PRIVATE voidrancho)
    target_include_directories(rancho_${t} PRIVATE vendor/json tests)
    target_compile_definitions(rancho_${t} PRIVATE RANCHO_HAVE_LEIDEN=${RANCHO_HAVE_LEIDEN})
    add_test(NAME rancho_${t} COMMAND rancho_${t})
  endforeach()

  # Void Core is a shared library: put it beside the tests so they start.
  add_custom_target(rancho_stage_voidcore ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:voidcore> ${CMAKE_BINARY_DIR}/bin/
    DEPENDS voidcore)
endif()
