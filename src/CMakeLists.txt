foreach(SUBDIR doom heretic hexen setup)
    add_subdirectory("${SUBDIR}")
endforeach()

# Common source files used by absolutely everything:

set(COMMON_SOURCE_FILES
    i_main.c
    i_system.c           i_system.h
    id_vars.c            id_vars.h
    m_argv.c             m_argv.h
    m_misc.c             m_misc.h)

# Dedicated server (chocolate-server):

set(DEDSERV_FILES
    d_dedicated.c
    d_iwad.c            d_iwad.h
    d_mode.c            d_mode.h
    deh_str.c           deh_str.h
    i_timer.c           i_timer.h
    m_config.c          m_config.h
    net_common.c        net_common.h
    net_dedicated.c     net_dedicated.h
    net_io.c            net_io.h
    net_packet.c        net_packet.h
    net_sdl.c           net_sdl.h
    net_query.c         net_query.h
    net_server.c        net_server.h
    net_structrw.c      net_structrw.h
    z_native.c          z_zone.h)

# Source files used by the game binaries (chocolate-doom, etc.)

set(GAME_SOURCE_FILES
    aes_prng.c          aes_prng.h
    d_event.c           d_event.h
                        doomkeys.h
                        doomtype.h
    d_iwad.c            d_iwad.h
    d_loop.c            d_loop.h
    d_mode.c            d_mode.h
                        d_ticcmd.h
    deh_str.c           deh_str.h
    gusconf.c           gusconf.h
    i_endoom.c          i_endoom.h
    i_flmusic.c
    i_glob.c            i_glob.h
    i_input.c           i_input.h
    i_joystick.c        i_joystick.h
                        i_swap.h
    i_musicpack.c
    i_oplmusic.c
    i_pcsound.c
    i_sdlmusic.c
    i_sdlsound.c
    i_sound.c           i_sound.h
    i_timer.c           i_timer.h
    i_truecolor.c       i_truecolor.h
    i_video.c           i_video.h
    i_videohr.c         i_videohr.h
    midifile.c          midifile.h
    mus2mid.c           mus2mid.h
                        m_array.h
    m_bbox.c            m_bbox.h
    m_cheat.c           m_cheat.h
    m_config.c          m_config.h
    m_controls.c        m_controls.h
    m_fixed.c           m_fixed.h
    net_client.c        net_client.h
    net_common.c        net_common.h
    net_dedicated.c     net_dedicated.h
    net_defs.h
    net_gui.c           net_gui.h
    net_io.c            net_io.h
    net_loop.c          net_loop.h
    net_packet.c        net_packet.h
    net_petname.c       net_petname.h
    net_query.c         net_query.h
    net_sdl.c           net_sdl.h
    net_server.c        net_server.h
    net_structrw.c      net_structrw.h
    sha1.c              sha1.h
    memio.c             memio.h
    tables.c            tables.h
    v_diskicon.c        v_diskicon.h
    v_postproc.c        v_postproc.h
    v_video.c           v_video.h
                        v_patch.h
    v_trans.c           v_trans.h
    w_checksum.c        w_checksum.h
    w_main.c            w_main.h
    w_wad.c             w_wad.h
    w_file.c            w_file.h
    w_file_stdc.c
    w_merge.c           w_merge.h
    z_zone.c            z_zone.h)

# Platform-dependent source files:

set(GAME_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/../")

if(WIN32)
    list(APPEND GAME_SOURCE_FILES i_winmusic.c)
    list(APPEND GAME_SOURCE_FILES midifallback.c    midifallback.h)
    list(APPEND GAME_SOURCE_FILES w_file_win32.c)
    list(APPEND GAME_SOURCE_FILES
        "../win32/win_opendir.c" "../win32/win_opendir.h")
    list(APPEND GAME_INCLUDE_DIRS
         "${PROJECT_SOURCE_DIR}/win32/")
elseif(POSIX)
    list(APPEND GAME_SOURCE_FILES w_file_posix.c)
endif()

set(DEHACKED_SOURCE_FILES
    deh_defs.h
    deh_io.c            deh_io.h
    deh_main.c          deh_main.h
    deh_mapping.c       deh_mapping.h
    deh_text.c)

# Some games support dehacked patches, some don't:

set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${GAME_SOURCE_FILES})
set(SOURCE_FILES_WITH_DEH ${SOURCE_FILES} ${DEHACKED_SOURCE_FILES})

set(EXTRA_LIBS SDL2::SDL2main SDL2::SDL2 miniz::miniz textscreen pcsound opl)
if(ENABLE_SDL2_MIXER)
    list(APPEND EXTRA_LIBS SDL2_mixer::SDL2_mixer)
endif()
if(ENABLE_SDL2_NET)
    list(APPEND EXTRA_LIBS SDL2_net::SDL2_net)
endif()
if(SampleRate_FOUND)
    list(APPEND EXTRA_LIBS SampleRate::samplerate)
endif()
if(FluidSynth_FOUND)
    list(APPEND EXTRA_LIBS FluidSynth::libfluidsynth)
endif()
if(WIN32)
	list(APPEND EXTRA_LIBS winmm shlwapi)
endif()

if(WIN32)
    add_executable("${PROGRAM_PREFIX}doom" WIN32 ${SOURCE_FILES_WITH_DEH} "${CMAKE_CURRENT_BINARY_DIR}/doom-res.rc")
else()
    add_executable("${PROGRAM_PREFIX}doom" ${SOURCE_FILES_WITH_DEH})
endif()

target_include_directories("${PROGRAM_PREFIX}doom" PRIVATE ${GAME_INCLUDE_DIRS})
target_link_libraries("${PROGRAM_PREFIX}doom" doom ${EXTRA_LIBS})

if(MSVC)
    set_target_properties("${PROGRAM_PREFIX}doom" PROPERTIES
                          LINK_FLAGS "/MANIFEST:NO /LTCG")
endif()

if(WIN32)
    add_executable("${PROGRAM_PREFIX}heretic" WIN32 ${SOURCE_FILES_WITH_DEH} "${CMAKE_CURRENT_BINARY_DIR}/heretic-res.rc")
else()
    add_executable("${PROGRAM_PREFIX}heretic" ${SOURCE_FILES_WITH_DEH})
endif()

target_include_directories("${PROGRAM_PREFIX}heretic" PRIVATE ${GAME_INCLUDE_DIRS})
target_link_libraries("${PROGRAM_PREFIX}heretic" heretic ${EXTRA_LIBS})

if(MSVC)
    set_target_properties("${PROGRAM_PREFIX}heretic" PROPERTIES
                          LINK_FLAGS "/MANIFEST:NO /LTCG")
endif()

if(WIN32)
    add_executable("${PROGRAM_PREFIX}hexen" WIN32 ${SOURCE_FILES} "${CMAKE_CURRENT_BINARY_DIR}/hexen-res.rc")
else()
    add_executable("${PROGRAM_PREFIX}hexen" ${SOURCE_FILES})
endif()

target_include_directories("${PROGRAM_PREFIX}hexen" PRIVATE ${GAME_INCLUDE_DIRS})
target_link_libraries("${PROGRAM_PREFIX}hexen" hexen ${EXTRA_LIBS})

if(MSVC)
    set_target_properties("${PROGRAM_PREFIX}hexen" PROPERTIES
                          LINK_FLAGS "/MANIFEST:NO /LTCG")
endif()

set_target_properties("${PROGRAM_PREFIX}doom" PROPERTIES
                      INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})
set_target_properties("${PROGRAM_PREFIX}heretic" PROPERTIES
                      INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})
set_target_properties("${PROGRAM_PREFIX}hexen" PROPERTIES
                      INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})

# Source files needed for chocolate-setup:

set(SETUP_FILES
    deh_str.c           deh_str.h
    d_mode.c            d_mode.h
    d_iwad.c            d_iwad.h
    i_timer.c           i_timer.h
    m_config.c          m_config.h
    m_controls.c        m_controls.h
    net_io.c            net_io.h
    net_packet.c        net_packet.h
    net_petname.c       net_petname.h
    net_sdl.c           net_sdl.h
    net_query.c         net_query.h
    net_structrw.c      net_structrw.h
    z_native.c          z_zone.h)

if(WIN32)
    add_executable("${PROGRAM_PREFIX}setup" WIN32 ${SETUP_FILES} ${COMMON_SOURCE_FILES} "${CMAKE_CURRENT_BINARY_DIR}/setup-res.rc")
else()
    add_executable("${PROGRAM_PREFIX}setup" ${SETUP_FILES} ${COMMON_SOURCE_FILES})
endif()

target_include_directories("${PROGRAM_PREFIX}setup"
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
target_link_libraries("${PROGRAM_PREFIX}setup" SDL2::SDL2main SDL2::SDL2 setup textscreen)
if(ENABLE_SDL2_MIXER)
    target_link_libraries("${PROGRAM_PREFIX}setup" SDL2_mixer::SDL2_mixer)
endif()
if(ENABLE_SDL2_NET)
    target_link_libraries("${PROGRAM_PREFIX}setup" SDL2_net::SDL2_net)
endif()
if(WIN32)
    target_link_libraries("${PROGRAM_PREFIX}setup" winmm)
endif()

if(MSVC)
    set_target_properties("${PROGRAM_PREFIX}setup" PROPERTIES
                          LINK_FLAGS "/MANIFEST:NO")
endif()

set_target_properties("${PROGRAM_PREFIX}setup" PROPERTIES
                      INTERPROCEDURAL_OPTIMIZATION ${ENABLE_LTO})

include(GNUInstallDirs)

install(TARGETS "${PROGRAM_PREFIX}doom" "${PROGRAM_PREFIX}heretic" "${PROGRAM_PREFIX}hexen" "${PROGRAM_PREFIX}setup" RUNTIME
    DESTINATION $<IF:$<BOOL:${WIN32}>,.,${CMAKE_INSTALL_BINDIR}>
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_WRITE WORLD_EXECUTE)
