add_library(heretic STATIC
            am_map.c            am_map.h
            ct_chat.c           ct_chat.h
            deh_ammo.c
            deh_frame.c
            deh_htext.c
            deh_htic.c          deh_htic.h
            deh_sound.c
            deh_thing.c
            deh_weapon.c
            d_main.c
            d_net.c
                                doomdata.h
                                doomdef.h
                                dstrings.h
            f_finale.c
            f_wipe.c            f_wipe.h
            g_game.c
            id_func.c           id_func.h
            info.c              info.h
            in_lude.c
            m_random.c          m_random.h
            mn_menu.c
                                p_action.h
            p_ceilng.c
            p_doors.c
            p_enemy.c
            p_floor.c
            p_inter.c
            p_lights.c
            p_local.h
            p_map.c
            p_maputl.c
            p_mobj.c
            p_plats.c
            p_pspr.c
            p_saveg.c
            p_setup.c
            p_sight.c
            p_spec.c            p_spec.h
            p_switch.c
            p_telept.c
            p_tick.c
            p_user.c
            r_bmaps.c
            r_bsp.c
            r_data.c
            r_draw.c
                                r_local.h
            r_main.c
            r_plane.c
            r_segs.c
            r_swirl.c
            r_things.c
            sb_bar.c            sb_bar.h
            sounds.c            sounds.h
            s_sound.c           s_sound.h)

target_include_directories(heretic PRIVATE "../" "${CMAKE_CURRENT_BINARY_DIR}/../../")
target_link_libraries(heretic textscreen SDL2::SDL2 miniz::miniz)
if(ENABLE_SDL2_MIXER)
    target_link_libraries(heretic SDL2_mixer::SDL2_mixer)
endif()
if(ENABLE_SDL2_NET)
    target_link_libraries(heretic SDL2_net::SDL2_net)
endif()
