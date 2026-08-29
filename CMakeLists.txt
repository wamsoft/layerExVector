cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_compile_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C>>:/utf-8>")
    add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/utf-8>")
    add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>")
    project(dummy)
endif()


set(TPSTUB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

# ThorVG: 親プロジェクトが既に thorvg ターゲットを提供していればそれを使う。
# (krkrz_dev umbrella では他のサブが先に作る場合がある)。
if(NOT TARGET thorvg)
    # ThorVG configuration - static library, SW engine only
    # option(NAME "description" DEFAULT) の引数順なので、2 つ引数で書くと
    # 第 2 引数が description、デフォルトは OFF になる。明示的に
    # set(... CACHE BOOL ... FORCE) で書いて事故防止。
    set(TVG_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
    set(TVG_THREADS      ON  CACHE BOOL "Enable multi-threading task scheduler" FORCE)
    set(TVG_PARTIAL      ON  CACHE BOOL "Enable Partial Rendering" FORCE)
    set(TVG_SIMD         ON  CACHE BOOL "Enable CPU Vectorization (SIMD)" FORCE)
    set(TVG_LOG          OFF CACHE BOOL "Enable log message" FORCE)
    set(TVG_FILE_IO      OFF CACHE BOOL "Enable File IO calls" FORCE)

    # Enable SW engine only
    set(TVG_ENGINE_SW    ON  CACHE BOOL "Enable Software Rasterizer Engine" FORCE)
    set(TVG_ENGINE_GL    OFF CACHE BOOL "Enable OpenGL Rasterizer Engine" FORCE)
    set(TVG_ENGINE_WG    OFF CACHE BOOL "Enable WebGPU Rasterizer Engine" FORCE)

    # Loaders - enable what we need
    set(TVG_LOADER_SVG    ON  CACHE BOOL "Enable SVG Loader" FORCE)
    set(TVG_LOADER_PNG    ON  CACHE BOOL "Enable PNG Loader" FORCE)
    set(TVG_LOADER_JPG    ON  CACHE BOOL "Enable JPG Loader" FORCE)
    set(TVG_LOADER_LOTTIE ON  CACHE BOOL "Enable Lottie Loader" FORCE)
    set(TVG_LOADER_TTF    OFF CACHE BOOL "Enable TTF Loader" FORCE)
    # ホスト (krkrz) が glyphware を持つ場合はブリッジローダ (GW) を使い、
    # thorvg 内蔵の FreeType+HarfBuzz スタックを外す (ブリッジは本体側が注入)。
    # 単体ビルド等 glyphware 無し環境は従来どおり FT ローダ。
    if(KRKRZ_USE_GLYPHWARE)
        set(TVG_LOADER_FT OFF CACHE BOOL "Disable in-tree FT+HarfBuzz text loader" FORCE)
        set(TVG_LOADER_GW ON  CACHE BOOL "Enable host-bridge (glyphware) text loader" FORCE)
    else()
        set(TVG_LOADER_FT ON  CACHE BOOL "Enable FT+HarfBuzz multilingual text loader" FORCE)
        set(TVG_LOADER_GW OFF CACHE BOOL "Disable host-bridge text loader" FORCE)
    endif()
    set(TVG_LOADER_WEBP   OFF CACHE BOOL "Enable WebP Loader" FORCE)

    # Disable savers and tools
    set(TVG_SAVER_GIF        OFF CACHE BOOL "Enable GIF Saver" FORCE)
    set(TVG_TOOL_SVG2PNG     OFF CACHE BOOL "Build svg2png tool" FORCE)
    set(TVG_TOOL_LOTTIE2GIF  OFF CACHE BOOL "Build lottie2gif tool" FORCE)

    # Disable bindings and tests
    set(TVG_BINDINGS_CAPI    OFF CACHE BOOL "Enable C API bindings" FORCE)
    set(TVG_BUILD_TESTS      OFF CACHE BOOL "Build unit tests" FORCE)

    # Add ThorVG subdirectory
    add_subdirectory(thorvg EXCLUDE_FROM_ALL)
endif()

krkrz_plugin(layerExVector
    NCBIND
    SOURCES
        LayerExDraw.cpp
        LicensesGen.cpp
        main.cpp
    LIBRARIES
        thorvg
)

# gw テキストローダ使用時はブリッジ注入コード (initThorvg) を有効化
if(TVG_LOADER_GW)
    target_compile_definitions(layerExVector PRIVATE LAYEREXVECTOR_TVG_GW=1)
endif()

# Lottie ローダが有効なときだけ、 Lottie 専用の同梱物 (JerryScript / RapidJSON)
# のライセンスを登録する。 OFF のときはソースが 1 本もコンパイルされないので
# 一覧に出さない (licenses/manifest.json の "when" と対応)。
if(TVG_LOADER_LOTTIE)
    target_compile_definitions(layerExVector PRIVATE TVP_LICENSE_WITH_LOTTIE=1)
endif()
