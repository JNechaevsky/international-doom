add_library(setup STATIC
            ../id_vars.c        ../id_vars.h
            mainmenu.c
            mode.c              mode.h
            multiplayer.c       multiplayer.h
            execute.c           execute.h)

target_include_directories(setup PRIVATE "../" "${CMAKE_CURRENT_BINARY_DIR}/../../")
target_link_libraries(setup textscreen SDL3::SDL3)
if(ENABLE_SDL3_MIXER)
    target_link_libraries(setup SDL3_mixer::SDL3_mixer)
endif()
