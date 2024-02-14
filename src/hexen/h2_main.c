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


// HEADER FILES ------------------------------------------------------------

// haleyjd: removed WATCOMC
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "config.h"

#include "doomkeys.h"
#include "h2def.h"
#include "ct_chat.h"
#include "d_iwad.h"
#include "d_mode.h"
#include "m_misc.h"
#include "s_sound.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "net_client.h"
#include "p_local.h"
#include "v_video.h"
#include "w_main.h"
#include "am_map.h"

#include "icon.c"

#include "id_vars.h"
#include "id_func.h"

// MACROS ------------------------------------------------------------------

#define MAXWADFILES 20
#define CT_KEY_BLUE         'b'
#define CT_KEY_RED          'r'
#define CT_KEY_YELLOW       'y'
#define CT_KEY_GREEN        'g'
#define CT_KEY_PLAYER5      'j'     // Jade
#define CT_KEY_PLAYER6      'w'     // White
#define CT_KEY_PLAYER7      'h'     // Hazel
#define CT_KEY_PLAYER8      'p'     // Purple
#define CT_KEY_ALL          't'

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void D_ConnectNetGame(void);
void D_CheckNetGame(void);
boolean F_Responder(event_t * ev);
void I_StartupKeyboard(void);
void I_StartupJoystick(void);
void I_ShutdownKeyboard(void);
void S_InitScript(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void H2_AdvanceDemo(void);
void H2_PageTicker(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void DrawMessage(void);
static void PageDrawer(void);
static void HandleArgs(void);
static void CheckRecordFrom(void);
static void DrawAndBlit(void);
static void CreateSavePath(void);
static void WarpCheck(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------


// PUBLIC DATA DEFINITIONS -------------------------------------------------

GameMode_t gamemode;
static const char *gamedescription;
char *iwadfile;
static char demolumpname[9];    // Demo lump to start playing.
boolean nomonsters;             // checkparm of -nomonsters
boolean respawnparm;            // checkparm of -respawn
boolean randomclass;            // checkparm of -randclass
boolean debugmode;              // checkparm of -debug
boolean ravpic;                 // checkparm of -ravpic
boolean cdrom = false;          // true if cd-rom mode active
boolean cmdfrag;                // true if a CMD_FRAG packet should be sent out
boolean artiskip;               // whether shift-enter skips an artifact
int maxzone = 0x800000;         // Maximum allocated for zone heap (8meg default)
skill_t startskill;
int startepisode;
int startmap;
boolean autostart;
boolean advancedemo;
FILE *debugfile;
int UpdateState;
int maxplayers = MAXPLAYERS;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int WarpMap;
static int demosequence;
static int pagetic;
static const char *pagename;
static char *SavePathConfig;

// CODE --------------------------------------------------------------------


static const char * const chat_macro_defaults[10] =
{
    HUSTR_CHATMACRO0,
    HUSTR_CHATMACRO1,
    HUSTR_CHATMACRO2,
    HUSTR_CHATMACRO3,
    HUSTR_CHATMACRO4,
    HUSTR_CHATMACRO5,
    HUSTR_CHATMACRO6,
    HUSTR_CHATMACRO7,
    HUSTR_CHATMACRO8,
    HUSTR_CHATMACRO9,
};


void D_BindVariables(void)
{
    int i;

    I_BindInputVariables();
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindControls();
    M_BindHereticControls();
    M_BindHexenControls();
    M_BindChatControls(MAXPLAYERS);

    key_multi_msgplayer[0] = CT_KEY_BLUE;
    key_multi_msgplayer[1] = CT_KEY_RED;
    key_multi_msgplayer[2] = CT_KEY_YELLOW;
    key_multi_msgplayer[3] = CT_KEY_GREEN;
    key_multi_msgplayer[4] = CT_KEY_PLAYER5;
    key_multi_msgplayer[5] = CT_KEY_PLAYER6;
    key_multi_msgplayer[6] = CT_KEY_PLAYER7;
    key_multi_msgplayer[7] = CT_KEY_PLAYER8;

    NET_BindVariables();

    M_BindIntVariable("mouse_sensitivity",      &mouseSensitivity);
    M_BindIntVariable("sfx_volume",             &snd_MaxVolume);
    M_BindIntVariable("music_volume",           &snd_MusicVolume);

    M_BindStringVariable("savedir", &SavePathConfig);

    // Multiplayer chat macros

    for (i=0; i<10; ++i)
    {
        char buf[12];

        chat_macros[i] = M_StringDuplicate(chat_macro_defaults[i]);
        M_snprintf(buf, sizeof(buf), "chatmacro%i", i);
        M_BindStringVariable(buf, &chat_macros[i]);
    }

	// [JN] Bind ID-specific config variables.
	ID_BindVariables(hexen);
}

// Set the default directory where hub savegames are saved.

static void D_SetDefaultSavePath(void)
{
    SavePath = M_GetSaveGameDir("hexen.wad");

    if (!strcmp(SavePath, ""))
    {
        // only get hexen.cfg path if one is not already found

        if (SavePathConfig == NULL || !strcmp(SavePathConfig, ""))
        {
            // If we are not using a savegame path (probably because we are on
            // Windows and not using a config dir), behave like Vanilla Hexen
            // and use hexndata/:

            SavePath = malloc(10);
            M_snprintf(SavePath, 10, "hexndata%c", DIR_SEPARATOR);
        }
        else
        {
            SavePath = M_StringDuplicate(SavePathConfig);
        }
    }

    // only set hexen.cfg path if using default handling

    if (!M_ParmExists("-savedir") && !M_ParmExists("-cdrom"))
    {
        SavePathConfig = SavePath;
    }
}

// The Mac version of the Hexen IWAD is different to the "normal" DOS
// version - it doesn't include lumps used by the DOS DMX library.
// This means that we can't do GUS or OPL emulation and need to apply
// a workaround.
static void AdjustForMacIWAD(void)
{
    boolean adjust_music = false;

    switch (snd_musicdevice)
    {
        case SNDDEVICE_ADLIB:
        case SNDDEVICE_SB:
            adjust_music = W_CheckNumForName("GENMIDI") < 0;
            break;

        case SNDDEVICE_GUS:
            adjust_music = W_CheckNumForName("DMXGUS") < 0;
            break;

        default:
            break;
    }

    if (adjust_music)
    {
        printf("** Note: You appear to be using the Mac version of the Hexen\n"
               "** IWAD file. This is missing the lumps required for OPL or\n"
               "** GUS emulation. Your music configuration is being adjusted\n"
               "** to a different setting that won't cause the game to "
               "crash.\n");
        snd_musicdevice = SNDDEVICE_GENMIDI;
    }
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

static boolean D_GrabMouseCallback(void)
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

    return (gamestate == GS_LEVEL) && !advancedemo && !demoplayback;
}

// Message displayed when quitting Hexen

static void D_HexenQuitMessage(void)
{
    printf("\nHexen: Beyond Heretic\n");
}

static void D_AddFile(char *filename)
{
    printf("  adding %s\n", filename);

    W_AddFile(filename);
}

// Find out what version of Hexen is playing.

void D_IdentifyVersion(void)
{
    // The Hexen Shareware, ne 4 Level Demo Version, is missing the SKY1 lump
    // and uses the SKY2 lump instead. Let's use this fact and the missing
    // levels from MAP05 onward to identify it and set gamemode accordingly.

    if (W_CheckNumForName("SKY1") == -1 &&
        W_CheckNumForName("MAP05") == -1 )
    {
	gamemode = shareware;
	maxplayers = 4;
    }

    // The v1.0 IWAD file is missing a bunch of lumps that can cause the game
    // to crash, so we exit with an error if the user tries to play with it.
    // But we provide an override command line flag if they really want to
    // do it.

    //!
    // If provided, the check for the v1.0 IWAD file is disabled, even though
    // it will almost certainly cause the game to crash.
    //
    // @category compat
    //

    if (!M_ParmExists("-v10override")
     && gamemode != shareware && W_CheckNumForName("CLUS1MSG") < 0)
    {
        I_Error(
            "You are trying to use the Hexen v1.0 IWAD. This isn't\n"
            "supported by " PACKAGE_NAME ". Please upgrade to the v1.1\n"
            "IWAD file. See here for more information:\n"
            "  https://www.doomworld.com/classicdoom/info/patches.php");
    }
}

// Set the gamedescription string.

void D_SetGameDescription(void)
{
/*
    NB: The 4 Level Demo Version actually prints a four-lined banner
    (and indeed waits for a keypress):

    Hexen:  Beyond Heretic

    4 Level Demo Version
    Press any key to continue.
*/

    if (gamemode == shareware)
    {
	gamedescription = "Hexen: 4 Level Demo Version";
    }
    else
    {
	gamedescription = "Hexen";
    }
}

//==========================================================================
//
// H2_Main
//
//==========================================================================
void InitMapMusicInfo(void);

void D_DoomMain(void)
{
    GameMission_t gamemission;
    int p;
    const int starttime = SDL_GetTicks();

    I_AtExit(D_HexenQuitMessage, false);
    startepisode = 1;
    autostart = false;
    startmap = 1;
    gamemode = commercial;

#ifdef _WIN32
    // [JN] Print colorized title.
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_RED
                           | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
                           | FOREGROUND_INTENSITY);

    for (p = 0 ; p < 26 ; p++) printf(" ");
    printf(PACKAGE_FULLNAME_HEXEN);
    for (p = 0 ; p < 37 ; p++) printf(" ");
    printf("\n");

    // [JN] Fallback to standard console colos.
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 
                            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    I_PrintBanner(PACKAGE_FULLNAME_HEXEN);
#endif

    I_AtExit(I_ShutdownGraphics, true);

#ifdef _WIN32
    // [JN] Pressing PrintScreen on Windows 11 is opening Snipping Tool.
    // Re-register PrintScreen key pressing for port needs to avoid this.
    // Taken from DOOM Retro.
    if (key_menu_screenshot == KEY_PRTSCR)
    {
        RegisterHotKey(NULL, 1, MOD_ALT, VK_SNAPSHOT);
        RegisterHotKey(NULL, 2, 0, VK_SNAPSHOT);
    }

    //!
    // @category obscure
    // @platform windows
    // @vanilla
    //
    // Save configuration data and savegames in c:\hexndata,
    // allowing play from CD.
    //

    cdrom = M_ParmExists("-cdrom");
#endif

    if (cdrom)
    {
        M_SetConfigDir("c:\\hexndata\\");
    }
    else
    {
        M_SetConfigDir(NULL);
    }

    // [JN] Set screeenshot files dir.
    M_SetScreenshotDir();

    // Load defaults before initing other systems
    ST_Message("M_LoadDefaults: Load system defaults.\n");
    D_BindVariables();
    M_SetConfigFilenames(PROGRAM_PREFIX "hexen.ini");
    M_LoadDefaults();

    D_SetDefaultSavePath();

    I_AtExit(M_SaveDefaults, true); // [crispy] always save configuration at exit

    // [JN] Use choosen default skill level.
    startskill = gp_default_skill;

    // Now that the savedir is loaded, make sure it exists
    CreateSavePath();

    ST_Message("Z_Init: Init zone memory allocation daemon.\n");
    Z_Init();

    // haleyjd: removed WATCOMC

    ST_Message("W_Init: Init WADfiles.\n");

    iwadfile = D_FindIWAD(IWAD_MASK_HEXEN, &gamemission);

    if (iwadfile == NULL)
    {
        I_Error("Game mode indeterminate. No IWAD was found. Try specifying\n"
                "one with the '-iwad' command line parameter.");
    }

    D_AddFile(iwadfile);
    W_CheckCorrectIWAD(hexen);
    D_IdentifyVersion();
    D_SetGameDescription();
    AdjustForMacIWAD();

    //!
    // @category mod
    //
    // Disable auto-loading of .wad files.
    //
    if (!M_ParmExists("-noautoload") && gamemode != shareware)
    {
        char *autoload_dir;
        autoload_dir = M_GetAutoloadDir("hexen.wad");
        if (autoload_dir != NULL)
        {
            // TODO? DEH_AutoLoadPatches(autoload_dir);
            W_AutoLoadWADs(autoload_dir);
            free(autoload_dir);
        }
    }

    HandleArgs();

    // Generate the WAD hash table.  Speed things up a bit.
    W_GenerateHashTable();

    I_PrintStartupBanner(gamedescription);

    ST_Message("MN_Init: Init menu system.\n");
    MN_Init();

    ST_Message("CT_Init: Init chat mode data.\n");
    CT_Init();

    InitMapMusicInfo();         // Init music fields in mapinfo

    ST_Message("S_InitScript\n");
    S_InitScript();

    ST_Message("SN_InitSequenceScript: Registering sound sequences.\n");
    SN_InitSequenceScript();
    ST_Message("I_Init: Setting up machine state.\n");
    I_CheckIsScreensaver();
    I_InitTimer();
    I_InitJoystick();
    I_InitSound(hexen);
    I_InitMusic();

    ST_Message("NET_Init: Init networking subsystem.\n");
    NET_Init();
    D_ConnectNetGame();

    S_Init();
    S_Start();

    ST_Message("ST_Init: Init startup screen.\n");
    ST_Init();

    // Show version message now, so it's visible during R_Init()
    ST_Message("R_Init: Init Hexen refresh daemon");
    R_Init();
    ST_Message("\n");

    //if (M_CheckParm("-net"))
    //    ST_NetProgress();       // Console player found

    ST_Message("P_Init: Init Playloop state.\n");
    P_Init();

    // Check for command line warping. Follows P_Init() because the
    // MAPINFO.TXT script must be already processed.
    WarpCheck();

    ST_Message("D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame();

    ST_Message("SB_Init: Loading patches.\n");
    SB_Init();

    ST_Done();

    ST_Message("AM_Init: Loading automap data.\n");
    AM_Init();

    if (autostart)
    {
        ST_Message("Warp to Map %d (\"%s\":%d), Skill %d\n",
                   WarpMap, P_GetMapName(startmap), startmap, startskill + 1);
    }

    CheckRecordFrom();

    //!
    // @arg <x>
    // @category demo
    // @vanilla
    //
    // Record a demo named x.lmp.
    //

    p = M_CheckParm("-record");
    if (p && p < myargc - 1)
    {
        G_RecordDemo(startskill, 1, startepisode, startmap, myargv[p + 1]);
        H2_GameLoop();          // Never returns
    }

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (p)
    {
        singledemo = true;      // Quit after one demo
        G_DeferedPlayDemo(demolumpname);
        H2_GameLoop();          // Never returns
    }

    // [crispy] we don't play a demo, so don't skip maps
    demowarp = 0;

    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p)
    {
        G_TimeDemo(demolumpname);
        H2_GameLoop();          // Never returns
    }

    //!
    // @category game
    // @arg <s>
    // @vanilla
    //
    // Load the game in savegame slot s.
    //

    p = M_CheckParmWithArgs("-loadgame", 1);
    if (p)
    {
        G_LoadGame(atoi(myargv[p + 1]));
    }

    if (gameaction != ga_loadgame)
    {
        if (autostart || netgame)
        {
            G_StartNewInit();
            G_InitNew(startskill, startepisode, startmap);
        }
        else
        {
            H2_StartTitle();
        }
    }

    // [JN] Show startup process time.
    printf("Startup process took %d ms.\n", SDL_GetTicks() - starttime);

    H2_GameLoop();              // Never returns
}

//==========================================================================
//
// HandleArgs
//
//==========================================================================

static void HandleArgs(void)
{
    int p;

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
    // @category net
    //
    // In deathmatch mode, change a player's class each time the
    // player respawns.
    //

    randomclass = M_ParmExists("-randclass");

    //!
    // @vanilla
    //
    // Take screenshots when F1 is pressed.
    //

    ravpic = M_ParmExists("-ravpic");

    //!
    // @category obscure
    // @vanilla
    //
    // Don't allow artifacts to be used when the run key is held down.
    //

    artiskip = M_ParmExists("-artiskip");

    debugmode = M_ParmExists("-debug");

    //!
    // @vanilla
    // @category net
    //
    // Start a deathmatch game.
    //

    deathmatch = M_ParmExists("-deathmatch");

    // currently broken or unused:
    cmdfrag = M_ParmExists("-cmdfrag");

    // Check WAD file command line options
    W_ParseCommandLine();

    //!
    // @category obscure
    // @vanilla
    // @arg <path>
    //
    // Development option to specify path to level scripts.
    //

    p = M_CheckParmWithArgs("-scripts", 1);

    if (p)
    {
        sc_FileScripts = true;
        sc_ScriptsDir = myargv[p+1];
    }

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
        startskill = myargv[p+1][0] - '1';
        autostart = true;
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
        char *uc_filename;
        char file[256];

        M_StringCopy(file, myargv[p+1], sizeof(file));

        // With Vanilla Hexen you have to specify the file without
        // extension, but make that optional.
        uc_filename = strdup(myargv[p + 1]);
        M_ForceUppercase(uc_filename);

        if (!M_StringEndsWith(uc_filename, ".LMP"))
        {
            M_StringConcat(file, ".lmp", sizeof(file));
        }

        free(uc_filename);

        if (W_AddFile(file) != NULL)
        {
            M_StringCopy(demolumpname, lumpinfo[numlumps - 1]->name,
                         sizeof(demolumpname));
        }
        else
        {
            // The file failed to load, but copy the original arg as a
            // demo name to make tricks like -playdemo demo1 possible.
            M_StringCopy(demolumpname, myargv[p+1], sizeof(demolumpname));
        }

        ST_Message("Playing demo %s.\n", myargv[p+1]);
    }

    //!
    // @category demo
    //
    // Record or playback a demo, automatically quitting
    // after either level exit or player respawn.
    //

    demoextend = (!M_ParmExists("-nodemoextend"));
    //[crispy] make demoextend the default

    if (M_ParmExists("-testcontrols"))
    {
        autostart = true;
        testcontrols = true;
    }
}

//==========================================================================
//
// WarpCheck
//
//==========================================================================

static void WarpCheck(void)
{
    int p;
    int map;

    //!
    // @category game
    // @arg x
    // @vanilla
    //
    // Start a game immediately, warping to MAPx.
    //

    p = M_CheckParm("-warp");
    if (p && p < myargc - 1)
    {
        WarpMap = atoi(myargv[p + 1]);
        map = P_TranslateMap(WarpMap);
        if (map == -1)
        {                       // Couldn't find real map number
            startmap = 1;
            ST_Message("-WARP: Invalid map number.\n");
        }
        else
        {                       // Found a valid startmap
            startmap = map;
            autostart = true;

            // [crispy] if used with -playdemo, fast-forward demo up to the desired map
            demowarp = startmap;
        }
    }
    else
    {
        WarpMap = 1;
        startmap = P_TranslateMap(1);
        if (startmap == -1)
        {
            startmap = 1;
        }
    }
}

//==========================================================================
//
// H2_GameLoop
//
//==========================================================================

void H2_GameLoop(void)
{
    if (M_CheckParm("-debugfile"))
    {
        char filename[20];
        M_snprintf(filename, sizeof(filename), "debug%i.txt", consoleplayer);
        debugfile = M_fopen(filename, "w");
    }
    I_SetWindowTitle(gamedescription);
    I_GraphicsCheckCommandLine();
    I_SetGrabMouseCallback(D_GrabMouseCallback);
    I_RegisterWindowIcon(hexen_data, hexen_w, hexen_h);
    I_InitGraphics();

    while (1)
    {
        // Frame syncronous IO operations
        I_StartFrame();

        // Process one or more tics
        // Will run at least one tic
        TryRunTics();

        if (oldgametic < gametic)
        {
            // [JN] Mute and restore sound and music volume.
            if (snd_mute_inactive && volume_needs_update)
            {
                S_MuteUnmuteSound (!window_focused);
            }

            // Move positional sounds
            S_UpdateSounds(players[displayplayer].mo);
            oldgametic = gametic;
        }

        // Update display, next frame, with current state.
        if (screenvisible)
        {
            DrawAndBlit();
        }
    }
}

//==========================================================================
//
// H2_ProcessEvents
//
// Send all the events of the given timestamp down the responder chain.
//
//==========================================================================

void H2_ProcessEvents(void)
{
    event_t *ev;

    for (;;)
    {
        ev = D_PopEvent();

        if (ev == NULL)
        {
            break;
        }

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

//==========================================================================
//
// DrawAndBlit
//
//==========================================================================

static void DrawAndBlit(void)
{
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

    // Change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize();
        // Force background redraw
        oldgamestate = -1;
    }

    // Do buffered drawing
    switch (gamestate)
    {
        case GS_LEVEL:
            if (!gametic)
            {
                break;
            }
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
            if (widget_levelname)
            {
                AM_LevelNameDrawer();
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

            CT_Drawer();

            // [JN] Main status bar drawing function.
            if (dp_screen_size < 13 || (automapactive && !automap_overlay))
            {
                SB_Drawer();
            }

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

            break;
        case GS_INTERMISSION:
            IN_Drawer();
            break;
        case GS_FINALE:
            F_Drawer();
            break;
        case GS_DEMOSCREEN:
            PageDrawer();
            break;
    }

    // [JN] Right widgets are not available while finale screens.
    if (dp_screen_size < 13 && gamestate != GS_FINALE)
    {
        ID_RightWidgets();
    }

    if (testcontrols)
    {
        V_DrawMouseSpeedBox(testcontrols_mousespeed);
    }

    if (paused && !MenuActive && !askforquit)
    {
        if (!netgame)
        {
            V_DrawShadowedPatchOptional(160, (viewwindowy / vid_resolution) + 5, 1, W_CacheLumpName("PAUSED",
                                                              PU_CACHE));
        }
        else
        {
            V_DrawShadowedPatchOptional(160, 70, 1, W_CacheLumpName("PAUSED", PU_CACHE));
        }
    }

    // Draw current message
    DrawMessage();

    // Draw Menu
    MN_Drawer();

    // Send out any new accumulation
    NetUpdate();

    // Flush buffered stuff to screen
    I_FinishUpdate();
}

//==========================================================================
//
// DrawMessage
//
//==========================================================================

static void DrawMessage(void)
{
    player_t *player;

    player = &players[consoleplayer];
    if (player->messageTics <= 0)
    {                           // No message
        return;
    }
    if (player->yellowMessage)
    {
        MN_DrTextAYellow(player->message,
                         160 - MN_TextAWidth(player->message) / 2, 1);
    }
    else
    {
        MN_DrTextA(player->message, 160 - MN_TextAWidth(player->message) / 2,
                   1, NULL);
    }
}

int right_widget_w, right_widget_h; // [crispy]
//==========================================================================
//
// H2_PageTicker
//
//==========================================================================

void H2_PageTicker(void)
{
    if (--pagetic < 0)
    {
        H2_AdvanceDemo();
    }
}

//==========================================================================
//
// PageDrawer
//
//==========================================================================

static void PageDrawer(void)
{
    V_DrawFullscreenRawOrPatch(W_GetNumForName(pagename)); // [crispy]

    if (demosequence == 1)
    {
        V_DrawPatch(4, 160, W_CacheLumpName("ADVISOR", PU_CACHE));
    }
}

//==========================================================================
//
// H2_AdvanceDemo
//
// Called after each demo or intro demosequence finishes.
//
//==========================================================================

void H2_AdvanceDemo(void)
{
    advancedemo = true;
}

//==========================================================================
//
// H2_DoAdvanceDemo
//
//==========================================================================

void H2_DoAdvanceDemo(void)
{
    static boolean mus_played = false;
    players[consoleplayer].playerstate = PST_LIVE;      // don't reborn
    advancedemo = false;
    usergame = false;           // can't save/end game here
    paused = false;
    gameaction = ga_nothing;
    demosequence = (demosequence + 1) % 7;
    switch (demosequence)
    {
        case 0:
            pagetic = 280;
            gamestate = GS_DEMOSCREEN;
            pagename = "TITLE";
            // [JN] Slightly modified logics for intro music:
            // - replay only if internal demos are on
            // - disallow playing again if internal demos are off
            if (!mus_played)
            {
                S_StartSongName("hexen", demo_internal);
            }
            mus_played = !demo_internal;
            break;
        case 1:
            pagetic = 210;
            gamestate = GS_DEMOSCREEN;
            pagename = "TITLE";
            break;
        case 2:
            if (demo_internal)
            {
                G_DeferedPlayDemo("demo1");
            }
            break;
        case 3:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            pagename = "CREDIT";
            break;
        case 4:
            if (demo_internal)
            {
                G_DeferedPlayDemo("demo2");
            }
            break;
        case 5:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            pagename = "CREDIT";
            break;
        case 6:
            if (demo_internal)
            {
                G_DeferedPlayDemo("demo3");
            }
            break;
    }
}

//==========================================================================
//
// H2_StartTitle
//
//==========================================================================

void H2_StartTitle(void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    H2_AdvanceDemo();
}

//==========================================================================
//
// CheckRecordFrom
//
// -recordfrom <savegame num> <demoname>
//
//==========================================================================

static void CheckRecordFrom(void)
{
    int p;

    //!
    // @vanilla
    // @category demo
    // @arg <savenum> <demofile>
    //
    // Record a demo, loading from the given filename. Equivalent
    // to -loadgame <savenum> -record <demofile>.
    //
    p = M_CheckParm("-recordfrom");
    if (!p || p > myargc - 2)
    {                           // Bad args
        return;
    }
    G_LoadGame(atoi(myargv[p + 1]));
    G_DoLoadGame();             // Load the gameskill etc info from savegame
    G_RecordDemo(gameskill, 1, gameepisode, gamemap, myargv[p + 2]);

    H2_GameLoop();              // Never returns
}

// haleyjd: removed WATCOMC
/*
void CleanExit(void)
{
	union REGS regs;

	I_ShutdownKeyboard();
	regs.x.eax = 0x3;
	int386(0x10, &regs, &regs);
	printf("Exited from HEXEN: Beyond Heretic.\n");
	exit(1);
}
*/

//==========================================================================
//
// CreateSavePath
//
//==========================================================================

static void CreateSavePath(void)
{
    M_MakeDirectory(SavePath);
}
