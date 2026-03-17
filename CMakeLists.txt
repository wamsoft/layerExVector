cmake_minimum_required(VERSION 3.16)

add_compile_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>")
# ignore warnings
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:/wd4819>")
add_compile_definitions("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:_CRT_SECURE_NO_WARNINGS>")

include(../tp_stub/krkrz.cmake)

# ThorVG configuration - static library, SW engine only
set(TVG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(TVG_THREADS ON CACHE BOOL "" FORCE)
set(TVG_PARTIAL ON CACHE BOOL "" FORCE)
set(TVG_SIMD ON CACHE BOOL "" FORCE)
set(TVG_LOG OFF CACHE BOOL "" FORCE)
set(TVG_FILE_IO ON CACHE BOOL "" FORCE)

# Enable SW engine only
set(TVG_ENGINE_SW ON CACHE BOOL "" FORCE)
set(TVG_ENGINE_GL OFF CACHE BOOL "" FORCE)
set(TVG_ENGINE_WG OFF CACHE BOOL "" FORCE)

# Loaders - enable what we need
set(TVG_LOADER_SVG ON CACHE BOOL "" FORCE)
set(TVG_LOADER_PNG ON CACHE BOOL "" FORCE)
set(TVG_LOADER_JPG ON CACHE BOOL "" FORCE)
set(TVG_LOADER_LOTTIE ON CACHE BOOL "" FORCE)
set(TVG_LOADER_TTF ON CACHE BOOL "" FORCE)
set(TVG_LOADER_WEBP OFF CACHE BOOL "" FORCE)

# Disable savers and tools
set(TVG_SAVER_GIF OFF CACHE BOOL "" FORCE)
set(TVG_TOOL_SVG2PNG OFF CACHE BOOL "" FORCE)
set(TVG_TOOL_LOTTIE2GIF OFF CACHE BOOL "" FORCE)

# Disable bindings and tests
set(TVG_BINDINGS_CAPI OFF CACHE BOOL "" FORCE)
set(TVG_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Add ThorVG subdirectory
add_subdirectory(thorvg)

krkrz_plugin(layerExVG
    NCBIND
    SOURCES
        LayerExDraw.cpp
        main.cpp
    LIBRARIES
        thorvg
)

# thorvg の フォントオブジェクトを直接参照するため細工
target_include_directories(layerExVG PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thorvg/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/thorvg/src/renderer
)
