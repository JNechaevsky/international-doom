add_library(pcsound STATIC
            pcsound.c       pcsound.h
            pcsound_sdl.c
                            pcsound_internal.h)
target_include_directories(pcsound
                           INTERFACE "."
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../" "../src/")
target_link_libraries(pcsound SDL3::SDL3)
if(ENABLE_SDL3_MIXER)
    target_link_libraries(pcsound SDL3_mixer::SDL3_mixer)
endif()
