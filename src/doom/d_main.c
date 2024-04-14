//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//	plus functions to determine game mode (shareware, registered),
//	parse command line parameters, configure game parameters (turbo),
//	and call the startup functions.
//


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "deh_main.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "dstrings.h"
#include "d_iwad.h"
#include "z_zone.h"
#include "w_main.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "p_local.h"
#include "i_endoom.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "g_game.h"
#include "wi_stuff.h"
#include "st_bar.h"
#include "am_map.h"
#include "net_client.h"
#include "net_dedicated.h"
#include "net_query.h"
#include "statdump.h"
#include "d_main.h"
#include "ct_chat.h"
#include "r_local.h"
#include "v_trans.h"

#include "icon.c"

#include "id_vars.h"
#include "id_func.h"



static char *gamedescription;

// Location where savegames are stored
char *savegamedir;

// location of IWAD and WAD files
static char *iwadfile;

boolean devparm;      // started game with -devparm
boolean nomonsters;   // checkparm of -nomonsters
boolean respawnparm;  // checkparm of -respawn
boolean fastparm;     // checkparm of -fast

int     startepisode;
int     startmap;
int     startloadgame;
skill_t startskill;
boolean autostart;

boolean advancedemo;
static boolean storedemo;  // Store demo, do not accept any inputs

static int   demosequence;
static int   pagetic;
static const char *pagename;
static char *SavePathConfig;  // [JN] "savedir" config file variable.

// If true, the main game loop has started.
boolean main_loop_started = false;

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;

// [JN] Semi-official addons.
// SIGIL
static boolean sigil_compat = false;
boolean sigil = false;
// SIGIL 2
boolean sigil2 = false;
// NERVE
boolean nerve = false;

// [JN] Check for available SSG from Crispy Doom.
boolean havessg = false;


// -----------------------------------------------------------------------------
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
// -----------------------------------------------------------------------------

void D_ProcessEvents (void)
{
    event_t *ev;

    // IF STORE DEMO, DO NOT ACCEPT INPUT
    if (storedemo)
    {
        return;
    }

    while ((ev = D_PopEvent()) != NULL)
    {
        if (M_Responder (ev))
        {
            continue;  // menu ate the event
        }
        G_Responder (ev);
    }
}

// -----------------------------------------------------------------------------
// ID_DrawMessage
// [JN] Draws message on the screen.
// -----------------------------------------------------------------------------

static void ID_DrawMessage (void)
{
    player_t *player = &players[displayplayer];

    if (player->messageTics <= 0 || !player->message)
    {
        return;  // No message
    }

    // Draw message, where?
    if (msg_alignment == 0)
    {
        // Left
        M_WriteText(0 - WIDESCREENDELTA, 0, player->message, player->messageColor);
    }
    else
    if (msg_alignment == 1)
    {
        // Status bar
        M_WriteText(0, 0, player->message, player->messageColor);
    }
    else
    {
        // Centered
        M_WriteTextCentered(0, player->message, player->messageColor);
    }
}

// -----------------------------------------------------------------------------
// ID_DrawMessage
// [JN] Draws message on the screen.
// -----------------------------------------------------------------------------

static void ID_DrawMessageCentered (void)
{
    player_t *player = &players[displayplayer];

    if (player->messageCenteredTics <= 0 || !player->messageCentered)
    {
        return;  // No message
    }

    // Always centered
    M_WriteTextCentered(63, player->messageCentered, player->messageCenteredColor);
}

// -----------------------------------------------------------------------------
// D_Display
//  draw current display, possibly wiping it from the previous
// -----------------------------------------------------------------------------

static void D_Display (void)
{
    int      nowtime;
    int      tics;
    int      wipestart;
    int      y;
    boolean  done;
    boolean  wipe;
    static   gamestate_t oldgamestate = -1;

    if (nodrawers)
    {
        return;  // for comparative timing / profiling
    }

    // [crispy] post-rendering function pointer to apply config changes
    // that affect rendering and that are better applied after the current
    // frame has finished rendering
    if (post_rendering_hook)
    {
        post_rendering_hook();
        post_rendering_hook = NULL;
    }

    // change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize();
        oldgamestate = -1;  // force background redraw
    }

    // save the current screen if about to wipe
    // [JN] Make screen wipe optional, use external config variable.
    if (gamestate != wipegamestate && vid_screenwipe)
    {
        wipe = true;
        wipe_StartScreen();
    }
    else
    {
        wipe = false;
    }

    // do buffered drawing
    switch (gamestate)
    {
        case GS_LEVEL:
        break;

        case GS_INTERMISSION:
        WI_Drawer();
        break;

        case GS_FINALE:
        F_Drawer();
        break;

        case GS_DEMOSCREEN:
        D_PageDrawer();
        break;
    }

    // draw the view directly
    if (gamestate == GS_LEVEL && gametic)
    {
        R_RenderPlayerView(&players[displayplayer]);
    }

    // [JN] Fail-safe: return earlier if post rendering hook is still active.
    if (post_rendering_hook)
    {
        return;
    }

    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
#ifndef CRISPY_TRUECOLOR
	I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));
#else
	I_SetPalette (0);
#endif

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
        R_FillBackScreen();  // draw the pattern into the back screen
    }

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && scaledviewwidth != SCREENWIDTH)
    {
        R_DrawViewBorder();  // erase old menu stuff
    }

    // [JN] Draw automap on top of player view and view border,
    // and update while playing. This also needed for widgets update.
    if (automapactive)
    {
        AM_Drawer();
    }

    // [JN] Allow to draw level name separately from automap.
    if (gamestate == GS_LEVEL && widget_levelname)
    {
        AM_LevelNameDrawer();
    }

    if (testcontrols)
    {
        // Box showing current mouse speed
        V_DrawMouseSpeedBox(testcontrols_mousespeed);
    }

    oldgamestate = wipegamestate = gamestate;

    // draw pause pic
    if (paused)
    {
	if (automapactive && !automap_overlay)
	    y = 4;
	else
	    y = (viewwindowy / vid_resolution)+4;
	V_DrawShadowedPatchOptional((viewwindowx / vid_resolution) + ((scaledviewwidth / vid_resolution) - 68) / 2 - WIDESCREENDELTA, y, 0,
                          W_CacheLumpName (DEH_String("M_PAUSE"), PU_CACHE));
    }

    // [JN] Do not draw any widgets if not in game level.
    if (gamestate == GS_LEVEL)
    {
        // [JN] Left widgets are available while active game level.
        if (dp_screen_size < 15)
        {
            ID_LeftWidgets();
        }

        // [crispy] demo timer widget
        if (demoplayback && (demo_timer == 1 || demo_timer == 3))
        {
            ID_DemoTimer(demo_timerdir ? (deftotaldemotics - defdemotics) : defdemotics);
        }
        else if (demorecording && (demo_timer == 2 || demo_timer == 3))
        {
            ID_DemoTimer(leveltime);
        }

        // [JN] Target's health widget.
        // Actual health values are gathered in G_Ticker.
        if (widget_health)
        {
            ID_DrawTargetsHealth();
        }

        // [JN] Draw crosshair.
        if (xhair_draw && !automapactive)
        {
            ID_DrawCrosshair();
        }

        // [JN] Main status bar drawing function.
        if (dp_screen_size < 15 || (automapactive && !automap_overlay))
        {
            // [JN] Only forcefully update/redraw on...
            const boolean st_forceredraw = 
                             (oldgametic < gametic  // Every game tic
                          ||  dp_screen_size > 10   // Crispy HUD (no solid status bar background)
                          ||  setsizeneeded         // Screen size changing
                          || (menuactive && dp_menu_shading)); // Menu shading while non-capped game mode

            ST_Drawer(st_forceredraw);
        }

        // [JN] Chat drawer
        if (netgame && chatmodeon)
        {
            CT_Drawer();
        }
    }

    // [JN] Draw right widgets in any states except finale text screens.
    if (gamestate != GS_FINALE)
    {
        ID_RightWidgets();
    }

    // [crispy] Demo Timer widget
    if (demoplayback && demo_bar)
    {
        ID_DemoBar();
    }

    // Handle player messages
    ID_DrawMessage();

    // [JN] Handle centered player messages.
    ID_DrawMessageCentered();

    // menus go directly to the screen
    M_Drawer ();   // menu is drawn even on top of everything
    NetUpdate ();  // send out any new accumulation

    // normal update
    if (!wipe)
    {
        I_FinishUpdate();  // page flip or blit buffer
        return;
    }

    // wipe update
    wipe_EndScreen();
    wipestart = I_GetTime () - 1;

    do
    {
        do
        {
            nowtime = I_GetTime ();
            tics = nowtime - wipestart;
            I_Sleep(1);
        } while (tics <= 0);

        wipestart = nowtime;
        done = wipe_ScreenWipe(tics);
        M_Drawer();        // menu is drawn even on top of wipes
        I_FinishUpdate();  // page flip or blit buffer
        } while (!done);
}

//
// Add configuration file variable bindings.
//

void D_BindVariables(void)
{
    int i;

    I_BindInputVariables();
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindControls();
    M_BindChatControls(MAXPLAYERS);

    key_multi_msgplayer[0] = HUSTR_KEYGREEN;
    key_multi_msgplayer[1] = HUSTR_KEYINDIGO;
    key_multi_msgplayer[2] = HUSTR_KEYBROWN;
    key_multi_msgplayer[3] = HUSTR_KEYRED;

    NET_BindVariables();

    // [JN] Game-dependent variables:
    M_BindIntVariable("key_message_refresh",    &key_message_refresh);
    M_BindIntVariable("sfx_volume",             &sfxVolume);
    M_BindIntVariable("music_volume",           &musicVolume);

    M_BindStringVariable("savegames_path",      &SavePathConfig);
    M_BindStringVariable("screenshots_path",    &ShotPathConfig);

    // Multiplayer chat macros

    for (i=0; i<10; ++i)
    {
        char buf[12];

        M_snprintf(buf, sizeof(buf), "chatmacro%i", i);
        M_BindStringVariable(buf, &chat_macros[i]);
    }

	// [JN] Bind ID-specific config variables.
	ID_BindVariables(doom);
}

// -----------------------------------------------------------------------------
// D_SetDefaultSavePath
// [JN] Set the default directory where savegames are saved.
// -----------------------------------------------------------------------------

static void D_SetDefaultSavePath (void)
{
    // Gather default "savegames" folder location.
    savegamedir = M_GetSaveGameDir(D_SaveGameIWADName(gamemission, gamevariant));

    if (!strcmp(savegamedir, ""))
    {
        if (SavePathConfig == NULL || !strcmp(SavePathConfig, ""))
        {
            // Config file variable not existing or emptry,
            // so use generated path from above.
            savegamedir = M_StringDuplicate("");
        }
        else
        {
            // Variable existing, use it's path.
            savegamedir = M_StringDuplicate(SavePathConfig);
        }
    }

    // Overwrite config file variable only if following command line
    // parameters are not present:
    if (!M_ParmExists("-savedir") && !M_ParmExists("-cdrom"))
    {
        SavePathConfig = savegamedir;
    }
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

boolean D_GrabMouseCallback(void)
{
    // Drone players don't need mouse focus

    if (drone)
        return false;

    // [JN] CRL - always grab mouse in spectator mode.
    // It's supposed to be controlled by hand, even while pause.
    // However, do not grab mouse while active game menu.

    if (crl_spectating)
        return menuactive ? false : true;

    // when menu is active or game is paused, release the mouse 
 
    if (menuactive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL) && ((!demoplayback && !advancedemo));
}

//
//  D_DoomLoop
//
void D_DoomLoop (void)
{
    if (gamevariant == bfgedition &&
        (demorecording || (gameaction == ga_playdemo) || netgame))
    {
        printf(" WARNING: You are playing using one of the Doom Classic\n"
               " IWAD files shipped with the Doom 3: BFG Edition. These are\n"
               " known to be incompatible with the regular IWAD files and\n"
               " may cause demos and network games to get out of sync.\n");
    }

    if (demorecording)
	G_BeginRecording ();

    main_loop_started = true;

    I_SetWindowTitle(gamedescription);
    I_GraphicsCheckCommandLine();
    I_SetGrabMouseCallback(D_GrabMouseCallback);
    I_RegisterWindowIcon(doom_data, doom_w, doom_h);
    I_InitGraphics();
    V_EnableLoadingDisk();
    // [JN] Calculate status bar elements background buffers.
    ST_InitElementsBackground();

    TryRunTics();

    V_RestoreBuffer();
    R_ExecuteSetViewSize();

    D_StartGameLoop();

    if (testcontrols)
    {
        wipegamestate = gamestate;
    }

    while (1)
    {
        // frame syncronous IO operations
        I_StartFrame ();

        // will run at least one tic
        TryRunTics ();

        // Update display, next frame, with current state.
        if (screenvisible)
        {
            D_Display();
        }

        // move positional sounds
        if (oldgametic < gametic)
        {
            // [JN] Mute and restore sound and music volume.
            if (snd_mute_inactive && volume_needs_update)
            {
                S_MuteUnmuteSound(!window_focused);
            }

            S_UpdateSounds (players[displayplayer].mo);
            oldgametic = gametic;
        }
    }
}


// =============================================================================
// DEMO LOOP
// =============================================================================


// -----------------------------------------------------------------------------
// D_PageTicker
// Handles timing for warped projection
// -----------------------------------------------------------------------------

void D_PageTicker (void)
{
    if (--pagetic < 0)
    {
        D_AdvanceDemo();
    }
}

// -----------------------------------------------------------------------------
// D_PageDrawer
// -----------------------------------------------------------------------------

void D_PageDrawer (void)
{
    V_DrawPatchFullScreen(W_CacheLumpName(pagename, PU_CACHE), false);
}

// -----------------------------------------------------------------------------
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
// -----------------------------------------------------------------------------

void D_AdvanceDemo (void)
{
    advancedemo = true;
}

// -----------------------------------------------------------------------------
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
// -----------------------------------------------------------------------------

void D_DoAdvanceDemo (void)
{
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    // The Ultimate Doom executable changed the demo sequence to add
    // a DEMO4 demo.  Final Doom was based on Ultimate, so also
    // includes this change; however, the Final Doom IWADs do not
    // include a DEMO4 lump, so the game bombs out with an error
    // when it reaches this point in the demo sequence.

    // However! There is an alternate version of Final Doom that
    // includes a fixed executable.

    // [JN] Play DEMO4 only in Ultimate Doom.
    demosequence = (demosequence + 1) % (gameversion == exe_ultimate ? 7 : 6);
    
    switch (demosequence)
    {
        case 0:
        if (gamemode == commercial)
        {
            pagetic = TICRATE * 11;
            S_StartMusic(mus_dm2ttl);
        }
        else
        {
            pagetic = 170;
            S_StartMusic (mus_intro);
        }
        gamestate = GS_DEMOSCREEN;
        pagename = DEH_String("TITLEPIC");
        break;

        case 1:
        if (demo_internal)
        {
            G_DeferedPlayDemo(DEH_String("demo1"));
        }
        break;

        case 2:
        pagetic = 200;
        gamestate = GS_DEMOSCREEN;
        pagename = DEH_String("CREDIT");
        break;
      
        case 3:
        if (demo_internal && !nerve)
        {
            G_DeferedPlayDemo(DEH_String("demo2"));
        }
        break;

        case 4:
        gamestate = GS_DEMOSCREEN;
        if (gamemode == commercial)
        {
            pagetic = TICRATE * 11;
            pagename = DEH_String("TITLEPIC");
            S_StartMusic(mus_dm2ttl);
        }
        else
        {
            pagetic = 200;
            pagename = DEH_String(gameversion >= exe_ultimate ? "CREDIT" : "HELP2");
        }
        break;

        case 5:
        if (demo_internal)
        {
            G_DeferedPlayDemo(DEH_String("demo3"));
        }
        break;
        
        // THE DEFINITIVE DOOM Special Edition demo
        case 6:
        if (demo_internal)
        {
            G_DeferedPlayDemo(DEH_String("demo4"));
        }
        break;
    }

    // The Doom 3: BFG Edition version of doom2.wad does not have a
    // TITLETPIC lump. Use INTERPIC instead as a workaround.
    if (gamevariant == bfgedition && !strcasecmp(pagename, "TITLEPIC")
    &&  W_CheckNumForName("titlepic") < 0)
    {
        pagename = DEH_String("INTERPIC");
    }
}

// -----------------------------------------------------------------------------
// D_StartTitle
// -----------------------------------------------------------------------------

void D_StartTitle (void)
{
    gameaction = ga_nothing;
    automapactive = false; // [crispy] clear overlaid automap remainings
    demosequence = -1;
    D_AdvanceDemo ();
}

// Strings for dehacked replacements of the startup banner
//
// These are from the original source: some of them are perhaps
// not used in any dehacked patches

static char *banners[] =
{
    // doom2.wad
    "                         "
    "DOOM 2: Hell on Earth v%i.%i"
    "                           ",
    // doom2.wad v1.666
    "                         "
    "DOOM 2: Hell on Earth v%i.%i66"
    "                          ",
    // doom1.wad
    "                            "
    "DOOM Shareware Startup v%i.%i"
    "                           ",
    // doom.wad
    "                            "
    "DOOM Registered Startup v%i.%i"
    "                           ",
    // Registered DOOM uses this
    "                          "
    "DOOM System Startup v%i.%i"
    "                          ",
    // Doom v1.666
    "                          "
    "DOOM System Startup v%i.%i66"
    "                          "
    // doom.wad (Ultimate DOOM)
    "                         "
    "The Ultimate DOOM Startup v%i.%i"
    "                        ",
    // tnt.wad
    "                     "
    "DOOM 2: TNT - Evilution v%i.%i"
    "                           ",
    // plutonia.wad
    "                   "
    "DOOM 2: Plutonia Experiment v%i.%i"
    "                           ",
};

//
// Get game name: if the startup banner has been replaced, use that.
// Otherwise, use the name given
// 

static char *GetGameName(const char *gamename)
{
    size_t i;

    for (i=0; i<arrlen(banners); ++i)
    {
        const char *deh_sub;
        // Has the banner been replaced?

        deh_sub = DEH_String(banners[i]);

        if (deh_sub != banners[i])
        {
            size_t gamename_size;
            int version;
            char *deh_gamename;

            // Has been replaced.
            // We need to expand via printf to include the Doom version number
            // We also need to cut off spaces to get the basic name

            gamename_size = strlen(deh_sub) + 10;
            deh_gamename = malloc(gamename_size);
            if (deh_gamename == NULL)
            {
                I_Error("GetGameName: Failed to allocate new string");
            }
            version = G_VanillaVersionCode();
            DEH_snprintf(deh_gamename, gamename_size, banners[i],
                         version / 100, version % 100);

            while (deh_gamename[0] != '\0' && isspace(deh_gamename[0]))
            {
                memmove(deh_gamename, deh_gamename + 1, gamename_size - 1);
            }

            while (deh_gamename[0] != '\0' && isspace(deh_gamename[strlen(deh_gamename)-1]))
            {
                deh_gamename[strlen(deh_gamename) - 1] = '\0';
            }

            return deh_gamename;
        }
    }

    return M_StringDuplicate(gamename);
}

static void SetMissionForPackName(char *pack_name)
{
    int i;
    static const struct
    {
        char *name;
        int mission;
    } packs[] = {
        { "doom2",    doom2 },
        { "tnt",      pack_tnt },
        { "plutonia", pack_plut },
    };

    for (i = 0; (size_t)i < arrlen(packs); ++i)
    {
        if (!strcasecmp(pack_name, packs[i].name))
        {
            gamemission = packs[i].mission;
            return;
        }
    }

    printf("Valid mission packs are:\n");

    for (i = 0; (size_t)i < arrlen(packs); ++i)
    {
        printf("\t%s\n", packs[i].name);
    }

    I_Error("Unknown mission pack name: %s", pack_name);
}

//
// Find out what version of Doom is playing.
//

void D_IdentifyVersion(void)
{
    // gamemission is set up by the D_FindIWAD function.  But if 
    // we specify '-iwad', we have to identify using 
    // IdentifyIWADByName.  However, if the iwad does not match
    // any known IWAD name, we may have a dilemma.  Try to 
    // identify by its contents.

    if (gamemission == none)
    {
        unsigned int i;

        for (i=0; i<numlumps; ++i)
        {
            if (!strncasecmp(lumpinfo[i]->name, "MAP01", 8))
            {
                gamemission = doom2;
                break;
            } 
            else if (!strncasecmp(lumpinfo[i]->name, "E1M1", 8))
            {
                gamemission = doom;
                break;
            }
        }

        if (gamemission == none)
        {
            // Still no idea.  I don't think this is going to work.

            I_Error("Unknown or invalid IWAD file.");
        }
    }

    // Make sure gamemode is set up correctly

    if (logical_gamemission == doom)
    {
        // Doom 1.  But which version?

        if (W_CheckNumForName("E4M1") > 0)
        {
            // Ultimate Doom

            gamemode = retail;
        } 
        else if (W_CheckNumForName("E3M1") > 0)
        {
            gamemode = registered;
        }
        else
        {
            gamemode = shareware;
        }
    }
    else
    {
        int p;

        // Doom 2 of some kind.
        gamemode = commercial;

        // We can manually override the gamemission that we got from the
        // IWAD detection code. This allows us to eg. play Plutonia 2
        // with Freedoom and get the right level names.

        //!
        // @category compat
        // @arg <pack>
        //
        // Explicitly specify a Doom II "mission pack" to run as, instead of
        // detecting it based on the filename. Valid values are: "doom2",
        // "tnt" and "plutonia".
        //
        p = M_CheckParmWithArgs("-pack", 1);
        if (p > 0)
        {
            SetMissionForPackName(myargv[p + 1]);
        }
    }
}

// Set the gamedescription string

void D_SetGameDescription(void)
{
    gamedescription = "Unknown";

    if (logical_gamemission == doom)
    {
        // Doom 1.  But which version?

        if (gamevariant == freedoom)
        {
            gamedescription = GetGameName("Freedoom: Phase 1");
        }
        else if (gamemode == retail)
        {
            // Ultimate Doom

            gamedescription = GetGameName("The Ultimate DOOM");
        }
        else if (gamemode == registered)
        {
            gamedescription = GetGameName("DOOM Registered");
        }
        else if (gamemode == shareware)
        {
            gamedescription = GetGameName("DOOM Shareware");
        }

        if (sigil_compat || sigil)
        {
            gamedescription = "SIGIL";
        }
        if (sigil2)
        {
            gamedescription = "SIGIL II";
        }
    }
    else
    {
        // Doom 2 of some kind.  But which mission?

        if (gamevariant == freedm)
        {
            gamedescription = GetGameName("FreeDM");
        }
        else if (gamevariant == freedoom)
        {
            gamedescription = GetGameName("Freedoom: Phase 2");
        }
        else if (logical_gamemission == doom2)
        {
            gamedescription = GetGameName("DOOM 2: Hell on Earth");
        }
        else if (logical_gamemission == pack_plut)
        {
            gamedescription = GetGameName("DOOM 2: Plutonia Experiment"); 
        }
        else if (logical_gamemission == pack_tnt)
        {
            gamedescription = GetGameName("DOOM 2: TNT - Evilution");
        }

        if (nerve)
        {
            gamedescription = "No Rest for the Living";
        }
    }
}

static boolean D_AddFile(char *filename)
{
    wad_file_t *handle;

    printf("  adding %s\n", filename);
    handle = W_AddFile(filename);

    return handle != NULL;
}

// Copyright message banners
// Some dehacked mods replace these.  These are only displayed if they are 
// replaced by dehacked.

static const char *copyright_banners[] =
{
    "===========================================================================\n"
    "ATTENTION:  This version of DOOM has been modified.  If you would like to\n"
    "get a copy of the original game, call 1-800-IDGAMES or see the readme file.\n"
    "        You will not receive technical support for modified games.\n"
    "                      press enter to continue\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "                 Commercial product - do not distribute!\n"
    "         Please report software piracy to the SPA: 1-800-388-PIR8\n"
    "===========================================================================\n",

    "===========================================================================\n"
    "                                Shareware!\n"
    "===========================================================================\n"
};

// Prints a message only if it has been modified by dehacked.

void PrintDehackedBanners(void)
{
    size_t i;

    for (i=0; i<arrlen(copyright_banners); ++i)
    {
        const char *deh_s;

        deh_s = DEH_String(copyright_banners[i]);

        if (deh_s != copyright_banners[i])
        {
            printf("%s", deh_s);

            // Make sure the modified banner always ends in a newline character.
            // If it doesn't, add a newline.  This fixes av.wad.

            if (deh_s[strlen(deh_s) - 1] != '\n')
            {
                printf("\n");
            }
        }
    }
}

static struct 
{
    const char *description;
    const char *cmdline;
    GameVersion_t version;
} gameversions[] = {
    {"Doom 1.2",             "1.2",        exe_doom_1_2},
    {"Doom 1.666",           "1.666",      exe_doom_1_666},
    {"Doom 1.7/1.7a",        "1.7",        exe_doom_1_7},
    {"Doom 1.8",             "1.8",        exe_doom_1_8},
    {"Doom 1.9",             "1.9",        exe_doom_1_9},
    {"Hacx",                 "hacx",       exe_hacx},
    {"Ultimate Doom",        "ultimate",   exe_ultimate},
    {"Final Doom",           "final",      exe_final},
    {"Final Doom (alt)",     "final2",     exe_final2},
    {"Chex Quest",           "chex",       exe_chex},
    { NULL,                  NULL,         0},
};

// Initialize the game version

static void InitGameVersion(void)
{
    byte *demolump;
    char demolumpname[6];
    int demoversion;
    int p;
    int i;
    boolean status;

    //! 
    // @arg <version>
    // @category compat
    //
    // Emulate a specific version of Doom.  Valid values are "1.2", 
    // "1.666", "1.7", "1.8", "1.9", "ultimate", "final", "final2",
    // "hacx" and "chex".
    //

    p = M_CheckParmWithArgs("-gameversion", 1);

    if (p)
    {
        for (i=0; gameversions[i].description != NULL; ++i)
        {
            if (!strcmp(myargv[p+1], gameversions[i].cmdline))
            {
                gameversion = gameversions[i].version;
                break;
            }
        }
        
        if (gameversions[i].description == NULL) 
        {
            printf("Supported game versions:\n");

            for (i=0; gameversions[i].description != NULL; ++i)
            {
                printf("\t%s (%s)\n", gameversions[i].cmdline,
                        gameversions[i].description);
            }
            
            I_Error("Unknown game version '%s'", myargv[p+1]);
        }
    }
    else
    {
        // Determine automatically

        if (gamemission == pack_chex)
        {
            // chex.exe - identified by iwad filename

            gameversion = exe_chex;
        }
        else if (gamemission == pack_hacx)
        {
            // hacx.exe: identified by iwad filename

            gameversion = exe_hacx;
        }
        else if (gamemode == shareware || gamemode == registered
              || (gamemode == commercial && gamemission == doom2))
        {
            // original
            gameversion = exe_doom_1_9;

            // Detect version from demo lump
            for (i = 1; i <= 3; ++i)
            {
                M_snprintf(demolumpname, 6, "demo%i", i);
                if (W_CheckNumForName(demolumpname) > 0)
                {
                    demolump = W_CacheLumpName(demolumpname, PU_STATIC);
                    demoversion = demolump[0];
                    W_ReleaseLumpName(demolumpname);
                    status = true;
                    switch (demoversion)
                    {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                        case 4:
                            gameversion = exe_doom_1_2;
                            break;
                        case 106:
                            gameversion = exe_doom_1_666;
                            break;
                        case 107:
                            gameversion = exe_doom_1_7;
                            break;
                        case 108:
                            gameversion = exe_doom_1_8;
                            break;
                        case 109:
                            gameversion = exe_doom_1_9;
                            break;
                        default:
                            status = false;
                            break;
                    }
                    if (status)
                    {
                        break;
                    }
                }
            }
        }
        else if (gamemode == retail)
        {
            gameversion = exe_ultimate;
        }
        else if (gamemode == commercial)
        {
            // Final Doom: tnt or plutonia
            // Defaults to emulating the first Final Doom executable,
            // which has the crash in the demo loop; however, having
            // this as the default should mean that it plays back
            // most demos correctly.

            gameversion = exe_final;
        }
    }

    // Deathmatch 2.0 did not exist until Doom v1.4
    if (gameversion <= exe_doom_1_2 && deathmatch == 2)
    {
        deathmatch = 1;
    }
    
    // The original exe does not support retail - 4th episode not supported

    if (gameversion < exe_ultimate && gamemode == retail)
    {
        gamemode = registered;
    }

    // EXEs prior to the Final Doom exes do not support Final Doom.

    if (gameversion < exe_final && gamemode == commercial
     && (gamemission == pack_tnt || gamemission == pack_plut))
    {
        gamemission = doom2;
    }
}

void PrintGameVersion(void)
{
    int i;

    for (i=0; gameversions[i].description != NULL; ++i)
    {
        if (gameversions[i].version == gameversion)
        {
            printf("Emulating the behavior of the "
                   "'%s' executable.\n", gameversions[i].description);
            break;
        }
    }
}

// Function called at exit to display the ENDOOM screen

static void D_Endoom(void)
{
    byte *endoom;

    // Don't show ENDOOM if we have it disabled, or we're running
    // in screensaver or control test mode. Only show it once the
    // game has actually started.

    if (!vid_endoom || !main_loop_started
     || screensaver_mode || M_CheckParm("-testcontrols") > 0)
    {
        return;
    }

    endoom = W_CacheLumpName(DEH_String("ENDOOM"), PU_STATIC);

    I_Endoom(endoom);
}

boolean IsFrenchIWAD(void)
{
    return (gamemission == doom2 && W_CheckNumForName("M_RDTHIS") < 0
          && W_CheckNumForName("M_EPISOD") < 0 && W_CheckNumForName("M_EPI1") < 0
          && W_CheckNumForName("M_EPI2") < 0 && W_CheckNumForName("M_EPI3") < 0
          && W_CheckNumForName("WIOSTF") < 0 && W_CheckNumForName("WIOBJ") >= 0);
}

// Load dehacked patches needed for certain IWADs.
static void LoadIwadDeh(void)
{
    // The Freedoom IWADs have DEHACKED lumps that must be loaded.
    if (gamevariant == freedoom || gamevariant == freedm)
    {
        // Old versions of Freedoom (before 2014-09) did not have technically
        // valid DEHACKED lumps, so ignore errors and just continue if this
        // is an old IWAD.
        DEH_LoadLumpByName("DEHACKED", false, true);
    }

    else // [crispy]
    // If this is the HACX IWAD, we need to load the DEHACKED lump.
    if (gameversion == exe_hacx)
    {
        if (!DEH_LoadLumpByName("DEHACKED", true, false))
        {
            I_Error("DEHACKED lump not found.  Please check that this is the "
                    "Hacx v1.2 IWAD.");
        }
    }

    else // [crispy]
    // Chex Quest needs a separate Dehacked patch which must be downloaded
    // and installed next to the IWAD.
    if (gameversion == exe_chex)
    {
        char *chex_deh = NULL;
        char *dirname;

        // Look for chex.deh in the same directory as the IWAD file.
        dirname = M_DirName(iwadfile);
        chex_deh = M_StringJoin(dirname, DIR_SEPARATOR_S, "chex.deh", NULL);
        free(dirname);

        // If the dehacked patch isn't found, try searching the WAD
        // search path instead.  We might find it...
        if (!M_FileExists(chex_deh))
        {
            free(chex_deh);
            chex_deh = D_FindWADByName("chex.deh");
        }

        // Still not found?
        if (chex_deh == NULL)
        {
            I_Error("Unable to find Chex Quest dehacked file (chex.deh).\n"
                    "The dehacked file is required in order to emulate\n"
                    "chex.exe correctly.  It can be found in your nearest\n"
                    "/idgames repository mirror at:\n\n"
                    "   themes/chex/chexdeh.zip");
        }

        if (!DEH_LoadFile(chex_deh))
        {
            I_Error("Failed to load chex.deh needed for emulating chex.exe.");
        }
    }
    // [crispy] try anyway...
    else if (W_CheckNumForName("DEHACKED") != -1)
    {
        DEH_LoadLumpByName("DEHACKED", true, true);
    }

    if (IsFrenchIWAD())
    {
        char *french_deh = NULL;
        char *dirname;

        // Look for french.deh in the same directory as the IWAD file.
        dirname = M_DirName(iwadfile);
        french_deh = M_StringJoin(dirname, DIR_SEPARATOR_S, "french.deh", NULL);
        printf("French version\n");
        free(dirname);

        // If the dehacked patch isn't found, try searching the WAD
        // search path instead.  We might find it...
        if (!M_FileExists(french_deh))
        {
            free(french_deh);
            french_deh = D_FindWADByName("french.deh");
        }

        // Still not found?
        if (french_deh == NULL)
        {
            I_Error("Unable to find French Doom II dehacked file\n"
                    "(french.deh).  The dehacked file is required in order to\n"
                    "emulate French doom2.exe correctly.  It can be found in\n"
                    "your nearest /idgames repository mirror at:\n\n"
                    "   utils/exe_edit/patches/french.zip");
        }

        if (!DEH_LoadFile(french_deh))
        {
            I_Error("Failed to load french.deh needed for emulating French\n"
                    "doom2.exe.");
        }
    }
}

static void G_CheckDemoStatusAtExit (void)
{
    G_CheckDemoStatus();
}


//
// D_DoomMain
//
void D_DoomMain (void)
{
    int p;
    char file[256];
    char demolumpname[9];
    const int starttime = SDL_GetTicks();

    // [crispy] unconditionally initialize DEH tables
    DEH_Init();

#ifdef _WIN32
    // [JN] Print colorized title.
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_BLUE
                           | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
                           | FOREGROUND_INTENSITY);

    for (p = 0 ; p < 26 ; p++) printf(" ");
    printf(PACKAGE_FULLNAME);
    for (p = 0 ; p < 27 ; p++) printf(" ");
    printf("\n");

    // [JN] Fallback to standard console colos.
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 
                            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    // print banner

    I_PrintBanner(PACKAGE_FULLNAME);
#endif

    DEH_printf("Z_Init: Init zone memory allocation daemon. \n");
    Z_Init ();
    
    // Call I_ShutdownGraphics on quit

   	I_AtExit(I_ShutdownGraphics, true);
   	i_error_title = PACKAGE_FULLNAME;

   	I_AtExit(D_Endoom, false);

    //!
    // @category net
    //
    // Start a dedicated server, routing packets but not participating
    // in the game itself.
    //

    if (M_CheckParm("-dedicated") > 0)
    {
        printf("Dedicated server mode.\n");
        NET_DedicatedServer();

        // Never returns
    }

    //!
    // @category net
    //
    // Query the Internet master server for a global list of active
    // servers.
    //

    if (M_CheckParm("-search"))
    {
        NET_MasterQuery();
        exit(0);
    }

    //!
    // @arg <address>
    // @category net
    //
    // Query the status of the server running on the given IP
    // address.
    //

    p = M_CheckParmWithArgs("-query", 1);

    if (p)
    {
        NET_QueryAddress(myargv[p+1]);
        exit(0);
    }

    //!
    // @category net
    //
    // Search the local LAN for running servers.
    //

    if (M_CheckParm("-localsearch"))
    {
        NET_LANQuery();
        exit(0);
    }

    //!
    // @vanilla
    //
    // Disable monsters.
    //
	
    nomonsters = M_CheckParm ("-nomonsters");

    //!
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    respawnparm = M_CheckParm ("-respawn");

    //!
    // @vanilla
    //
    // Monsters move faster.
    //

    fastparm = M_CheckParm ("-fast");

    //! 
    // @vanilla
    //
    // Developer mode.  F1 saves a screenshot in the current working
    // directory.
    //

    devparm = M_CheckParm ("-devparm");

    I_DisplayFPSDots(devparm);

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch game.
    //

    if (M_CheckParm ("-deathmatch"))
	deathmatch = 1;

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch 2.0 game.  Weapons do not stay in place and
    // all items respawn after 30 seconds.
    //

    if (M_CheckParm ("-altdeath"))
	deathmatch = 2;

    //!
    // @category net
    // @vanilla
    //
    // Start a deathmatch 3.0 game.  Weapons stay in place and
    // all items respawn after 30 seconds.
    //

    if (M_CheckParm ("-dm3"))
	deathmatch = 3;

    if (devparm)
	DEH_printf(D_DEVSTR);
    
    //!
    // @category game
    //
    // Start single player game with items spawns as in cooperative netgame.
    //

    coop_spawns = M_CheckParm ("-coop_spawns");

    // find which dir to use for config files

#ifdef _WIN32

    //!
    // @platform windows
    // @vanilla
    //
    // Save configuration data and savegames in c:\doomdata,
    // allowing play from CD.
    //

    if (M_ParmExists("-cdrom"))
    {
        printf(D_CDROM);

        M_SetConfigDir("c:\\doomdata\\");
    }
    else
#endif
    {
        // Auto-detect the configuration dir.

        M_SetConfigDir(NULL);
    }

    //!
    // @arg <x>
    // @vanilla
    //
    // Turbo mode.  The player's speed is multiplied by x%.  If unspecified,
    // x defaults to 200.  Values are rounded up to 10 and down to 400.
    //

    if ( (p=M_CheckParm ("-turbo")) )
    {
	int     scale = 200;
	
	if (p<myargc-1)
	    scale = atoi (myargv[p+1]);
	if (scale < 10)
	    scale = 10;
	if (scale > 400)
	    scale = 400;
        DEH_printf("turbo scale: %i%%\n", scale);
	forwardmove[0] = forwardmove[0]*scale/100;
	forwardmove[1] = forwardmove[1]*scale/100;
	sidemove[0] = sidemove[0]*scale/100;
	sidemove[1] = sidemove[1]*scale/100;
    }
    
    // Load configuration files before initialising other subsystems.
    DEH_printf("M_LoadDefaults: Load system defaults.\n");
    M_SetConfigFilenames(PROGRAM_PREFIX "doom.ini");
    D_BindVariables();
    M_LoadDefaults();

    // [JN] Set screeenshot files dir.
    M_SetScreenshotDir();

    // [JN] Disk icon can be enabled for Doom.
    diskicon_enabled = true;

#ifdef _WIN32
    // [JN] Pressing PrintScreen on Windows 11 is opening Snipping Tool.
    // Re-register PrintScreen key pressing for port needs to avoid this.
    // Taken from DOOM Retro.
    if (key_menu_screenshot == KEY_PRTSCR)
    {
        RegisterHotKey(NULL, 1, MOD_ALT, VK_SNAPSHOT);
        RegisterHotKey(NULL, 2, 0, VK_SNAPSHOT);
    }
#endif

    // Save configuration at exit.
    I_AtExit(M_SaveDefaults, true); // [crispy] always save configuration at exit

    // Find main IWAD file and load it.
    iwadfile = D_FindIWAD(IWAD_MASK_DOOM, &gamemission);

    // None found?

    if (iwadfile == NULL)
    {
        i_error_safe = true;
        I_Error("Game mode indeterminate, no IWAD file was found.\n"
                "Try to do following:\n"
                "- Copy IWAD file into the folder with executable file.\n"
                "- Drag and drop IWAD file onto executable file.\n"
                "- Specifying one with the '-iwad' command line parameter.\n\n"
                "Valid IWAD files are:\n"
                "doom1.wad, doom.wad, doom2.wad, plutonia.wad, tnt.wad,\n"
                "freedoom1.wad or freedoom2.wad");
    }

    modifiedgame = false;

    DEH_printf("W_Init: Init WADfiles.\n");
    D_AddFile(iwadfile);

    W_CheckCorrectIWAD(doom);

    // Now that we've loaded the IWAD, we can figure out what gamemission
    // we're playing and which version of Vanilla Doom we need to emulate.
    D_IdentifyVersion();
    InitGameVersion();

    // Check which IWAD variant we are using.

    if (W_CheckNumForName("FREEDOOM") >= 0)
    {
        if (W_CheckNumForName("FREEDM") >= 0)
        {
            gamevariant = freedm;
        }
        else
        {
            gamevariant = freedoom;
        }
    }
    else if (W_CheckNumForName("DMENUPIC") >= 0)
    {
        gamevariant = bfgedition;
    }

    //!
    // @category mod
    //
    // Disable automatic loading of Dehacked patches for certain
    // IWAD files.
    //
    if (!M_ParmExists("-nodeh"))
    {
        // Some IWADs have dehacked patches that need to be loaded for
        // them to be played properly.
        LoadIwadDeh();
    }

    // Doom 3: BFG Edition includes modified versions of the classic
    // IWADs which can be identified by an additional DMENUPIC lump.
    // Furthermore, the M_GDHIGH lumps have been modified in a way that
    // makes them incompatible to Vanilla Doom and the modified version
    // of doom2.wad is missing the TITLEPIC lump.
    // We specifically check for DMENUPIC here, before PWADs have been
    // loaded which could probably include a lump of that name.

    if (gamevariant == bfgedition)
    {
        printf("BFG Edition: Using workarounds as needed.\n");

        // BFG Edition changes the names of the secret levels to
        // censor the Wolfenstein references. It also has an extra
        // secret level (MAP33). In Vanilla Doom (meaning the DOS
        // version), MAP33 overflows into the Plutonia level names
        // array, so HUSTR_33 is actually PHUSTR_1.
        DEH_AddStringReplacement(HUSTR_31, "level 31: idkfa");
        DEH_AddStringReplacement(HUSTR_32, "level 32: keen");
        DEH_AddStringReplacement(PHUSTR_1, "level 33: betray");

        // The BFG edition doesn't have the "low detail" menu option (fair
        // enough). But bizarrely, it reuses the M_GDHIGH patch as a label
        // for the options menu (says "Fullscreen:"). Why the perpetrators
        // couldn't just add a new graphic lump and had to reuse this one,
        // I don't know.
        //
        // The end result is that M_GDHIGH is too wide and causes the game
        // to crash. As a workaround to get a minimum level of support for
        // the BFG edition IWADs, use the "ON"/"OFF" graphics instead.
        DEH_AddStringReplacement("M_GDHIGH", "M_MSGON");
        DEH_AddStringReplacement("M_GDLOW", "M_MSGOFF");

        // The BFG edition's "Screen Size:" graphic has also been changed
        // to say "Gamepad:". Fortunately, it (along with the original
        // Doom IWADs) has an unused graphic that says "Display". So we
        // can swap this in instead, and it kind of makes sense.
        DEH_AddStringReplacement("M_SCRNSZ", "M_DISP");
    }

    //!
    // @category mod
    //
    // Disable auto-loading of .wad and .deh files.
    //
    if (!M_ParmExists("-noautoload") && gamemode != shareware)
    {
        char *autoload_dir;

        // common auto-loaded files for all Doom flavors

        if (gamemission < pack_chex && gamevariant != freedoom)
        {
            autoload_dir = M_GetAutoloadDir("doom-all");
            if (autoload_dir != NULL)
            {
                DEH_AutoLoadPatches(autoload_dir);
                W_AutoLoadWADs(autoload_dir);
                free(autoload_dir);
            }
        }

        // auto-loaded files per IWAD
        autoload_dir = M_GetAutoloadDir(D_SaveGameIWADName(gamemission, gamevariant));
        if (autoload_dir != NULL)
        {
            DEH_AutoLoadPatches(autoload_dir);
            W_AutoLoadWADs(autoload_dir);
            free(autoload_dir);
        }
    }

    // Load Dehacked patches specified on the command line with -deh.
    // Note that there's a very careful and deliberate ordering to how
    // Dehacked patches are loaded. The order we use is:
    //  1. IWAD dehacked patches.
    //  2. Command line dehacked patches specified with -deh.
    //  3. PWAD dehacked patches in DEHACKED lumps.
    DEH_ParseCommandLine();

    // Load PWAD files.
    modifiedgame = W_ParseCommandLine();

    // Debug:
//    W_PrintDirectory();

    //!
    // @arg <demo>
    // @category demo
    // @vanilla
    //
    // Play back the demo named demo.lmp.
    //

    p = M_CheckParmWithArgs ("-playdemo", 1);

    if (!p)
    {
        //!
        // @arg <demo>
        // @category demo
        // @vanilla
        //
        // Play back the demo named demo.lmp, determining the framerate
        // of the screen.
        //
	p = M_CheckParmWithArgs("-timedemo", 1);

    }

    if (p)
    {
        char *uc_filename = strdup(myargv[p + 1]);
        M_ForceUppercase(uc_filename);

        // With Vanilla you have to specify the file without extension,
        // but make that optional.
        if (M_StringEndsWith(uc_filename, ".LMP"))
        {
            M_StringCopy(file, myargv[p + 1], sizeof(file));
        }
        else
        {
            DEH_snprintf(file, sizeof(file), "%s.lmp", myargv[p+1]);
        }

        free(uc_filename);

        if (D_AddFile(file))
        {
            M_StringCopy(demolumpname, lumpinfo[numlumps - 1]->name,
                         sizeof(demolumpname));
        }
        else
        {
            // If file failed to load, still continue trying to play
            // the demo in the same way as Vanilla Doom.  This makes
            // tricks like "-playdemo demo1" possible.

            M_StringCopy(demolumpname, myargv[p + 1], sizeof(demolumpname));
        }

        printf("Playing demo %s.\n", file);
    }

    I_AtExit(G_CheckDemoStatusAtExit, true);

    // [JN] Check for SIGIL (compat) and NERVE loading.
    p = M_CheckParmWithArgs ("-file", 1);
    if (p)
    {
        while (++p != myargc && myargv[p][0] != '-')
        {
            // SIGIL compat. Just replace window title.
            if (M_StrCaseStr(myargv[p], "SIGIL_COMPAT_v1_21.wad")
            ||  M_StrCaseStr(myargv[p], "SIGIL_COMPAT_v1_2.wad")
            ||  M_StrCaseStr(myargv[p], "SIGIL_COMPAT.wad"))
            {
                sigil_compat = true;
            }

            // NERVE
            if (M_StrCaseStr(myargv[p], "NERVE.wad"))
            {
                nerve = true;
            }
        }
    }

    // [JN] Check for SIGIL (main) and SIGIL II.
    {
        // SIGIL I
        sigil = (gameversion == exe_ultimate) &&
                (W_CheckNumForName("m_epi5") != -1) &&
                (W_CheckNumForName("e5m1") != -1) &&
                (W_CheckNumForName("wilv40") != -1);

        // SIGIL II
        sigil2 = (gameversion == exe_ultimate) &&
                 (W_CheckNumForName("m_epi6") != -1) &&
                 (W_CheckNumForName("e6m1") != -1) &&
                 (W_CheckNumForName("wilv50") != -1);
    }

    // Generate the WAD hash table.  Speed things up a bit.
    W_GenerateHashTable();

    // Load DEHACKED lumps from WAD files - but only if we give the right
    // command line parameter.

    // [crispy] load DEHACKED lumps by default, but allow overriding

    //!
    // @category mod
    //
    // Disable automatic loading of embedded DEHACKED lumps in wad files.
    //
    if (!M_ParmExists("-nodehlump") && !M_ParmExists("-nodeh"))
    {
        int i, loaded = 0;
        int numiwadlumps = numlumps;

        while (!W_IsIWADLump(lumpinfo[numiwadlumps - 1]))
        {
            numiwadlumps--;
        }

        for (i = numiwadlumps; i < numlumps; ++i)
        {
            if (!strncmp(lumpinfo[i]->name, "DEHACKED", 8))
            {
                DEH_LoadLump(i, true, true); // [crispy] allow long, allow error
                loaded++;
            }
        }

        printf("  loaded %i DEHACKED lumps from PWAD files.\n", loaded);
    }

    // Set the gamedescription string. This is only possible now that
    // we've finished loading Dehacked patches.
    D_SetGameDescription();

    // [JN] Set the default directory where savegames are saved.
    D_SetDefaultSavePath();

    // Check for -file in shareware
    if (modifiedgame && (gamevariant != freedoom))
    {
	// These are the lumps that will be checked in IWAD,
	// if any one is not present, execution will be aborted.
	char name[23][8]=
	{
	    "e2m1","e2m2","e2m3","e2m4","e2m5","e2m6","e2m7","e2m8","e2m9",
	    "e3m1","e3m3","e3m3","e3m4","e3m5","e3m6","e3m7","e3m8","e3m9",
	    "dphoof","bfgga0","heada1","cybra1","spida1d1"
	};
	int i;
	
	if ( gamemode == shareware)
	{
	    i_error_safe = true;
	    I_Error(DEH_String("\nYou cannot -file with the shareware "
			       "version. Register!"));
	}

	// Check for fake IWAD with right name,
	// but w/o all the lumps of the registered version. 
	if (gamemode == registered)
	    for (i = 0;i < 23; i++)
		if (W_CheckNumForName(name[i])<0)
		    I_Error(DEH_String("\nThis is not the registered version."));
    }

    if (W_CheckNumForName("SS_START") >= 0
     || W_CheckNumForName("FF_END") >= 0)
    {
        I_PrintDivider();
        printf(" WARNING: The loaded WAD file contains modified sprites or\n"
               " floor textures.  You may want to use the '-merge' command\n"
               " line option instead of '-file'.\n");
    }

    I_PrintStartupBanner(gamedescription);
    PrintDehackedBanners();

    DEH_printf("I_Init: Setting up machine state.\n");
    I_CheckIsScreensaver();
    I_InitTimer();
    I_InitJoystick();
    I_InitSound(doom);
    I_InitMusic();

    // [crispy] check for SSG resources
    havessg =
    (
        gamemode == commercial ||
        (
            W_CheckNumForName("sht2a0")         != -1 && // [crispy] wielding/firing sprite sequence
            I_GetSfxLumpNum(&S_sfx[sfx_dshtgn]) != -1 && // [crispy] firing sound
            I_GetSfxLumpNum(&S_sfx[sfx_dbopn])  != -1 && // [crispy] opening sound
            I_GetSfxLumpNum(&S_sfx[sfx_dbload]) != -1 && // [crispy] reloading sound
            I_GetSfxLumpNum(&S_sfx[sfx_dbcls])  != -1    // [crispy] closing sound
        )
    );

    printf ("NET_Init: Init network subsystem.\n");
    NET_Init ();

    // Initial netgame startup. Connect to server etc.
    D_ConnectNetGame();

    // get skill / episode / map from parms
    // [JN] Use chosen default skill level.
    startskill = gp_default_skill;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    //!
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParmWithArgs("-skill", 1);

    if (p)
    {
	startskill = myargv[p+1][0]-'1';
	autostart = true;
    }

    //!
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParmWithArgs("-episode", 1);

    if (p)
    {
	startepisode = myargv[p+1][0]-'0';
	startmap = 1;
	autostart = true;
    }
	
    timelimit = 0;

    //! 
    // @arg <n>
    // @category net
    // @vanilla
    //
    // For multiplayer games: exit each level after n minutes.
    //

    p = M_CheckParmWithArgs("-timer", 1);

    if (p)
    {
	timelimit = atoi(myargv[p+1]);
    }

    //!
    // @category net
    // @vanilla
    //
    // Austin Virtual Gaming: end levels after 20 minutes.
    //

    p = M_CheckParm ("-avg");

    if (p)
    {
	timelimit = 20;
    }

    //!
    // @arg [<x> <y> | <xy>]
    // @vanilla
    //
    // Start a game immediately, warping to ExMy (Doom 1) or MAPxy
    // (Doom 2)
    //

    p = M_CheckParmWithArgs("-warp", 1);

    if (p)
    {
        if (gamemode == commercial)
            startmap = atoi (myargv[p+1]);
        else
        {
            startepisode = myargv[p+1][0]-'0';

            // [crispy] only if second argument is not another option
            if (p + 2 < myargc && myargv[p+2][0] != '-')
            {
                startmap = myargv[p+2][0]-'0';
            }
            else
            {
                // [crispy] allow second digit without space in between for Doom 1
                startmap = myargv[p+1][1]-'0';
            }
        }
        autostart = true;
        // [crispy] if used with -playdemo, fast-forward demo up to the desired map
        demowarp = startmap;
    }

    // Undocumented:
    // Invoked by setup to test the controls.

    p = M_CheckParm("-testcontrols");

    if (p > 0)
    {
        startepisode = 1;
        startmap = 1;
        autostart = true;
        testcontrols = true;
    }

    // Check for load game parameter
    // We do this here and save the slot number, so that the network code
    // can override it or send the load slot to other players.

    //!
    // @arg <s>
    // @vanilla
    //
    // Load the game in slot s.
    //

    p = M_CheckParmWithArgs("-loadgame", 1);
    
    if (p)
    {
        startloadgame = atoi(myargv[p+1]);
    }
    else
    {
        // Not loading a game
        startloadgame = -1;
    }

    DEH_printf("M_Init: Init miscellaneous info.\n");
    M_Init ();

    DEH_printf("R_Init: Init DOOM refresh daemon - [");
    R_Init ();

    DEH_printf("\nP_Init: Init Playloop state.\n");
    P_Init ();

    DEH_printf("S_Init: Setting up sound.\n");
    S_Init (sfxVolume * 8, musicVolume * 8);

    DEH_printf("D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame ();

    PrintGameVersion();

    DEH_printf("CT_Init: Setting up messages system.\n");
    CT_Init ();

    DEH_printf("ST_Init: Init status bar.\n");
    ST_Init ();

    // [JN] Predefine some automap variables at program startup.
    AM_Init ();

    // [JN] Show startup process time.
    printf("Startup process took %d ms.\n", SDL_GetTicks() - starttime);

    // If Doom II without a MAP01 lump, this is a store demo.
    // Moved this here so that MAP01 isn't constantly looked up
    // in the main loop.

    if (gamemode == commercial && W_CheckNumForName("map01") < 0)
        storedemo = true;

    if (M_CheckParmWithArgs("-statdump", 1))
    {
        I_AtExit(StatDump, true);
        DEH_printf("External statistics registered.\n");
    }

    //!
    // @arg <x>
    // @category demo
    // @vanilla
    //
    // Record a demo named x.lmp.
    //

    p = M_CheckParmWithArgs("-record", 1);

    if (p)
    {
	G_RecordDemo (myargv[p+1]);
	autostart = true;
    }

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (p)
    {
	singledemo = true;              // quit after one demo
	G_DeferedPlayDemo (demolumpname);
	D_DoomLoop ();  // never returns
    }
    demowarp = 0; // [crispy] we don't play a demo, so don't skip maps
	
    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p)
    {
	G_TimeDemo (demolumpname);
	D_DoomLoop ();  // never returns
    }
	
    if (startloadgame >= 0)
    {
        M_StringCopy(file, P_SaveGameFile(startloadgame), sizeof(file));
	G_LoadGame(file);
    }
	
    if (gameaction != ga_loadgame )
    {
		if (autostart || netgame)
			G_InitNew (startskill, startepisode, startmap);
		else
		{
			D_StartTitle ();                // start up intro loop
		}
    }

    D_DoomLoop ();  // never returns
}
