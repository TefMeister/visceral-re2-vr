cmake_minimum_required(VERSION 3.20)
project(visceral_native C CXX)

# Visceral RE2, the clean rebuild (2026-11-07): one REFramework plugin (API 1.15, our patched 76298bd loader).
# Folders: src/core (wiring, bridge, helpers), src/game (what differs between RE2 and Village), src/body (RE2's body, moved
# over unchanged), src/guns and src/hands (Village's code from layer 3 on), src/tools (only with -DVN_TOOLS=ON).
# tools/build.sh builds; tools/deploy.py installs exactly one layer of tools/install-list.txt.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(VN_TOOLS "build the test and recording tools into the plugin (never in a player build)" OFF)

file(GLOB VN_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp src/game/*.cpp src/body/*.cpp src/guns/*.cpp src/hands/*.cpp)
if(VN_TOOLS)
    file(GLOB VN_TOOL_SOURCES CONFIGURE_DEPENDS src/tools/*.cpp)
    list(APPEND VN_SOURCES ${VN_TOOL_SOURCES})
endif()

add_library(visceral_native SHARED ${VN_SOURCES} third_party/miniaudio_impl.c)
target_include_directories(visceral_native PRIVATE include third_party src/core src/game src/body src/guns src/hands src/tools)
set_target_properties(visceral_native PROPERTIES OUTPUT_NAME "visceral_native" PREFIX "")
target_compile_definitions(visceral_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN $<$<BOOL:${VN_TOOLS}>:VN_TOOLS=1>)
set_source_files_properties(third_party/miniaudio_impl.c PROPERTIES COMPILE_OPTIONS "/W0")   # public-domain library, its warnings are not ours
if(MSVC)
    target_compile_options(visceral_native PRIVATE /W4 /utf-8 /wd4267 /wd4189)   # two warnings inside praydog's API.hpp
    target_link_options(visceral_native PRIVATE "/Brepro")                       # reproducible: same source, same hash
endif()
