//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

// D_main.c

#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "txt_main.h"
#include "txt_io.h"

#include "net_client.h"

#include "config.h"
#include "ct_chat.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "deh_main.h"
#include "d_iwad.h"
#include "f_wipe.h"
#include "i_endoom.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_sound.h"
#include "i_swap.h" // [crispy] SHORT()
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "s_sound.h"
#include "w_main.h"
#include "v_diskicon.h"
#include "v_trans.h"
#include "v_video.h"
#include "am_map.h"

#include "icon.c"

#include "id_vars.h"
#include "id_func.h"


#define STARTUP_WINDOW_X 17
#define STARTUP_WINDOW_Y 7

GameMode_t gamemode = indetermined;
const char *gamedescription = "unknown";

boolean nomonsters;             // checkparm of -nomonsters
boolean respawnparm;            // checkparm of -respawn
boolean debugmode;              // checkparm of -debug
boolean ravpic;                 // checkparm of -ravpic
boolean coop_spawns = false;    // [crispy] checkparm of -coop_spawns
boolean cdrom;                  // true if cd-rom mode active

skill_t startskill;
int startepisode;
int startmap;
//int graphical_startup = 0;
static boolean using_graphical_startup;
static boolean main_loop_started = false;
boolean autostart;

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;

boolean advancedemo;

void D_ConnectNetGame(void);
void D_CheckNetGame(void);
void D_PageDrawer(void);
void D_AdvanceDemo(void);
boolean F_Responder(event_t * ev);

//---------------------------------------------------------------------------
//
// PROC D_ProcessEvents
//
// Send all the events of the given timestamp down the responder chain.
//
//---------------------------------------------------------------------------

void D_ProcessEvents(void)
{
    event_t *ev;

    while ((ev = D_PopEvent()) != NULL)
    {
        if (F_Responder(ev))
        {
            continue;
        }
        if (MN_Responder(ev))
        {
            continue;
        }
        G_Responder(ev);
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawMessage
//
//---------------------------------------------------------------------------

static void DrawMessage (void)
{
    player_t *player = &players[displayplayer];
    
    if (player->messageTics <= 0 || !player->message)
    {                           // No message
        return;
    }
    MN_DrTextA(player->message, 160 - MN_TextAWidth(player->message) / 2, 1, player->messageColor);
}

// -----------------------------------------------------------------------------
// ID_DrawMessageCentered
// [JN] Draws message on the screen.
// -----------------------------------------------------------------------------

static void ID_DrawMessageCentered (void)
{
    player_t *player = &players[displayplayer];

    if (player->messageCenteredTics <= 0 || !player->messageCentered)
    {
        return;  // No message
    }

    // Always centered and colored yellow.
    MN_DrTextACentered(player->messageCentered,
                       gp_revealed_secrets == 1 ? 10 : 60, cr[CR_YELLOW]);
}

//---------------------------------------------------------------------------
//
// PROC D_Display
//
// Draw current display, possibly wiping it from the previous.
//
//---------------------------------------------------------------------------

void D_Display(void)
{
    int      nowtime;
    int      tics;
    int      wipestart;
    boolean  done;
    boolean  wipe;
    // [JN] Optimized screen background and beveled edge drawing.
    static gamestate_t oldgamestate = -1;

    // For comparative timing / profiling
    if (nodrawers)
    {
        return;
    }

    // [crispy] post-rendering function pointer to apply config changes
    // that affect rendering and that are better applied after the current
    // frame has finished rendering
    if (post_rendering_hook)
    {
        post_rendering_hook();
        post_rendering_hook = NULL;
    }

    if (vid_uncapped_fps)
    {
        I_StartDisplay();
        G_FastResponder();
        G_PrepTiccmd();
    }

    // Change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize();
        // Force background redraw
        oldgamestate = -1;
    }

    // save the current screen if about to wipe
    // [JN] Make screen wipe optional, use external config variable.
    if (gamestate != wipegamestate && vid_screenwipe_hr)
    {
        wipe = true;
        wipe_StartScreen();
    }
    else
    {
        wipe = false;
    }

//
// do buffered drawing
//
    switch (gamestate)
    {
        case GS_LEVEL:
            if (!gametic)
                break;
            // draw the view directly
            R_RenderPlayerView(&players[displayplayer]);

            // [JN] Fail-safe: return earlier if post rendering hook is still active.
            if (post_rendering_hook)
            {
                return;
            }

            // [JN] See if the border needs to be initially drawn.
            if (oldgamestate != GS_LEVEL)
            {
                R_FillBackScreen();
            }

            // [JN] See if the border needs to be updated to the screen.
            if (scaledviewwidth != SCREENWIDTH)
            {
                R_DrawViewBorder();
            }

            if (automapactive)
            {
                AM_Drawer();
            }

            // [JN] Allow to draw level name separately from automap.
            if (automapactive || (widget_levelname && widget_enable && dp_screen_size < 13))
            {
                AM_LevelNameDrawer();
            }

            CT_Drawer();

            // [JN] Main status bar drawing function.
            if (dp_screen_size < 13 || (automapactive && !automap_overlay))
            {
                SB_Drawer();
            }

            if (widget_enable)
            {
                // [JN] Left widgets are available while active game level.
                if (dp_screen_size < 13)
                {
                    ID_LeftWidgets();
                }

                // [crispy] demo progress bar
                if (demoplayback && demo_bar)
                {
                    ID_DemoBar();
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
            }

            // [JN] Draw crosshair.
            if (xhair_draw && !automapactive)
            {
                ID_DrawCrosshair();
            }

            break;
        case GS_INTERMISSION:
            IN_Drawer();
            break;
        case GS_FINALE:
            F_Drawer();
            break;
        case GS_DEMOSCREEN:
            D_PageDrawer();
            break;
    }

    // [JN] Right widgets are not available while finale screens.
    if (widget_enable)
    {
        if (dp_screen_size < 13 && gamestate != GS_FINALE)
        {
            ID_RightWidgets();
        }
    }

    if (testcontrols)
    {
        V_DrawMouseSpeedBox(testcontrols_mousespeed);
    }

    oldgamestate = wipegamestate = gamestate;

    if (paused && !MenuActive && !askforquit)
    {
        if (!netgame)
        {
            V_DrawShadowedPatchOptional(160, (viewwindowy / vid_resolution) + 5, 1, W_CacheLumpName(DEH_String("PAUSED"),
                                                              PU_CACHE));
        }
        else
        {
            V_DrawShadowedPatchOptional(160, 70, 1, W_CacheLumpName(DEH_String("PAUSED"), PU_CACHE));
        }
    }
    // [JN] Handle centered player messages.
    ID_DrawMessageCentered();

    // Menu drawing
    MN_Drawer();

    // [JN] Handle player messages on top of game menu.
    DrawMessage();

    // Send out any new accumulation
    NetUpdate();

    // Normal update
    if (!wipe)
    {
    // Flush buffered stuff to screen
    I_FinishUpdate();
    return;
    }

    // Wipe update
    wipe_EndScreen();
    wipestart = I_GetTime () - 1;

    do
    {
        do
        {
            nowtime = I_GetTime ();
            tics = nowtime - wipestart;
            I_Sleep(1);
#ifndef CRISPY_TRUECOLOR
        // [JN] Note: in paletted render tics are counting slower,
        // since the effect can't be smooth because of palette limitation.
        } while (tics < 3);
#else
        } while (tics <= 0);
#endif

            wipestart = nowtime;
            done = wipe_ScreenWipe(tics);
            MN_Drawer();       // Menu is drawn even on top of wipes
            I_FinishUpdate();  // Flush buffered stuff to screen
        } while (!done);
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

boolean D_GrabMouseCallback(void)
{
    // [JN] CRL - always grab mouse in spectator mode.
    // It's supposed to be controlled by hand, even while pause.
    // However, do not grab mouse while active game menu.

    if (crl_spectating)
        return MenuActive ? false : true;

    // when menu is active or game is paused, release the mouse

    if (MenuActive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL) && !demoplayback && !advancedemo;
}

//---------------------------------------------------------------------------
//
// PROC D_DoomLoop
//
//---------------------------------------------------------------------------

void D_DoomLoop(void)
{
    I_GraphicsCheckCommandLine();
    I_SetGrabMouseCallback(D_GrabMouseCallback);
    I_RegisterWindowIcon(heretic_data, heretic_w, heretic_h);
    I_InitGraphics();

    main_loop_started = true;

    while (1)
    {
        // Frame syncronous IO operations
        I_StartFrame();

        // Process one or more tics
        // Will run at least one tic
        TryRunTics();

        // Move positional sounds
        if (oldgametic < gametic)
        {
            // [JN] Mute and restore sound and music volume.
            if (snd_mute_inactive && volume_needs_update)
            {
                S_MuteUnmuteSound (!window_focused);
            }

            S_UpdateSounds(players[displayplayer].mo);
            oldgametic = gametic;
        }

        // Update display, next frame, with current state.
        if (screenvisible)
        {
            D_Display();
        }

    }
}

/*
===============================================================================

						DEMO LOOP

===============================================================================
*/

static int demosequence;
static int pagetic;
static const char *pagename;


/*
================
=
= D_PageTicker
=
= Handles timing for warped projection
=
================
*/

void D_PageTicker(void)
{
    if (--pagetic < 0)
        D_AdvanceDemo();
}


/*
================
=
= D_PageDrawer
=
================
*/

void D_PageDrawer(void)
{
    V_DrawFullscreenRawOrPatch(W_GetNumForName(pagename));
    if (demosequence == 1)
    {
        V_DrawPatch(4, 160, W_CacheLumpName(DEH_String("ADVISOR"), PU_CACHE));
    }
}

/*
=================
=
= D_AdvanceDemo
=
= Called after each demo or intro demosequence finishes
=================
*/

void D_AdvanceDemo(void)
{
    advancedemo = true;
}

void D_DoAdvanceDemo(void)
{
    players[consoleplayer].playerstate = PST_LIVE;      // don't reborn
    advancedemo = false;
    usergame = false;           // can't save / end game here
    paused = false;
    gameaction = ga_nothing;
    demosequence = (demosequence + 1) % 7;
    switch (demosequence)
    {
        case 0:
            pagetic = 210;
            gamestate = GS_DEMOSCREEN;
            pagename = DEH_String("TITLE");
            S_StartSong(mus_titl, false);
            break;
        case 1:
            pagetic = 140;
            gamestate = GS_DEMOSCREEN;
            pagename = DEH_String("TITLE");
            break;
        case 2:
            if (demo_internal)
            {
                G_DeferedPlayDemo(DEH_String("demo1"));
            }
            break;
        case 3:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            pagename = DEH_String("CREDIT");
            break;
        case 4:
            if (demo_internal)
            {
                G_DeferedPlayDemo(DEH_String("demo2"));
            }
            break;
        case 5:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            if (gamemode == shareware)
            {
                pagename = DEH_String("ORDER");
            }
            else
            {
                pagename = DEH_String("CREDIT");
            }
            break;
        case 6:
            if (demo_internal)
            {
                G_DeferedPlayDemo(DEH_String("demo3"));
            }
            break;
    }
}


/*
=================
=
= D_StartTitle
=
=================
*/

void D_StartTitle(void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo();
}


/*
==============
=
= D_CheckRecordFrom
=
= -recordfrom <savegame num> <demoname>
==============
*/

void D_CheckRecordFrom(void)
{
    int p;
    char *filename;

    //!
    // @vanilla
    // @category demo
    // @arg <savenum> <demofile>
    //
    // Record a demo, loading from the given filename. Equivalent
    // to -loadgame <savenum> -record <demofile>.

    p = M_CheckParmWithArgs("-recordfrom", 2);
    if (!p)
        return;

    filename = SV_Filename(myargv[p + 1][0] - '0');
    G_LoadGame(filename);
    G_DoLoadGame();             // load the gameskill etc info from savegame

    G_RecordDemo(gameskill, 1, gameepisode, gamemap, myargv[p + 2]);
    D_DoomLoop();               // never returns
    free(filename);
}

/*
===============
=
= D_AddFile
=
===============
*/

// MAPDIR should be defined as the directory that holds development maps
// for the -wart # # command

#define MAPDIR "\\data\\"

#define SHAREWAREWADNAME "heretic1.wad"

char *iwadfile;


void wadprintf(void)
{
    if (debugmode)
    {
        return;
    }
}

boolean D_AddFile(char *file)
{
    wad_file_t *handle;

    printf("  adding %s\n", file);

    handle = W_AddFile(file);

    return handle != NULL;
}

//==========================================================
//
//  Startup Thermo code
//
//==========================================================
#define MSG_Y       9
#define THERM_X     14
#define THERM_Y     14

int thermMax;
int thermCurrent;
char smsg[80];                  // status bar line

//
//  Heretic startup screen shit
//

static int startup_line = STARTUP_WINDOW_Y;

void hprintf(const char *string)
{
    if (using_graphical_startup)
    {
        TXT_BGColor(TXT_COLOR_CYAN, 0);
        TXT_FGColor(TXT_COLOR_BRIGHT_WHITE);

        TXT_GotoXY(STARTUP_WINDOW_X, startup_line);
        ++startup_line;
        TXT_Puts(string);

        TXT_UpdateScreen();
    }

    // haleyjd: shouldn't be WATCOMC-only
    if (debugmode)
        puts(string);
}

void drawstatus(void)
{
    int i;

    TXT_GotoXY(1, 24);
    TXT_BGColor(TXT_COLOR_BLUE, 0);
    TXT_FGColor(TXT_COLOR_BRIGHT_WHITE);

    for (i=0; smsg[i] != '\0'; ++i) 
    {
        TXT_PutChar(smsg[i]);
    }
}

static void status(const char *string)
{
    if (using_graphical_startup)
    {
        M_StringConcat(smsg, string, sizeof(smsg));
        drawstatus();
    }
}

void DrawThermo(void)
{
    static int last_progress = -1;
    int progress;
    int i;

    if (!using_graphical_startup)
    {
        return;
    }

    // No progress? Don't update the screen.

    progress = (50 * thermCurrent) / thermMax + 2;

    if (last_progress == progress)
    {
        return;
    }

    last_progress = progress;

    TXT_GotoXY(THERM_X, THERM_Y);

    TXT_FGColor(TXT_COLOR_BRIGHT_GREEN);
    TXT_BGColor(TXT_COLOR_GREEN, 0);

    for (i = 0; i < progress; i++)
    {
        TXT_PutChar(0xdb);
    }

    TXT_UpdateScreen();
    
    // [JN] Emulate slower startup.
    if (vid_graphical_startup == 2)
    {
        I_Sleep(50);
    }
}

void initStartup(void)
{
    byte *textScreen;
    byte *loading;

    if (!vid_graphical_startup || debugmode || testcontrols)
    {
        using_graphical_startup = false;
        return;
    }

    if (!TXT_Init()) 
    {
        using_graphical_startup = false;
        return;
    }

    I_InitWindowTitle();
    I_InitWindowIcon();

    // Blit main screen
    textScreen = TXT_GetScreenData();
    loading = W_CacheLumpName(DEH_String("LOADING"), PU_CACHE);
    memcpy(textScreen, loading, 4000);

    // Print version string

    TXT_BGColor(TXT_COLOR_RED, 0);
    TXT_FGColor(TXT_COLOR_YELLOW);
    TXT_GotoXY(46, 2);
    TXT_Puts(HERETIC_VERSION_TEXT);

    TXT_UpdateScreen();

    using_graphical_startup = true;
}

static void finishStartup(void)
{
    if (using_graphical_startup)
    {
        TXT_Shutdown();
    }
}

char tmsg[300];
void tprintf(const char *msg, int initflag)
{
    printf("%s", msg);
}

// haleyjd: moved up, removed WATCOMC code
void CleanExit(void)
{
    DEH_printf("Exited from HERETIC.\n");
    exit(1);
}

void CheckAbortStartup(void)
{
    // haleyjd: removed WATCOMC
    // haleyjd FIXME: this should actually work in text mode too, but how to
    // get input before SDL video init?
    if(using_graphical_startup)
    {
        if(TXT_GetChar() == 27)
            CleanExit();
    }
}

void IncThermo(void)
{
    thermCurrent++;
    DrawThermo();
    CheckAbortStartup();
}

void InitThermo(int max)
{
    thermMax = max;
    thermCurrent = 0;
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
    M_BindHereticControls();
    M_BindChatControls(MAXPLAYERS);

    key_multi_msgplayer[0] = CT_KEY_GREEN;
    key_multi_msgplayer[1] = CT_KEY_YELLOW;
    key_multi_msgplayer[2] = CT_KEY_RED;
    key_multi_msgplayer[3] = CT_KEY_BLUE;

    NET_BindVariables();

    // [JN] Game-dependent variables:
    M_BindIntVariable("key_message_refresh",    &key_message_refresh_hr);
    M_BindIntVariable("sfx_volume",             &snd_MaxVolume);
    M_BindIntVariable("music_volume",           &snd_MusicVolume);
    //M_BindIntVariable("graphical_startup",      &graphical_startup);

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
	ID_BindVariables(heretic);
}

// 
// Called at exit to display the ENDOOM screen (ENDTEXT in Heretic)
//

static void D_Endoom(void)
{
    byte *endoom_data;
    const char *endoom_name;

    // Disable ENDOOM?

    if (!vid_endoom || testcontrols || !main_loop_started)
    {
        return;
    }

    // [JN] Extended to show for "PWAD only".
    endoom_name = DEH_String("ENDTEXT");
    endoom_data = W_CacheLumpName(endoom_name, PU_STATIC);
    
    if (vid_endoom == 1
    || (vid_endoom == 2 && !W_IsIWADLump(lumpinfo[W_GetNumForName(endoom_name)])))
    {
        I_Endoom(endoom_data);
    }
}

// static const char *const loadparms[] = {"-file", "-merge", NULL}; // [crispy]

//---------------------------------------------------------------------------
//
// PROC D_DoomMain
//
//---------------------------------------------------------------------------

void D_DoomMain(void)
{
    GameMission_t gamemission;
    int p;
    char file[256];
    char demolumpname[9];
    const int starttime = SDL_GetTicks();

#ifdef _WIN32
    // [JN] Print colorized title.
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_GREEN
                           | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
                           | FOREGROUND_INTENSITY);

    for (p = 0 ; p < 26 ; p++) printf(" ");
    printf(PACKAGE_FULLNAME_HERETIC);
    for (p = 0 ; p < 37 ; p++) printf(" ");
    printf("\n");

    // [JN] Fallback to standard console colos.
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 
                            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    I_PrintBanner(PACKAGE_FULLNAME_HERETIC);
#endif

    I_AtExit(I_ShutdownGraphics, true);
    i_error_title = PACKAGE_FULLNAME_HERETIC;

    I_AtExit(D_Endoom, false);

    //!
    // @category game
    // @vanilla
    //
    // Disable monsters.
    //

    nomonsters = M_ParmExists("-nomonsters");

    //!
    // @category game
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    respawnparm = M_ParmExists("-respawn");

    //!
    // @vanilla
    //
    // Take screenshots when F1 is pressed.
    //

    ravpic = M_ParmExists("-ravpic");

    debugmode = M_ParmExists("-debug");
    startepisode = 1;
    startmap = 1;
    autostart = false;

//
// get skill / episode / map from parms
//

    //!
    // @vanilla
    // @category net
    //
    // Start a deathmatch game.
    //

    if (M_ParmExists("-deathmatch"))
    {
        deathmatch = true;
    }

    //!
    // @category game
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParmWithArgs("-episode", 1);
    if (p)
    {
        startepisode = myargv[p + 1][0] - '0';
        startmap = 1;
        autostart = true;
    }

    //!
    // @category game
    // @arg <x> <y>
    // @vanilla
    //
    // Start a game immediately, warping to level ExMy.
    //

    p = M_CheckParmWithArgs("-warp", 1);
    if (p)
    {
        startepisode = myargv[p + 1][0] - '0';
        // [crispy] only if second argument is not another option
        if (p + 2 < myargc && myargv[p+2][0] != '-')
        {
            startmap = myargv[p + 2][0] - '0';
        }
        else
        {
            // [crispy] allow second digit without space in between for Heretic
            startmap = myargv[p + 1][1] - '0';
        }
        autostart = true;

        // [crispy] if used with -playdemo, fast-forward demo up to the desired map
        demowarp = startmap;
    }

//
// init subsystems
//

    // Check for -CDROM

    cdrom = false;

#ifdef _WIN32

    //!
    // @category obscure
    // @platform windows
    // @vanilla
    //
    // Save configuration data and savegames in c:\heretic.cd,
    // allowing play from CD.
    //

    if (M_CheckParm("-cdrom"))
    {
        cdrom = true;
    }
#endif

    if (cdrom)
    {
        M_SetConfigDir(DEH_String("c:\\heretic.cd"));
    }
    else
    {
        M_SetConfigDir(NULL);
    }

    // Load defaults before initing other systems
    DEH_printf("M_LoadDefaults: Load system defaults.\n");
    D_BindVariables();
    M_SetConfigFilenames(PROGRAM_PREFIX "heretic.ini");
    M_LoadDefaults();

    I_AtExit(M_SaveDefaults, true); // [crispy] always save configuration at exit

    // [JN] Use chosen default skill level.
    startskill = gp_default_skill;

    //!
    // @category game
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParmWithArgs("-skill", 1);
    if (p)
    {
        startskill = myargv[p + 1][0] - '1';
        autostart = true;
    }

    DEH_printf("Z_Init: Init zone memory allocation daemon.\n");
    Z_Init();

    DEH_printf("W_Init: Init WADfiles.\n");

    iwadfile = D_FindIWAD(IWAD_MASK_HERETIC, &gamemission);

    if (iwadfile == NULL)
    {
        I_Error("Game mode indeterminate. No IWAD was found. Try specifying\n"
                "one with the '-iwad' command line parameter.");
    }

    D_AddFile(iwadfile);
    W_CheckCorrectIWAD(heretic);

    //!
    // @category mod
    //
    // Disable auto-loading of .wad files.
    //
    if (!M_ParmExists("-noautoload") && gamemode != shareware)
    {
        char *autoload_dir;
        autoload_dir = M_GetAutoloadDir("heretic.wad");
        if (autoload_dir != NULL)
        {
            DEH_AutoLoadPatches(autoload_dir);
            W_AutoLoadWADs(autoload_dir);
            free(autoload_dir);
        }
    }

    // Load dehacked patches specified on the command line.
    DEH_ParseCommandLine();

    // Load PWAD files.
    W_ParseCommandLine();

    // [crispy] add wad files from autoload PWAD directories
    // [JN] Please do not. No need to create additional directories,
    // consider using "heretic.wad" for autoloading purposes.

    /*
    if (!M_ParmExists("-noautoload") && gamemode != shareware)
    {
        int i;

        for (i = 0; loadparms[i]; i++)
        {
            int p;
            p = M_CheckParmWithArgs(loadparms[i], 1);
            if (p)
            {
                while (++p != myargc && myargv[p][0] != '-')
                {
                    char *autoload_dir;
                    if ((autoload_dir = M_GetAutoloadDir(M_BaseName(myargv[p]))))
                    {
                        W_AutoLoadWADs(autoload_dir);
                        free(autoload_dir);
                    }
                }
            }
        }
    }
    */

    if (W_CheckNumForName("HEHACKED") != -1)
    {
        DEH_LoadLumpByName("HEHACKED", true, true);
    }

    //!
    // @arg <demo>
    // @category demo
    // @vanilla
    //
    // Play back the demo named demo.lmp.
    //

    p = M_CheckParmWithArgs("-playdemo", 1);
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

        // In Vanilla, the filename must be specified without .lmp,
        // but make that optional.
        if (M_StringEndsWith(uc_filename, ".LMP"))
        {
            M_StringCopy(file, myargv[p + 1], sizeof(file));
        }
        else
        {
            DEH_snprintf(file, sizeof(file), "%s.lmp", myargv[p + 1]);
        }

        free(uc_filename);

        if (D_AddFile(file))
        {
            M_StringCopy(demolumpname, lumpinfo[numlumps - 1]->name,
                         sizeof(demolumpname));
        }
        else
        {
            // The file failed to load, but copy the original arg as a
            // demo name to make tricks like -playdemo demo1 possible.
            M_StringCopy(demolumpname, myargv[p + 1], sizeof(demolumpname));
        }

        printf("Playing demo %s.\n", file);
    }

    // Generate the WAD hash table.  Speed things up a bit.
    W_GenerateHashTable();

    // [crispy] process .deh files from PWADs autoload directories
    // [JN] Please do not. No need to create additional directories,
    // consider using "heretic.wad" for autoloading purposes.

    /*
    if (!M_ParmExists("-noautoload") && gamemode != shareware)
    {
        int i;

        for (i = 0; loadparms[i]; i++)
        {
            int p;
            p = M_CheckParmWithArgs(loadparms[i], 1);
            if (p)
            {
                while (++p != myargc && myargv[p][0] != '-')
                {
                    char *autoload_dir;
                    if ((autoload_dir = M_GetAutoloadDir(M_BaseName(myargv[p]))))
                    {
                        DEH_AutoLoadPatches(autoload_dir);
                        free(autoload_dir);
                    }
                }
            }
        }
    }
    */

    //!
    // @category demo
    //
    // Record or playback a demo, automatically quitting
    // after either level exit or player respawn.
    //

    demoextend = (!M_ParmExists("-nodemoextend"));
    //[crispy] make demoextend the default

    if (W_CheckNumForName(DEH_String("E2M1")) == -1)
    {
        gamemode = shareware;
        gamedescription = "Heretic (shareware)";
    }
    else if (W_CheckNumForName("EXTENDED") != -1)
    {
        // Presence of the EXTENDED lump indicates the retail version

        gamemode = retail;
        gamedescription = "Heretic: Shadow of the Serpent Riders";
    }
    else
    {
        gamemode = registered;
        gamedescription = "Heretic (registered)";
    }

    I_SetWindowTitle(gamedescription);

    // [JN] Set the default directory where savegames are saved.
    savegamedir = M_GetSaveGameDir("heretic.wad");

    // [JN] Set the default directory where screenshots are saved.
    M_SetScreenshotDir();

    I_PrintStartupBanner(gamedescription);

    if (M_ParmExists("-testcontrols"))
    {
        startepisode = 1;
        startmap = 1;
        autostart = true;
        testcontrols = true;
    }

    I_InitTimer();
    I_InitSound(heretic);
    I_InitMusic();

    tprintf("NET_Init: Init network subsystem.\n", 1);
    NET_Init ();

    D_ConnectNetGame();

    // haleyjd: removed WATCOMC
    initStartup();

    //
    //  Build status bar line!
    //
    smsg[0] = 0;
    if (deathmatch)
        status(DEH_String("DeathMatch..."));
    if (nomonsters)
        status(DEH_String("No Monsters..."));
    if (respawnparm)
        status(DEH_String("Respawning..."));
    if (autostart)
    {
        char temp[64];
        DEH_snprintf(temp, sizeof(temp),
                     "Warp to Episode %d, Map %d, Skill %d ",
                     startepisode, startmap, startskill + 1);
        status(temp);
    }
    wadprintf();                // print the added wadfiles

    tprintf(DEH_String("MN_Init: Init menu system.\n"), 1);
    MN_Init();

    CT_Init();

    tprintf(DEH_String("R_Init: Init Heretic refresh daemon - ["), 1);
    hprintf(DEH_String("Loading graphics"));
    R_Init();
    tprintf("]\n", 0);

    tprintf(DEH_String("P_Init: Init Playloop state.\n"), 1);
    hprintf(DEH_String("Init game engine."));
    P_Init();
    IncThermo();

    tprintf(DEH_String("I_Init: Setting up machine state.\n"), 1);
    I_CheckIsScreensaver();
    I_InitJoystick();
    IncThermo();

    tprintf(DEH_String("S_Init: Setting up sound.\n"), 1);
    S_Init();
    //IO_StartupTimer();
    S_Start();

    tprintf(DEH_String("D_CheckNetGame: Checking network game status.\n"), 1);
    hprintf(DEH_String("Checking network game status."));
    D_CheckNetGame();
    IncThermo();

    // haleyjd: removed WATCOMC

    tprintf(DEH_String("SB_Init: Loading patches.\n"), 1);
    SB_Init();
    IncThermo();

    tprintf(DEH_String("AM_Init: Loading automap data.\n"), 1);
    AM_Init();

//
// start the appropriate game based on params
//

    //!
    // @category game
    //
    // Start single player game with items spawns as in cooperative netgame.
    //

    p = M_ParmExists("-coop_spawns");
    if (p)
    {
        coop_spawns = true;
    }

    D_CheckRecordFrom();

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
        G_RecordDemo(startskill, 1, startepisode, startmap, myargv[p + 1]);
        D_DoomLoop();           // Never returns
    }

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (p)
    {
        singledemo = true;      // Quit after one demo
        G_DeferedPlayDemo(demolumpname);
        D_DoomLoop();           // Never returns
    }

    // [crispy] we don't play a demo, so don't skip maps
    demowarp = 0;

    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p)
    {
        G_TimeDemo(demolumpname);
        D_DoomLoop();           // Never returns
    }

    //!
    // @category game
    // @arg <s>
    // @vanilla
    //
    // Load the game in savegame slot s.
    //

    p = M_CheckParmWithArgs("-loadgame", 1);
    if (p && p < myargc - 1)
    {
        char *filename;

	filename = SV_Filename(myargv[p + 1][0] - '0');
        G_LoadGame(filename);
	free(filename);
    }

    // Check valid episode and map
    if (autostart || netgame)
    {
        if (!D_ValidEpisodeMap(heretic, gamemode, startepisode, startmap))
        {
            startepisode = 1;
            startmap = 1;
        }
    }

    if (gameaction != ga_loadgame)
    {
        if (autostart || netgame)
        {
            G_InitNew(startskill, startepisode, startmap);
        }
        else
        {
            D_StartTitle();
        }
    }

    finishStartup();

    // [JN] Show startup process time.
    printf("Startup process took %d ms.\n", SDL_GetTicks() - starttime);

    D_DoomLoop();               // Never returns
}
