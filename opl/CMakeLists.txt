add_library(opl STATIC
            opl.c           opl.h
                            opl_internal.h
            opl_queue.c     opl_queue.h
            opl_sdl.c
            opl3.c          opl3.h)
target_include_directories(opl
                           INTERFACE "."
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../" "../src/")
target_link_libraries(opl SDL2::SDL2)
if(ENABLE_SDL2_MIXER)
    target_link_libraries(opl SDL2_mixer::SDL2_mixer)
endif()
