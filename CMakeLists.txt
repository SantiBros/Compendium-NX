# Persona Mod Manager: install and manage Persona mods on a modded Switch.
#
# Three ways to build:
#
#   Core + unit tests + pmmcli dev tool (host):
#     cmake -S . -B build && cmake --build build && ./build/pmmtests
#
#   The app on Linux/macOS (for UI development, with a fake SD card):
#     cmake -S . -B build-desktop -DPLATFORM_DESKTOP=ON && cmake --build build-desktop
#     MM_SD_ROOT=/tmp/fake-sd ./build-desktop/PersonaModManager
#
#   The app for Switch (.nro), with devkitPro:
#     DEVKITPRO=/opt/devkitpro cmake -S . -B build-switch -DPLATFORM_SWITCH=ON
#     cmake --build build-switch --target PersonaModManager.nro
cmake_minimum_required(VERSION 3.15)

set(APP_BUILD OFF)
if(PLATFORM_DESKTOP OR PLATFORM_SWITCH)
    set(APP_BUILD ON)
    set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library)
    include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
    option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
    cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
    cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
    cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)
    include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)
endif()

file(STRINGS VERSION PMM_VERSION LIMIT_COUNT 1)
project(PersonaModManager VERSION ${PMM_VERSION} LANGUAGES C CXX)
set(APP_TITLE "Persona Mod Manager")
set(APP_AUTHOR "SantiBros")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------- core library
file(GLOB CORE_SOURCES source/core/*.cpp)
add_library(mmcore STATIC ${CORE_SOURCES})
target_include_directories(mmcore PUBLIC source/core source/third_party)
target_compile_options(mmcore PRIVATE -Wall -Wextra)

if(NINTENDO_SWITCH OR PLATFORM_SWITCH)
    # Static portlibs; private deps listed explicitly (pkg-config may see host .pc files).
    target_include_directories(mmcore PUBLIC $ENV{DEVKITPRO}/portlibs/switch/include)
    target_link_directories(mmcore PUBLIC $ENV{DEVKITPRO}/portlibs/switch/lib)
    target_link_libraries(mmcore PUBLIC curl mbedtls mbedx509 mbedcrypto archive zstd lz4 lzma bz2 expat z)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive)
    find_package(CURL REQUIRED)
    find_package(ZLIB REQUIRED)
    target_link_libraries(mmcore PUBLIC PkgConfig::LIBARCHIVE CURL::libcurl ZLIB::ZLIB)
endif()

# ---------------------------------------------------------------- host tools
if(NOT APP_BUILD)
    file(GLOB TEST_SOURCES tests/*.cpp)
    add_executable(pmmtests ${TEST_SOURCES})
    target_link_libraries(pmmtests PRIVATE mmcore)
    target_compile_definitions(pmmtests PRIVATE MM_FIXTURES="${CMAKE_SOURCE_DIR}/tests/fixtures")
    enable_testing()
    add_test(NAME pmmtests COMMAND pmmtests)

    add_executable(pmmcli tools/pmmcli.cpp)
    target_link_libraries(pmmcli PRIVATE mmcore)
    return()
endif()

# ---------------------------------------------------------------- the app
add_subdirectory(library/borealis/library)
find_package(Threads REQUIRED)
if(TARGET yogacore)
    target_compile_options(yogacore PRIVATE -frtti)
endif()

set(APP_RESOURCES ${CMAKE_BINARY_DIR}/resources)
set(BRLS_RES ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/resources)
add_custom_target(app_resources
    COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BRLS_RES}/material ${APP_RESOURCES}/material
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BRLS_RES}/i18n/en-US ${APP_RESOURCES}/i18n/en-US
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BRLS_RES}/img/sys ${APP_RESOURCES}/img/sys
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources ${APP_RESOURCES}
)
if(PLATFORM_DESKTOP)
    add_custom_command(TARGET app_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${BRLS_RES}/font ${APP_RESOURCES}/font)
endif()

file(GLOB_RECURSE APP_SOURCES source/app/*.cpp)
list(APPEND APP_SOURCES source/third_party/qrcodegen/qrcodegen.cpp)
set(APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})
if(PLATFORM_DESKTOP)
    pkg_check_modules(LIBWEBP REQUIRED IMPORTED_TARGET libwebp)
    list(APPEND APP_PLATFORM_LIB PkgConfig::LIBWEBP)
endif()
if(PLATFORM_SWITCH)
    list(APPEND APP_SOURCES ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
    list(APPEND APP_PLATFORM_LIB webp glfw3 EGL glapi drm_nouveau nx m)
endif()

program_target(PersonaModManager "${APP_SOURCES}")
add_dependencies(PersonaModManager app_resources)
target_include_directories(PersonaModManager PRIVATE source/app)
target_compile_options(PersonaModManager PRIVATE -ffunction-sections -fdata-sections -Wall)
target_compile_definitions(PersonaModManager PRIVATE APP_VERSION="${PROJECT_VERSION}" APP_TITLE="${APP_TITLE}")
target_link_libraries(PersonaModManager PRIVATE mmcore borealis ${APP_PLATFORM_LIB})

if(PLATFORM_SWITCH)
    set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)
    add_custom_target(PersonaModManager.nro DEPENDS PersonaModManager app_resources
        COMMAND ${NX_NACPTOOL_EXE} --create "${APP_TITLE}" "${APP_AUTHOR}" "${PROJECT_VERSION}" PersonaModManager.nacp
        COMMAND ${NX_ELF2NRO_EXE} PersonaModManager.elf PersonaModManager.nro --icon=${APP_ICON}
                --nacp=PersonaModManager.nacp --romfsdir=${APP_RESOURCES}
    )
endif()
