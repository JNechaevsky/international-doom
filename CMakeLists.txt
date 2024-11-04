set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.9)
project("Inter" VERSION 8.1.0 LANGUAGES C)

# Autotools variables
set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR})
set(top_builddir ${CMAKE_CURRENT_BINARY_DIR})

# AC_INIT variables
set(PACKAGE_VERSION_DOOM "8.1")
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_FULLNAME "International Doom ${PACKAGE_VERSION_DOOM}")
set(PACKAGE_TARNAME "inter-doom")
set(PACKAGE_STRING "${PROJECT_NAME} ${PACKAGE_VERSION_DOOM}")

set(PACKAGE_VERSION_HERETIC "8.1")
set(PACKAGE_FULLNAME_HERETIC "International Heretic ${PACKAGE_VERSION_HERETIC}")
set(PACKAGE_TARNAME_HERETIC "inter-heretic")
set(PACKAGE_STRING_HERETIC "${PROJECT_NAME} ${PACKAGE_VERSION_HERETIC}")

set(PACKAGE_VERSION_HEXEN "8.1")
set(PACKAGE_FULLNAME_HEXEN "International Hexen ${PACKAGE_VERSION_HEXEN}")
set(PACKAGE_TARNAME_HEXEN "inter-hexen")
set(PACKAGE_STRING_HEXEN "${PROJECT_NAME} ${PACKAGE_VERSION_HEXEN}")

set(PACKAGE_BUGREPORT "julia.nechaevskaya@live.com")

string(REGEX REPLACE " Doom$" "" PACKAGE_SHORTNAME "${PACKAGE_NAME}")
set(PACKAGE_COPYRIGHT "Copyright (C) 2016-2024 Julia Nechaevskaya")
set(PACKAGE_LICENSE "GNU GPL (version 2)")

# Any settings that should apply to all targets in this directory and all
# subdirectories should go here.  Use judiciously.
if(MSVC)
    add_definitions("/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_SECURE_NO_DEPRECATE"
                    "/D_CRT_NONSTDC_NO_DEPRECATE")
# Disable several warnings for cl.exe.
add_compile_options("/wd4013" # Function undefined; assuming extern returning int
                    "/wd4018" # Signed/unsigned mismatch
                    "/wd4146" # Unary minus operator applied to unsigned type, result still unsigned
                    "/wd4244" # Conversion from 'type1' to 'type2', possible loss of data
                    "/wd4267" # Conversion from 'size_t' to 'type', possible loss of data
                    "/wd4305" # Truncation from 'type1' to 'type2'
                    )
else()
    add_compile_options("-Wall"
                        "-Wdeclaration-after-statement"
                        "-Wredundant-decls"
                        "-Wformat-security"
                        # "-Wno-ignored-qualifiers"
                        # "-Wpedantic"
                        # "-Wextra"
                        )
endif()

# Enable LTO if available.  Hovewer, CMake does not appear to support
# LTO properly for MinGW GCC which results in much longer linking time.
# Consider only enabling it when making a release.
if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT HAVE_LTO)

include(CMakeDependentOption) # Note: replace first ON to OFF to disable.
cmake_dependent_option(ENABLE_LTO "Use link-time optimization" ON "HAVE_LTO" OFF)

# Enable ASan (AddressSanitizer).
# Note: doesn't work with MinGW, use Clang64 if using MSYS enviroment.
option(ENABLE_ASAN "Enable ASan" OFF)
if(ENABLE_ASAN)
    add_compile_options("-fsanitize=address")
    add_link_options(-fsanitize=address)
endif()

option(CMAKE_FIND_PACKAGE_PREFER_CONFIG
       "Lookup package config files before using find modules" On)

option(ENABLE_SDL2_NET "Enable SDL2_net" On)
option(ENABLE_SDL2_MIXER "Enable SDL2_mixer" On)

find_package(SDL2 2.0.18)
if(ENABLE_SDL2_MIXER)
    find_package(SDL2_mixer 2.0.2)
    if(NOT TARGET SDL2_mixer::SDL2_mixer)
        add_library(SDL2_mixer::SDL2_mixer ALIAS SDL2_mixer::SDL2_mixer-static)
    endif()
else()
    add_compile_definitions(DISABLE_SDL2MIXER=1)
endif()

if(ENABLE_SDL2_NET)
    find_package(SDL2_net 2.0.0)
    if(NOT TARGET SDL2_net::SDL2_net)
        add_library(SDL2_net::SDL2_net ALIAS SDL2_net::SDL2_net-static)
    endif()
else()
    add_compile_definitions(DISABLE_SDL2NET=1)
endif()

option(CRISPY_TRUECOLOR "True color rendering" ON)

# Check for libsamplerate.
find_package(SampleRate)
if(SampleRate_FOUND)
    set(HAVE_LIBSAMPLERATE TRUE)
endif()

# Check for Miniz (replaces libpng).
add_subdirectory("miniz")

# Check for FluidSynth.
find_package(FluidSynth)
if(FluidSynth_FOUND)
    set(HAVE_FLUIDSYNTH TRUE)
endif()

find_package(m)

include(CheckSymbolExists)
include(CheckIncludeFile)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)
check_include_file("dirent.h" HAVE_DIRENT_H)

string(CONCAT WINDOWS_RC_VERSION "${PROJECT_VERSION_MAJOR}, "
    "${PROJECT_VERSION_MINOR}, ${PROJECT_VERSION_PATCH}, 0")

# Without a hyphen. This is used for the bash-completion scripts.
string(TOLOWER "${PACKAGE_SHORTNAME}" PROGRAM_SPREFIX)

# With a hyphen, used almost everywhere else.
set(PROGRAM_PREFIX "${PROGRAM_SPREFIX}-")

configure_file(cmake/config.h.cin config.h)

configure_file(src/doom-res.rc.in src/doom-res.rc)
configure_file(src/heretic-res.rc.in src/heretic-res.rc)
configure_file(src/hexen-res.rc.in src/hexen-res.rc)
configure_file(src/setup-res.rc.in src/setup-res.rc)
configure_file(src/setup/setup-manifest.xml.in src/setup/setup-manifest.xml)

foreach(SUBDIR textscreen opl pcsound src)
    add_subdirectory("${SUBDIR}")
endforeach()
