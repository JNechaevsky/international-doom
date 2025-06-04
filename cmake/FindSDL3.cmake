# FindSDL3.cmake
#
# Copyright (c) 2018, Alex Mayfield <alexmax2742@gmail.com>
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <organization> nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Currently works with the following generators:
# - Unix Makefiles (Linux, MSYS2)
# - Ninja (Linux, MSYS2)
# - Visual Studio

# Cache variable that allows you to point CMake at a directory containing
# an extracted development library.
set(SDL3_DIR "${SDL3_DIR}" CACHE PATH "Location of SDL3 library directory")

# Use pkg-config to find library locations in *NIX environments.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_search_module(PC_SDL3 QUIET sdl3)
endif()

# Find the include directory.
find_path(SDL3_INCLUDE_DIR "SDL3/SDL.h"
    HINTS "${SDL3_DIR}/include" ${PC_SDL3_INCLUDE_DIRS})

# Find the version.  Taken and modified from CMake's FindSDL.cmake.
if(SDL3_INCLUDE_DIR AND EXISTS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h")
    file(STRINGS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h" SDL3_VERSION_MAJOR_LINE REGEX "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h" SDL3_VERSION_MINOR_LINE REGEX "^#define[ \t]+SDL_MINOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h" SDL3_VERSION_PATCH_LINE REGEX "^#define[ \t]+SDL_PATCHLEVEL[ \t]+[0-9]+$")
    string(REGEX REPLACE "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL3_VERSION_MAJOR "${SDL3_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_MINOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL3_VERSION_MINOR "${SDL3_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_PATCHLEVEL[ \t]+([0-9]+)$" "\\1" SDL3_VERSION_PATCH "${SDL3_VERSION_PATCH_LINE}")
    set(SDL3_VERSION "${SDL3_VERSION_MAJOR}.${SDL3_VERSION_MINOR}.${SDL3_VERSION_PATCH}")
    unset(SDL3_VERSION_MAJOR_LINE)
    unset(SDL3_VERSION_MINOR_LINE)
    unset(SDL3_VERSION_PATCH_LINE)
    unset(SDL3_VERSION_MAJOR)
    unset(SDL3_VERSION_MINOR)
    unset(SDL3_VERSION_PATCH)
endif()

# Find the SDL3 and SDL3main libraries
if(CMAKE_SIZEOF_VOID_P STREQUAL 8)
    find_library(SDL3_LIBRARY "SDL3"
        HINTS "${SDL3_DIR}/lib/x64" ${PC_SDL3_LIBRARY_DIRS})
    find_library(SDL3_MAIN_LIBRARY "SDL3main"
        HINTS "${SDL3_DIR}/lib/x64" ${PC_SDL3_LIBRARY_DIRS})
else()
    find_library(SDL3_LIBRARY "SDL3"
        HINTS "${SDL3_DIR}/lib/x86" ${PC_SDL3_LIBRARY_DIRS})
    find_library(SDL3_MAIN_LIBRARY "SDL3main"
        HINTS "${SDL3_DIR}/lib/x86" ${PC_SDL3_LIBRARY_DIRS})
endif()
set(SDL3_LIBRARIES "${SDL3_MAIN_LIBRARY}" "${SDL3_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL3
    FOUND_VAR SDL3_FOUND
    REQUIRED_VARS SDL3_INCLUDE_DIR SDL3_LIBRARIES
    VERSION_VAR SDL3_VERSION
)

if(SDL3_FOUND)
    # SDL3 imported target.
    add_library(SDL3::SDL3 UNKNOWN IMPORTED)
    set_target_properties(SDL3::SDL3 PROPERTIES
                          INTERFACE_COMPILE_OPTIONS "${PC_SDL3_CFLAGS_OTHER}"
                          INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIR}"
                          IMPORTED_LOCATION "${SDL3_LIBRARY}")

    # SDL3main imported target.
    if(MINGW)
        # Gross hack to get mingw32 first in the linker order.
        add_library(SDL3::_SDL3main_detail UNKNOWN IMPORTED)
        set_target_properties(SDL3::_SDL3main_detail PROPERTIES
                              IMPORTED_LOCATION "${SDL3_MAIN_LIBRARY}")

        # Ensure that SDL3main comes before SDL3 in the linker order.  CMake
        # isn't smart enough to keep proper ordering for indirect dependencies
        # so we have to spell it out here.
        target_link_libraries(SDL3::_SDL3main_detail INTERFACE SDL3::SDL3)

        add_library(SDL3::SDL3main INTERFACE IMPORTED)
        set_target_properties(SDL3::SDL3main PROPERTIES
                              IMPORTED_LIBNAME mingw32)
        target_link_libraries(SDL3::SDL3main INTERFACE SDL3::_SDL3main_detail)
    else()
        add_library(SDL3::SDL3main UNKNOWN IMPORTED)
        set_target_properties(SDL3::SDL3main PROPERTIES
                              IMPORTED_LOCATION "${SDL3_MAIN_LIBRARY}")
    endif()
endif()
