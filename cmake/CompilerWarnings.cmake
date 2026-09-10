# A single include(), placed before the first add_library(), applies these project-wide
# via add_compile_options() rather than SushiDSPCompilerFlags.cmake's per-target function --
# this file has no per-target call site of its own to hook into.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
elseif(MSVC)
    add_compile_options(/W4)
endif()
