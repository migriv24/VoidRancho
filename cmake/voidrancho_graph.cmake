# voidrancho_graph — the graph analysis behind rancho/graph.hpp: a CSR graph,
# components, PageRank, Leiden, its hierarchy, NMI (namespace rancho::graph).
#
# Void Rancho's since 2026-11-02 (the author's ruling; okf/design/boundaries.md
# §3), moved from Void Maiz. A LEAF: it links nothing (no SQLite, no Void Core),
# so a host can take the graph analysis without the database, and Void Maiz's
# bridge (VoidMaiz:cmake/voidmaiz_graph.cmake) wraps it when Rancho is beside it.
#
# This file stands on its own: include it without add_subdirectory(VoidRancho).
#
#   include("${VOIDRANCHO_ROOT}/cmake/voidrancho_graph.cmake")
#   target_link_libraries(yours PRIVATE voidrancho_graph)
#
# The guard makes the first include define the target and every later one reuse
# it, in any add order: one copy of rancho::graph::* per build. (Two archives
# each holding the same symbols was the 2026-10-30 bug: the linker kept
# whichever it met first, silently.)
if(NOT TARGET voidrancho_graph)
  get_filename_component(_rancho_graph_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
  add_library(voidrancho_graph STATIC "${_rancho_graph_root}/src/graph/graph.cpp")
  target_include_directories(voidrancho_graph PUBLIC "${_rancho_graph_root}/include")
  target_compile_features(voidrancho_graph PUBLIC cxx_std_20)
  set_target_properties(voidrancho_graph PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(NOT MSVC)
    target_compile_options(voidrancho_graph PRIVATE -Wall -Wextra)
  endif()
  message(STATUS "voidrancho_graph: Void Rancho's graph analysis (${_rancho_graph_root})")
  unset(_rancho_graph_root)
endif()
