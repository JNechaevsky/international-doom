add_library(netgame STATIC
            ../id_vars.c        ../id_vars.h
            mainmenu.c
            mode.c              mode.h
            multiplayer.c       multiplayer.h
            execute.c           execute.h)

target_include_directories(netgame PRIVATE "../" "${CMAKE_CURRENT_BINARY_DIR}/../../")
target_link_libraries(netgame textscreen SDL2::SDL2)
if(ENABLE_SDL2_MIXER)
    target_link_libraries(netgame SDL2_mixer::SDL2_mixer)
endif()
