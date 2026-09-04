cmake_minimum_required(VERSION 3.20)
project(visceral_core CXX)

# Visceral's native core: a REFramework plugin (plugin API 1.15, the version the
# pinned RE2 build 76298bd ships). Built with VS2022 Build Tools; see tools/build.sh.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(visceral_core SHARED src/Plugin.cpp)
target_include_directories(visceral_core PRIVATE include)
set_target_properties(visceral_core PROPERTIES OUTPUT_NAME "visceral_core" PREFIX "")
target_compile_definitions(visceral_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(visceral_core PRIVATE user32)
if(MSVC)
    target_compile_options(visceral_core PRIVATE /W4 /utf-8 /wd4267 /wd4189)  # the two praydog API.hpp warnings
endif()
