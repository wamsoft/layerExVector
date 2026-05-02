set(TPSTUB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

# ThorVG configuration - static library, SW engine only
option(TVG_BUILD_SHARED OFF)
option(TVG_THREADS ON)
option(TVG_PARTIAL ON)
option(TVG_SIMD ON)
option(TVG_LOG OFF)
option(TVG_FILE_IO OFF)

# Enable SW engine only
option(TVG_ENGINE_SW ON)
option(TVG_ENGINE_GL OFF)
option(TVG_ENGINE_WG OFF)

# Loaders - enable what we need
option(TVG_LOADER_SVG ON)
option(TVG_LOADER_PNG ON)
option(TVG_LOADER_JPG ON)
option(TVG_LOADER_LOTTIE ON)
option(TVG_LOADER_TTF OFF)
option(TVG_LOADER_FT ON)
option(TVG_LOADER_WEBP OFF)

# Disable savers and tools
option(TVG_SAVER_GIF OFF)
option(TVG_TOOL_SVG2PNG OFF)
option(TVG_TOOL_LOTTIE2GIF OFF)

# Disable bindings and tests
option(TVG_BINDINGS_CAPI OFF)
option(TVG_BUILD_TESTS OFF)

# Add ThorVG subdirectory
add_subdirectory(thorvg EXCLUDE_FROM_ALL)

krkrz_plugin(layerExVector
    NCBIND
    SOURCES
        LayerExDraw.cpp
        main.cpp
    LIBRARIES
        thorvg
)
