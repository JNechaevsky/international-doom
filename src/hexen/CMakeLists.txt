add_library(hexen STATIC
            a_action.c
            am_map.c            am_map.h
            ct_chat.c
                                ct_chat.h
            d_net.c
            f_finale.c
            f_wipe.c            f_wipe.h
            g_game.c
                                h2def.h
            h2_main.c
            id_func.c           id_func.h
            info.c              info.h
            in_lude.c
            m_random.c          m_random.h
            mn_menu.c
            p_acs.c
                                p_action.h
            p_anim.c
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
            po_man.c
            p_plats.c
            p_pspr.c
            p_setup.c
            p_sight.c
            p_spec.c           p_spec.h
            p_switch.c
            p_telept.c
            p_things.c
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
            s_sound.c           s_sound.h
            sb_bar.c
            sc_man.c
            sn_sonix.c
            sounds.c            sounds.h
            st_start.c          st_start.h
            sv_save.c
                                textdefs.h
                                xddefs.h)

target_include_directories(hexen PRIVATE "../" "${CMAKE_CURRENT_BINARY_DIR}/../../")
target_link_libraries(hexen SDL2::SDL2)
if(ENABLE_SDL2_MIXER)
    target_link_libraries(hexen SDL2_mixer::SDL2_mixer)
endif()
if(ENABLE_SDL2_NET)
    target_link_libraries(hexen SDL2_net::SDL2_net)
endif()
