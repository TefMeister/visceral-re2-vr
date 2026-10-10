cmake_minimum_required(VERSION 3.20)
project(visceral_native C CXX)

# Visceral RE2's new native code (2026-10-05): a REFramework plugin, API 1.15 (the DLSS build 76298bd we run).
# VS2022 Build Tools; tools/build.sh builds, checks the exports and deploys.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(visceral_native SHARED
    src/plugin.cpp
    src/common.cpp
    src/bridge.cpp
    src/weapons.cpp
    src/shortcut.cpp
    src/suppress.cpp
    src/holster.cpp
    src/ladder.cpp
    src/run.cpp
    src/fire.cpp
    src/menu_body.cpp
    src/menu_probe.cpp
    src/options.cpp
    src/pickup.cpp
    src/menu_tint.cpp
    src/reload.cpp
    src/reload_block.cpp
    src/sfx.cpp
    src/subweapon.cpp
    third_party/miniaudio_impl.c
    src/spread.cpp
    src/spreadprobe.cpp)
target_include_directories(visceral_native PRIVATE include src)
set_target_properties(visceral_native PROPERTIES OUTPUT_NAME "visceral_native" PREFIX "")
target_compile_definitions(visceral_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
set_source_files_properties(third_party/miniaudio_impl.c PROPERTIES COMPILE_OPTIONS "/W0")   # public-domain library, its warnings are not ours
if(MSVC)
    target_compile_options(visceral_native PRIVATE /W4 /utf-8 /wd4267 /wd4189)   # two warnings inside praydog API.hpp
    target_link_options(visceral_native PRIVATE "/Brepro")                # reproducible: same source, same hash
endif()
