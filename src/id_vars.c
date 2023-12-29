//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2015-2018 Fabian Greffrath
// Copyright(C) 2016-2024 Julia Nechaevskaya
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//


#include "m_config.h"  // [JN] M_BindIntVariable


// -----------------------------------------------------------------------------
// [JN] ID-specific config variables.
// -----------------------------------------------------------------------------

// System and video
#ifdef CRISPY_TRUECOLOR
int vid_truecolor = 0;
#endif
int vid_resolution = 2;
int vid_widescreen = 0;

int vid_startup_delay = 35;
int vid_resize_delay = 35;
int vid_uncapped_fps = 0;
int vid_fpslimit = 60;
int vid_vsync = 1;
int vid_showfps = 0;
int vid_gamma = 10;
int vid_fov = 90;
int vid_saturation = 100;
float vid_r_intensity = 1.000000;
float vid_g_intensity = 1.000000;
float vid_b_intensity = 1.000000;
int dp_screen_size = 10;
int vid_screenwipe = 1;
int msg_text_shadows = 0;

// Display
int dp_menu_shading = 0;
int dp_level_brightness = 0;

// Messages
int msg_alignment = 0;
int msg_local_time = 0;

// Game modes
int crl_spectating = 0;
int crl_freeze = 0;

// Widgets
int widget_location = 0;
int widget_coords = 0;
int widget_render = 0;
int widget_kis = 0;
int widget_time = 0;
int widget_totaltime = 0;
int widget_levelname = 0;
int widget_health = 0;

// Sound
int snd_monosfx = 0;
int snd_mute_inactive = 0;

// Automap
int automap_scheme = 0;
int automap_smooth = 0;
int automap_secrets = 0;
int automap_rotate = 0;
int automap_overlay = 0;
int automap_shading = 0;

// Gameplay features
int vis_brightmaps = 0;
int vis_translucency = 0;
int vis_fake_contrast = 1;
int vis_smooth_light = 0;
int vis_improved_fuzz = 0;
int vis_colored_blood = 0;
int vis_swirling_liquids = 0;
int vis_invul_sky = 0;
int vis_linear_sky = 0;
int vis_flip_corpses = 0;

int xhair_draw = 0;
int xhair_color = 0;

int st_colored_stbar = 0;
int st_negative_health = 0;
int st_blinking_keys = 0;

int aud_z_axis_sfx = 0;
int aud_full_sounds = 0;
int aud_exit_sounds = 0;

int phys_torque = 0;
int phys_ssg_tear_monsters = 0;
int phys_toss_drop = 0;
int phys_floating_powerups = 0;
int phys_weapon_alignment = 0;

int gp_default_skill = 2;
int gp_revealed_secrets = 0;
int phys_breathing = 0;
int gp_flip_levels = 0;

// Demos
int demo_timer = 0;
int demo_timerdir = 0;
int demo_bar = 0;
int demo_internal = 1;

// Compatibility-breaking
int compat_pistol_start = 0;
int compat_blockmap_fix = 0;
int compat_vertical_aiming = 0;

// Mouse look
int mouse_look = 0;

// -----------------------------------------------------------------------------
// [JN] ID-specific config variables binding function.
// -----------------------------------------------------------------------------

void ID_BindVariables (void)
{
    // System and video
#ifdef CRISPY_TRUECOLOR
    M_BindIntVariable("vid_truecolor",                  &vid_truecolor);
#endif
    M_BindIntVariable("vid_resolution",                 &vid_resolution);
    M_BindIntVariable("vid_widescreen",                 &vid_widescreen);
    
    M_BindIntVariable("vid_startup_delay",              &vid_startup_delay);
    M_BindIntVariable("vid_resize_delay",               &vid_resize_delay);
    M_BindIntVariable("vid_uncapped_fps",               &vid_uncapped_fps);
    M_BindIntVariable("vid_fpslimit",                   &vid_fpslimit);
    M_BindIntVariable("vid_vsync",                      &vid_vsync);
    M_BindIntVariable("vid_showfps",                    &vid_showfps);
    M_BindIntVariable("vid_gamma",                      &vid_gamma);
    M_BindIntVariable("vid_fov",                        &vid_fov);
    M_BindIntVariable("vid_saturation",                 &vid_saturation);
    M_BindFloatVariable("vid_r_intensity",              &vid_r_intensity);
    M_BindFloatVariable("vid_g_intensity",              &vid_g_intensity);
    M_BindFloatVariable("vid_b_intensity",              &vid_b_intensity);
    M_BindIntVariable("dp_screen_size",                 &dp_screen_size);
    M_BindIntVariable("vid_screenwipe",                 &vid_screenwipe);
    M_BindIntVariable("msg_text_shadows",               &msg_text_shadows);

    // Display
    M_BindIntVariable("dp_menu_shading",                &dp_menu_shading);
    M_BindIntVariable("dp_level_brightness",            &dp_level_brightness);

    // Messages
    M_BindIntVariable("msg_alignment",                  &msg_alignment);
    M_BindIntVariable("msg_local_time",                 &msg_local_time);

    // Widgets
    M_BindIntVariable("widget_location",                &widget_location);
    M_BindIntVariable("widget_coords",                  &widget_coords);
    M_BindIntVariable("widget_render",                  &widget_render);
    M_BindIntVariable("widget_kis",                     &widget_kis);
    M_BindIntVariable("widget_time",                    &widget_time);
    M_BindIntVariable("widget_totaltime",               &widget_totaltime);
    M_BindIntVariable("widget_levelname",               &widget_levelname);
    M_BindIntVariable("widget_health",                  &widget_health);

    // Sound
    M_BindIntVariable("snd_monosfx",                    &snd_monosfx);
    M_BindIntVariable("snd_mute_inactive",              &snd_mute_inactive);

    // Automap
    M_BindIntVariable("automap_scheme",                 &automap_scheme);
    M_BindIntVariable("automap_smooth",                 &automap_smooth);
    M_BindIntVariable("automap_secrets",                &automap_secrets);
    M_BindIntVariable("automap_rotate",                 &automap_rotate);
    M_BindIntVariable("automap_overlay",                &automap_overlay);
    M_BindIntVariable("automap_shading",                &automap_shading);

    // Gameplay features
    M_BindIntVariable("vis_brightmaps",                 &vis_brightmaps);
    M_BindIntVariable("vis_translucency",               &vis_translucency);
    M_BindIntVariable("vis_fake_contrast",              &vis_fake_contrast);
    M_BindIntVariable("vis_smooth_light",               &vis_smooth_light);
    M_BindIntVariable("vis_improved_fuzz",              &vis_improved_fuzz);
    M_BindIntVariable("vis_colored_blood",              &vis_colored_blood);
    M_BindIntVariable("vis_swirling_liquids",           &vis_swirling_liquids);
    M_BindIntVariable("vis_invul_sky",                  &vis_invul_sky);
    M_BindIntVariable("vis_linear_sky",                 &vis_linear_sky);
    M_BindIntVariable("vis_flip_corpses",               &vis_flip_corpses);

    M_BindIntVariable("xhair_draw",                     &xhair_draw);
    M_BindIntVariable("xhair_color",                    &xhair_color);

    M_BindIntVariable("st_colored_stbar",               &st_colored_stbar);
    M_BindIntVariable("st_negative_health",             &st_negative_health);
    M_BindIntVariable("st_blinking_keys",               &st_blinking_keys);

    M_BindIntVariable("aud_z_axis_sfx",                 &aud_z_axis_sfx);
    M_BindIntVariable("aud_full_sounds",                &aud_full_sounds);
    M_BindIntVariable("aud_exit_sounds",                &aud_exit_sounds);

    M_BindIntVariable("phys_torque",                    &phys_torque);
    M_BindIntVariable("phys_ssg_tear_monsters",         &phys_ssg_tear_monsters);
    M_BindIntVariable("phys_toss_drop",                 &phys_toss_drop);
    M_BindIntVariable("phys_floating_powerups",         &phys_floating_powerups);
    M_BindIntVariable("phys_weapon_alignment",          &phys_weapon_alignment);

    M_BindIntVariable("gp_default_skill",               &gp_default_skill);
    M_BindIntVariable("gp_revealed_secrets",            &gp_revealed_secrets);
    M_BindIntVariable("phys_breathing",                 &phys_breathing);
    M_BindIntVariable("gp_flip_levels",                 &gp_flip_levels);

    // Demos
    M_BindIntVariable("demo_timer",                     &demo_timer);
    M_BindIntVariable("demo_timerdir",                  &demo_timerdir);
    M_BindIntVariable("demo_bar",                       &demo_bar);
    M_BindIntVariable("demo_internal",                  &demo_internal);

    // Compatibility-breaking
    M_BindIntVariable("compat_pistol_start",            &compat_pistol_start);
    M_BindIntVariable("compat_blockmap_fix",            &compat_blockmap_fix);
    M_BindIntVariable("compat_vertical_aiming",         &compat_vertical_aiming);

    // Mouse look
    M_BindIntVariable("mouse_look",                     &mouse_look);
}
