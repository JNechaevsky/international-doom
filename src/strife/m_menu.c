//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//


#include <stdlib.h>
#include <ctype.h>


#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"

#include "d_main.h"
#include "deh_main.h"

#include "i_input.h"
#include "i_joystick.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_diskicon.h"
#include "v_trans.h"
#include "v_video.h"
#include "w_wad.h"

#include "p_local.h"
#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_saves.h"    // [STRIFE]
#include "p_saveg.h"

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"
#include "p_dialog.h"


void M_QuitStrife(int);


//
// defaulted values
//
//int			mouseSensitivity = 5;

// [STRIFE]: removed this entirely
// Show messages has default, 0 = off, 1 = on
//int			showMessages = 1;
	

// Blocky mode, has default, 0 = high, 1 = normal
int			detailLevel = 0;
int			screenblocks = 10; // [STRIFE] default 10, not 9

// temp for screenblocks (0-9)
int			screenSize;

// -1 = no quicksave slot picked!
int			quickSaveSlot;

 // 1 = message to be printed
int			messageToPrint;
// ...and here is the message string!
const char		*messageString;

// message x & y
int			messx;
int			messy;
int			messageLastMenuActive;

// timed message = no input from user
boolean			messageNeedsInput;

void    (*messageRoutine)(int response);

static char *gammalvls[MAXGAMMA][2] =
{
    { GAMMALVL_N050,  "-0.50" },
    { GAMMALVL_N055,  "-0.55" },
    { GAMMALVL_N060,  "-0.60" },
    { GAMMALVL_N065,  "-0.65" },
    { GAMMALVL_N070,  "-0.70" },
    { GAMMALVL_N075,  "-0.75" },
    { GAMMALVL_N080,  "-0.80" },
    { GAMMALVL_N085,  "-0.85" },
    { GAMMALVL_N090,  "-0.90" },
    { GAMMALVL_N095,  "-0.95" },
    { GAMMALVL_OFF,   "OFF"   },
    { GAMMALVL_010,   "0.1"   },
    { GAMMALVL_020,   "0.2"   },
    { GAMMALVL_030,   "0.3"   },
    { GAMMALVL_040,   "0.4"   },
    { GAMMALVL_050,   "0.5"   },
    { GAMMALVL_060,   "0.6"   },
    { GAMMALVL_070,   "0.7"   },
    { GAMMALVL_080,   "0.8"   },
    { GAMMALVL_090,   "0.9"   },
    { GAMMALVL_100,   "1.0"   },
    { GAMMALVL_110,   "1.1"   },
    { GAMMALVL_120,   "1.2"   },
    { GAMMALVL_130,   "1.3"   },
    { GAMMALVL_140,   "1.4"   },
    { GAMMALVL_150,   "1.5"   },
    { GAMMALVL_160,   "1.6"   },
    { GAMMALVL_170,   "1.7"   },
    { GAMMALVL_180,   "1.8"   },
    { GAMMALVL_190,   "1.9"   },
    { GAMMALVL_200,   "2.0"   },
    { GAMMALVL_220,   "2.2"   },
    { GAMMALVL_240,   "2.4"   },
    { GAMMALVL_260,   "2.6"   },
    { GAMMALVL_280,   "2.8"   },
    { GAMMALVL_300,   "3.0"   },
    { GAMMALVL_320,   "3.2"   },
    { GAMMALVL_340,   "3.4"   },
    { GAMMALVL_360,   "3.6"   },
    { GAMMALVL_380,   "3.8"   },
    { GAMMALVL_400,   "4.0"   },
};

// we are going to be entering a savegame string
int			saveStringEnter;              
int             	saveSlot;	// which slot to save in
int			saveCharIndex;	// which char we're editing
// old save description before edit
char			saveOldString[SAVESTRINGSIZE];  

boolean                 inhelpscreens;
boolean                 menuactive;
boolean                 menupause;      // haleyjd 08/29/10: [STRIFE] New global
int                     menupausetime;  // haleyjd 09/04/10: [STRIFE] New global
boolean                 menuindialog;   // haleyjd 09/04/10: ditto

// haleyjd 08/27/10: [STRIFE] SKULLXOFF == -28, LINEHEIGHT == 19
#define CURSORXOFF		-28
#define LINEHEIGHT		19

char			savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];

// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

// haleyjd 09/04/10: [STRIFE] Moved menuitem / menu structures into header
// because they are needed externally by the dialog engine.

// haleyjd 08/27/10: [STRIFE] skull* stuff changed to cursor* stuff
short		itemOn;			// menu item skull is on
short		cursorAnimCounter;	// skull animation counter
short		whichCursor;		// which skull to draw

// graphic name of cursors
// haleyjd 08/27/10: [STRIFE] M_SKULL* -> M_CURS*
const char *cursorName[8] = {"M_CURS1", "M_CURS2", "M_CURS3", "M_CURS4",
                             "M_CURS5", "M_CURS6", "M_CURS7", "M_CURS8" };

// haleyjd 20110210 [STRIFE]: skill level for menus
int menuskill;

// current menudef
menu_t*	currentMenu;                          

// haleyjd 03/01/13: [STRIFE] v1.31-only:
// Keeps track of whether the save game menu is being used to name a new
// character slot, or to just save the current game. In the v1.31 disassembly
// this was the new dword_8632C variable.
boolean namingCharacter; 

// =============================================================================
// PROTOTYPES
// =============================================================================

void M_NewGame(int choice);
void M_Episode(int choice);
void M_ChooseSkill(int choice);
void M_LoadGame(int choice);
void M_SaveGame(int choice);
void M_Options(int choice);
void M_EndGame(int choice);
void M_ReadThis(int choice);
void M_ReadThis2(int choice);
void M_ReadThis3(int choice); // [STRIFE]

static void M_Choose_ID_Main (int choice);
static menu_t ID_Def_Main;

//void M_ChangeMessages(int choice); [STRIFE]
void M_ChangeSensitivity(int choice);
void M_SfxVol(int choice);
void M_VoiceVol(int choice); // [STRIFE]
void M_MusicVol(int choice);
void M_SizeDisplay(int choice);
void M_StartGame(int choice);
void M_Sound(int choice);

//void M_FinishReadThis(int choice); - [STRIFE] unused
void M_SaveSelect(int choice);
void M_ReadSaveStrings(void);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);
void M_DrawReadThis3(void); // [STRIFE]
void M_DrawNewGame(void);
void M_DrawEpisode(void);
void M_DrawOptions(void);
void M_DrawSound(void);
void M_DrawLoad(void);
void M_DrawSave(void);

void M_DrawSaveLoadBorder(int x,int y);
void M_SetupNextMenu(menu_t *menudef);
void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
void M_DrawEmptyCell(menu_t *menu,int item);
void M_DrawSelCell(menu_t *menu,int item);
int  M_StringWidth(const char *string);
int  M_StringHeight(const char *string);
void M_StartMessage(const char *string,void *routine,boolean input);
void M_StopMessage(void);




//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

menuitem_t MainMenu[]=
{
    {M_SWTC,"M_NGAME",M_NewGame,'n'},
    {M_SWTC,"M_OPTION",M_Choose_ID_Main,'o'},
    {M_SWTC,"M_LOADG",M_LoadGame,'l'},
    {M_SWTC,"M_SAVEG",M_SaveGame,'s'},
    // Another hickup with Special edition.
    {M_SWTC,"M_RDTHIS",M_ReadThis,'h'}, // haleyjd 08/28/10: 'r' -> 'h'
    {M_SWTC,"M_QUITG",M_QuitStrife,'q'}
};

menu_t  MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    97,45, // haleyjd 08/28/10: [STRIFE] changed y coord
    0,
    false, false, false,
};


//
// EPISODE SELECT
//
/*
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

menuitem_t EpisodeMenu[]=
{
    {1,"M_EPI1", M_Episode,'k'},
    {1,"M_EPI2", M_Episode,'t'},
    {1,"M_EPI3", M_Episode,'i'},
    {1,"M_EPI4", M_Episode,'t'}
};

menu_t  EpiDef =
{
    ep_end,		// # of menu items
    &MainDef,		// previous menu
    EpisodeMenu,	// menuitem_t ->
    M_DrawEpisode,	// drawing routine ->
    48,63,              // x,y
    ep1			// lastOn
};
*/

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

menuitem_t NewGameMenu[]=
{
    // haleyjd 08/28/10: [STRIFE] changed all shortcut letters
    {M_SWTC,"M_JKILL",   M_ChooseSkill, 't'},
    {M_SWTC,"M_ROUGH",   M_ChooseSkill, 'r'},
    {M_SWTC,"M_HURT",    M_ChooseSkill, 'v'},
    {M_SWTC,"M_ULTRA",   M_ChooseSkill, 'e'},
    {M_SWTC,"M_NMARE",   M_ChooseSkill, 'b'}
};

menu_t  NewDef =
{
    newg_end,           // # of menu items
    &MainDef,           // previous menu - haleyjd [STRIFE] changed to MainDef
    NewGameMenu,        // menuitem_t ->
    M_DrawNewGame,      // drawing routine ->
    48,63,              // x,y
    toorough,           // lastOn - haleyjd [STRIFE]: default to skill 1
    false, false, false,
};

//
// OPTIONS MENU
//
enum
{
    // haleyjd 08/28/10: [STRIFE] Removed messages, mouse sens., detail.
    endgame,
    scrnsize,
    option_empty1,
    soundvol,
    opt_end
} options_e;

menuitem_t OptionsMenu[]=
{
    // haleyjd 08/28/10: [STRIFE] Removed messages, mouse sens., detail.
    {M_SWTC,"M_ENDGAM",	M_EndGame,'e'},
    {M_LFRT,"M_SCRNSZ",	M_SizeDisplay,'s'},
    {M_SKIP,"",0,'\0'},
    {M_SWTC,"M_SVOL",	M_Sound,'s'}
};

menu_t  OptionsDef =
{
    opt_end,
    &MainDef,
    OptionsMenu,
    M_DrawOptions,
    60,37,
    0,
    false, false, false,
};

//
// Read This! MENU 1 & 2 & [STRIFE] 3
//
enum
{
    rdthsempty1,
    read1_end
} read_e;

menuitem_t ReadMenu1[] =
{
    {M_SWTC,"",M_ReadThis2,0}
};

menu_t  ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,185,
    0,
    false, false, false,
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

menuitem_t ReadMenu2[]=
{
    {M_SWTC,"",M_ReadThis3,0} // haleyjd 08/28/10: [STRIFE] Go to ReadThis3
};

menu_t  ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    250,185, // haleyjd 08/28/10: [STRIFE] changed coords
    0,
    false, false, false,
};

// haleyjd 08/28/10: Added Read This! menu 3
enum
{
    rdthsempty3,
    read3_end
} read_e3;

menuitem_t ReadMenu3[]=
{
    {M_SWTC,"",M_ClearMenus,0}
};

menu_t  ReadDef3 =
{
    read3_end,
    &ReadDef2,
    ReadMenu3,
    M_DrawReadThis3,
    250, 185,
    0,
    false, false, false,
};

//
// SOUND VOLUME MENU
//
enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    voice_vol,
    sfx_empty3,
    sfx_mouse,
    sfx_empty4,
    sound_end
} sound_e;

// haleyjd 08/29/10:
// [STRIFE] 
// * Added voice volume
// * Moved mouse sensitivity here (who knows why...)
menuitem_t SoundMenu[]=
{
    {M_LFRT,"M_SFXVOL",M_SfxVol,'s'},
    {M_SKIP,"",0,'\0'},
    {M_LFRT,"M_MUSVOL",M_MusicVol,'m'},
    {M_SKIP,"",0,'\0'},
    {M_LFRT,"M_VOIVOL",M_VoiceVol,'v'}, 
    {M_SKIP,"",0,'\0'},
    {M_LFRT,"M_MSENS",M_ChangeSensitivity,'m'},
    {M_SKIP,"",0,'\0'}
};

menu_t  SoundDef =
{
    sound_end,
    &OptionsDef,
    SoundMenu,
    M_DrawSound,
    80,35,       // [STRIFE] changed y coord 64 -> 35
    0,
    false, false, false,
};

//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
} load_e;

menuitem_t LoadMenu[]=
{
    {M_SWTC,"", M_LoadSelect,'1'},
    {M_SWTC,"", M_LoadSelect,'2'},
    {M_SWTC,"", M_LoadSelect,'3'},
    {M_SWTC,"", M_LoadSelect,'4'},
    {M_SWTC,"", M_LoadSelect,'5'},
    {M_SWTC,"", M_LoadSelect,'6'}
};

menu_t  LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    80,54,
    0,
    false, false, false,
};

//
// SAVE GAME MENU
//
menuitem_t SaveMenu[]=
{
    {M_SWTC,"", M_SaveSelect,'1'},
    {M_SWTC,"", M_SaveSelect,'2'},
    {M_SWTC,"", M_SaveSelect,'3'},
    {M_SWTC,"", M_SaveSelect,'4'},
    {M_SWTC,"", M_SaveSelect,'5'},
    {M_SWTC,"", M_SaveSelect,'6'}
};

menu_t  SaveDef =
{
    load_end,
    &MainDef,
    SaveMenu,
    M_DrawSave,
    80,54,
    0,
    false, false, false,
};

void M_DrawNameChar(void);

//
// NAME CHARACTER MENU
//
// [STRIFE]
// haleyjd 20110210: New "Name Your Character" Menu
//
menu_t NameCharDef =
{
    load_end,
    &NewDef,
    SaveMenu,
    M_DrawNameChar,
    80,54,
    0,
    false, false, false,
};


// =============================================================================
// [JN] Custom ID menu
// =============================================================================

#define ID_MENU_TOPOFFSET         (18)
#define ID_MENU_LEFTOFFSET        (48)
#define ID_MENU_LEFTOFFSET_SML    (93)
#define ID_MENU_LEFTOFFSET_MID    (64)
#define ID_MENU_LEFTOFFSET_BIG    (32)
#define ID_MENU_LEFTOFFSET_LEVEL  (74)

#define ID_MENU_LINEHEIGHT_SMALL  (9)
#define ID_MENU_CURSOR_OFFSET     (10)

// Utility function to align menu item names by the right side.
static int M_ItemRightAlign (const char *text)
{
    return ORIGWIDTH - currentMenu->x - M_StringWidth(text);
}

static void M_Draw_ID_Main (void);

static void M_Choose_ID_Video (int choice);
static void M_Draw_ID_Video (void);
static void M_ID_TrueColor (int choice);
static void M_ID_RenderingRes (int choice);
static void M_ID_Widescreen (int choice);
static void M_ID_UncappedFPS (int choice);
static void M_ID_LimitFPS (int choice);
static void M_ID_VSync (int choice);
static void M_ID_ShowFPS (int choice);
static void M_ID_PixelScaling (int choice);
static void M_ID_GfxStartup (int choice);
static void M_ID_ScreenWipe (int choice);
static void M_ID_DiskIcon (int choice);
static void M_ID_ShowExitScreen (int choice);
static void M_ID_ShowENDSTRF (int choice);

static void M_Choose_ID_Display (int choice);
static void M_Draw_ID_Display (void);
static void M_ID_Gamma (int choice);
static void M_ID_FOV (int choice);
static void M_ID_MenuShading (int choice);
static void M_ID_LevelBrightness (int choice);
static void M_ID_Saturation (int choice);
static void M_ID_R_Intensity (int choice);
static void M_ID_G_Intensity (int choice);
static void M_ID_B_Intensity (int choice);

// -----------------------------------------------------------------------------

// [JN] Delay before shading.
static int shade_wait;

// [JN] Shade background while in active menu.
static void M_ShadeBackground (void)
{
    if (dp_menu_shading)
    {
        for (int y = 0; y < SCREENWIDTH * SCREENHEIGHT; y++)
        {
#ifndef CRISPY_TRUECOLOR
            I_VideoBuffer[y] = colormaps[((dp_menu_shading + 3) * 2) * 256 + I_VideoBuffer[y]];
#else
            I_VideoBuffer[y] = I_BlendDark(I_VideoBuffer[y], I_ShadeFactor[dp_menu_shading]);
#endif
        }
    }
}

static byte *M_Small_Line_Glow (const int tics)
{
    return
        //tics == 5 ? cr[CR_MENU_BRIGHT5] :
        //tics == 4 ? cr[CR_MENU_BRIGHT4] :
        tics >= 3 ? cr[CR_MENU_BRIGHT3] :
        tics == 2 ? cr[CR_MENU_BRIGHT2] :
        tics == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
}

static byte *M_Big_Line_Glow (const int tics)
{
    return
        //tics == 5 ? cr[CR_MENU_BRIGHT5] :
        tics >= 4 ? cr[CR_MENU_BRIGHT4] :
        tics == 3 ? cr[CR_MENU_BRIGHT3] :
        tics == 2 ? cr[CR_MENU_BRIGHT2] :
        tics == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
}

static void M_Reset_Line_Glow (void)
{
    for (int i = 0 ; i < currentMenu->numitems ; i++)
    {
        currentMenu->menuitems[i].tics = 0;
    }
}

#define GLOW_UNCOLORED  0
#define GLOW_RED        1
#define GLOW_DARKRED    2
#define GLOW_GREEN      3
#define GLOW_YELLOW     4
#define GLOW_ORANGE     5
#define GLOW_LIGHTGRAY  6
#define GLOW_BLUE       7
#define GLOW_OLIVE      8
#define GLOW_DARKGREEN  9
#define GLOW_GRAY       10

#define ITEMONTICS      currentMenu->menuitems[itemOn].tics
#define ITEMSETONTICS   currentMenu->menuitems[itemSetOn].tics

static byte *M_Item_Glow (const int itemSetOn, const int color)
{
    if (itemOn == itemSetOn)
    {
        return
            color == GLOW_RED ||
            color == GLOW_DARKRED   ? cr[CR_RED_BRIGHT5]       :
            color == GLOW_GREEN     ? cr[CR_GREEN_BRIGHT5]     :
            color == GLOW_YELLOW    ? cr[CR_YELLOW_BRIGHT5]    :
            color == GLOW_ORANGE    ? cr[CR_ORANGE_BRIGHT5]    :
            color == GLOW_LIGHTGRAY ? cr[CR_LIGHTGRAY_BRIGHT5] :
            color == GLOW_BLUE      ? cr[CR_BLUE2_BRIGHT5]     :
            color == GLOW_OLIVE     ? cr[CR_OLIVE_BRIGHT5]     :
            color == GLOW_DARKGREEN ? cr[CR_DARKGREEN_BRIGHT5] :
            color == GLOW_GRAY      ? cr[CR_GRAY_BRIGHT5]      :
                                      cr[CR_MENU_BRIGHT5]      ; // GLOW_UNCOLORED
    }
    else
    {
        if (color == GLOW_UNCOLORED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_MENU_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_MENU_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_MENU_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_MENU_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
        }
        if (color == GLOW_RED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_RED_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_RED_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_RED_BRIGHT1] : cr[CR_RED];
        }
        if (color == GLOW_DARKRED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_DARK1] :
                ITEMSETONTICS == 4 ? cr[CR_RED_DARK2] :
                ITEMSETONTICS == 3 ? cr[CR_RED_DARK3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_DARK4] :
                ITEMSETONTICS == 1 ? cr[CR_RED_DARK5] : cr[CR_DARKRED];
        }
        if (color == GLOW_GREEN)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1] : cr[CR_GREEN];
        }
        if (color == GLOW_YELLOW)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_YELLOW_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_YELLOW_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_YELLOW_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_YELLOW_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_YELLOW_BRIGHT1] : cr[CR_YELLOW];
        }
        if (color == GLOW_ORANGE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_ORANGE_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_ORANGE_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_ORANGE_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_ORANGE_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_ORANGE_BRIGHT1] : cr[CR_ORANGE];
        }
        if (color == GLOW_LIGHTGRAY)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_LIGHTGRAY_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_LIGHTGRAY_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_LIGHTGRAY_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_LIGHTGRAY_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_LIGHTGRAY_BRIGHT1] : cr[CR_LIGHTGRAY];
        }
        if (color == GLOW_BLUE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_BLUE2_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_BLUE2_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_BLUE2_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_BLUE2_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_BLUE2_BRIGHT1] : cr[CR_BLUE2];
        }
        if (color == GLOW_OLIVE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_OLIVE_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_OLIVE_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_OLIVE_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_OLIVE_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_OLIVE_BRIGHT1] : cr[CR_OLIVE];
        }
        if (color == GLOW_DARKGREEN)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_DARKGREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_DARKGREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_DARKGREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_DARKGREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_DARKGREEN_BRIGHT1] : cr[CR_DARKGREEN];
        }
        if (color == GLOW_GRAY)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GRAY_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GRAY_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GRAY_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GRAY_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GRAY_BRIGHT1] : cr[CR_GRAY];
        }
    }
    return NULL;
}

static byte *M_Cursor_Glow (const int tics)
{
    return
        tics ==  8 || tics ==  7 ? cr[CR_MENU_BRIGHT4] :
        tics ==  6 || tics ==  5 ? cr[CR_MENU_BRIGHT3] :
        tics ==  4 || tics ==  3 ? cr[CR_MENU_BRIGHT2] :
        tics ==  2 || tics ==  1 ? cr[CR_MENU_BRIGHT1] :
        tics == -1 || tics == -2 ? cr[CR_MENU_DARK1]   :
        tics == -3 || tics == -4 ? cr[CR_MENU_DARK2]   :
        tics == -5 || tics == -6 ? cr[CR_MENU_DARK3]   :
        tics == -7 || tics == -8 ? cr[CR_MENU_DARK4]   : NULL;
}

static const int M_INT_Slider (int val, int min, int max, int direction, boolean capped)
{
    switch (direction)
    {
        case 0:
        val--;
        if (val < min) 
            val = capped ? min : max;
        break;

        case 1:
        val++;
        if (val > max)
            val = capped ? max : min;
        break;
    }
    return val;
}

static const float M_FLOAT_Slider (float val, float min, float max, float step,
                                   int direction, boolean capped)
{
    char buf[9];

    switch (direction)
    {
        case 0:
        val -= step;
        if (val < min) 
            val = capped ? min : max;
        break;

        case 1:
        val += step;
        if (val > max)
            val = capped ? max : min;
        break;
    }

    // [JN] Do a float correction to always get x.xxx000 values:
    sprintf (buf, "%f", val);
    val = (float)atof(buf);
    return val;
}

// -----------------------------------------------------------------------------
// Main ID Menu
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Main[]=
{
    { M_SWTC, "VIDEO OPTIONS",     M_Choose_ID_Video,    'v' },
    { M_SWTC, "DISPLAY OPTIONS",   M_Choose_ID_Display,  'd' },
    // { M_SWTC, "SOUND OPTIONS",     M_Choose_ID_Sound,    's' },
    // { M_SWTC, "CONTROL SETTINGS",  M_Choose_ID_Controls, 'c' },
    // { M_SWTC, "WIDGETS SETTINGS",  M_Choose_ID_Widgets,  'w' },
    // { M_SWTC, "AUTOMAP SETTINGS",  M_Choose_ID_Automap,  'a' },
    // { M_SWTC, "GAMEPLAY FEATURES", M_Choose_ID_Gameplay, 'g' },
    // { M_SWTC, "LEVEL SELECT",      M_Choose_ID_Level,    'l' },
    // { M_SWTC, "END GAME",          M_EndGame,            'e' },
    // { M_SWTC, "RESET SETTINGS",    M_Choose_ID_Reset,    'r' },
};

static menu_t ID_Def_Main =
{
    2,
    &MainDef,
    ID_Menu_Main,
    M_Draw_ID_Main,
    ID_MENU_LEFTOFFSET_SML, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Main (int choice)
{
    M_SetupNextMenu (&ID_Def_Main);
}

static void M_Draw_ID_Main (void)
{
    M_WriteTextCentered(9, "OPTIONS", cr[CR_ORANGE]);
}

// -----------------------------------------------------------------------------
// Video options
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Video[]=
{
    { M_LFRT, "TRUECOLOR RENDERING",  M_ID_TrueColor,      't' },
    { M_LFRT, "RENDERING RESOLUTION", M_ID_RenderingRes,   'r' },
    { M_LFRT, "WIDESCREEN MODE",      M_ID_Widescreen,     'w' },
    { M_LFRT, "UNCAPPED FRAMERATE",   M_ID_UncappedFPS,    'u' },
    { M_LFRT, "FRAMERATE LIMIT",      M_ID_LimitFPS,       'f' },
    { M_LFRT, "ENABLE VSYNC",         M_ID_VSync,          'e' },
    { M_LFRT, "SHOW FPS COUNTER",     M_ID_ShowFPS,        's' },
    { M_LFRT, "PIXEL SCALING",        M_ID_PixelScaling,   'p' },
    { M_SKIP, "", 0, '\0' },
    { M_LFRT, "GRAPHICAL STARTUP",    M_ID_GfxStartup,     'g' },
    { M_LFRT, "SCREEN WIPE EFFECT",   M_ID_ScreenWipe,     's' },
    { M_LFRT, "SHOW DISK ICON",       M_ID_DiskIcon,       's' },
    { M_LFRT, "SHOW EXIT SCREEN",     M_ID_ShowExitScreen, 's' },
    { M_LFRT, "SHOW ENDSTRF SCREEN",  M_ID_ShowENDSTRF,    's' },
};

static menu_t ID_Def_Video =
{
    14,
    &ID_Def_Main,
    ID_Menu_Video,
    M_Draw_ID_Video,
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Video (int choice)
{
    M_SetupNextMenu (&ID_Def_Video);
}

static void M_Draw_ID_Video (void)
{
    char str[32];

    M_WriteTextCentered(9, "VIDEO OPTIONS", cr[CR_ORANGE]);

#ifndef CRISPY_TRUECOLOR
    sprintf(str, "N/A");
    M_WriteText (M_ItemRightAlign(str), 18, str, 
                 M_Item_Glow(0, GLOW_DARKRED));
#else
    sprintf(str, vid_truecolor ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 18, str, 
                 M_Item_Glow(0, vid_truecolor ? GLOW_GREEN : GLOW_DARKRED));
#endif

    // Rendering resolution
    sprintf(str, vid_resolution == 1 ? "1X (200P)"  :
                 vid_resolution == 2 ? "2X (400P)"  :
                 vid_resolution == 3 ? "3X (600P)"  :
                 vid_resolution == 4 ? "4X (800P)"  :
                 vid_resolution == 5 ? "5X (1000P)" :
                 vid_resolution == 6 ? "6X (1200P)" :
                                       "CUSTOM");
    M_WriteText (M_ItemRightAlign(str), 27, str, 
                 M_Item_Glow(1, vid_resolution == 1 ? GLOW_DARKRED :
                                vid_resolution == 2 ||
                                vid_resolution == 3 ? GLOW_GREEN :
                                vid_resolution == 4 ||
                                vid_resolution == 5 ? GLOW_YELLOW :
                                                      GLOW_ORANGE));

    // Widescreen rendering
    sprintf(str, vid_widescreen == 1 ? "MATCH SCREEN" :
                 vid_widescreen == 2 ? "16:10" :
                 vid_widescreen == 3 ? "16:9" :
                 vid_widescreen == 4 ? "21:9" :
                 vid_widescreen == 5 ? "32:9" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 36, str, 
                 M_Item_Glow(2, vid_widescreen ? GLOW_GREEN : GLOW_DARKRED));

    // Uncapped framerate
    sprintf(str, vid_uncapped_fps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 45, str, 
                 M_Item_Glow(3, vid_uncapped_fps ? GLOW_GREEN : GLOW_DARKRED));

    // Framerate limit
    sprintf(str, !vid_uncapped_fps ? "35" :
                 vid_fpslimit ? "%d" : "NONE", vid_fpslimit);
    M_WriteText (M_ItemRightAlign(str), 54, str, 
                 !vid_uncapped_fps ? cr[CR_DARKRED] :
                 M_Item_Glow(4, vid_fpslimit == 0 ? GLOW_RED :
                                vid_fpslimit >= 500 ? GLOW_YELLOW : GLOW_GREEN));

    // Enable vsync
    sprintf(str, vid_vsync ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 63, str, 
                 M_Item_Glow(5, vid_vsync ? GLOW_GREEN : GLOW_DARKRED));

    // Show FPS counter
    sprintf(str, vid_showfps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 72, str, 
                 M_Item_Glow(6, vid_showfps ? GLOW_GREEN : GLOW_DARKRED));

    // Pixel scaling
    sprintf(str, vid_smooth_scaling ? "SMOOTH" : "SHARP");
    M_WriteText (M_ItemRightAlign(str), 81, str, 
                 M_Item_Glow(7, vid_smooth_scaling ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(90, "MISCELLANEOUS", cr[CR_ORANGE]);

    // Graphical startup
    sprintf(str, vid_graphical_startup ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 99, str, 
                 M_Item_Glow(9, vid_graphical_startup ? GLOW_GREEN : GLOW_DARKRED));

    // Screen wipe effect
    sprintf(str, vid_screenwipe ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 108, str, 
                 M_Item_Glow(10, vid_screenwipe ? GLOW_GREEN : GLOW_DARKRED));

    // Show disk icon
    sprintf(str, vid_diskicon_st == 1 ? "BOTTOM" :
                 vid_diskicon_st == 2 ? "TOP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 117, str, 
                 M_Item_Glow(11, vid_diskicon_st == 1 ? GLOW_DARKRED : GLOW_GREEN));

    // Screen exit screen
    sprintf(str, vid_exit_screen ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 126, str, 
                 M_Item_Glow(12, vid_exit_screen ? GLOW_GREEN : GLOW_DARKRED));

    // Show ENDSTRF screen
    sprintf(str, vid_endoom == 1 ? "ALWAYS" :
                 vid_endoom == 2 ? "PWAD ONLY" : "NEVER");
    M_WriteText (M_ItemRightAlign(str), 135, str, 
                 M_Item_Glow(13, vid_endoom == 1 ? GLOW_DARKRED : GLOW_GREEN));
}

#ifdef CRISPY_TRUECOLOR
static void M_ID_TrueColorHook (void)
{
    vid_truecolor ^= 1;

    // [crispy] re-calculate amount of colormaps and light tables
    R_InitColormaps();
    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
    // [crispy] re-calculate fake contrast
    P_SegLengths(true);
}
#endif

static void M_ID_TrueColor (int choice)
{
#ifdef CRISPY_TRUECOLOR
    post_rendering_hook = M_ID_TrueColorHook;
#endif
}

static void M_ID_RenderingResHook (void)
{
    // [crispy] re-initialize framebuffers, textures and renderer
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    // [crispy] re-calculate framebuffer coordinates
    R_ExecuteSetViewSize();
    // [crispy] re-draw bezel
    R_FillBackScreen();
    // [crispy] re-calculate disk icon coordinates
    V_EnableLoadingDisk();
    
    // TODO!
    /*
    // [JN] re-calculate status bar elements background buffers
    ST_InitElementsBackground();
    // [crispy] re-calculate automap coordinates
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }
    */
}

static void M_ID_RenderingRes (int choice)
{
    vid_resolution = M_INT_Slider(vid_resolution, 1, MAXHIRES, choice, false);
    post_rendering_hook = M_ID_RenderingResHook;
}

static void M_ID_WidescreenHook (void)
{
    // [crispy] re-initialize framebuffers, textures and renderer
    I_ReInitGraphics(REINIT_FRAMEBUFFERS | REINIT_TEXTURES | REINIT_ASPECTRATIO);
    // [crispy] re-calculate framebuffer coordinates
    R_ExecuteSetViewSize();
    // [crispy] re-draw bezel
    R_FillBackScreen();
    // [crispy] re-calculate disk icon coordinates
    V_EnableLoadingDisk();

    // TODO!
    /*
    // [JN] re-calculate status bar elements background buffers
    ST_InitElementsBackground();
    // [crispy] re-calculate automap coordinates
    AM_LevelInit(true);
    if (automapactive)
    {
        AM_Start();
    }
    */
}

static void M_ID_Widescreen (int choice)
{
    vid_widescreen = M_INT_Slider(vid_widescreen, 0, 5, choice, false);
    post_rendering_hook = M_ID_WidescreenHook;
}

static void M_ID_UncappedFPS (int choice)
{
    vid_uncapped_fps ^= 1;
    // [JN] Skip weapon bobbing interpolation for next frame.
    pspr_interp = false;
}

static void M_ID_LimitFPS (int choice)
{
    if (!vid_uncapped_fps)
    {
        return;  // Do not allow change value in capped framerate.
    }
    
    switch (choice)
    {
        case 0:
            if (vid_fpslimit)
            {
                if (speedkeydown())
                    vid_fpslimit -= 10;
                else
                    vid_fpslimit -= 1;
            }

            if (vid_fpslimit < TICRATE)
                vid_fpslimit = 0;

            break;
        case 1:
            if (vid_fpslimit < 501)
            {
                if (speedkeydown())
                    vid_fpslimit += 10;
                else
                    vid_fpslimit += 1;
            }

            if (vid_fpslimit < TICRATE)
                vid_fpslimit = TICRATE;
            if (vid_fpslimit > 500)
                vid_fpslimit = 500;

        default:
            break;
    }
}

static void M_ID_VSyncHook (void)
{
    I_ToggleVsync();
}

static void M_ID_VSync (int choice)
{
    vid_vsync ^= 1;
    post_rendering_hook = M_ID_VSyncHook;    
}

static void M_ID_ShowFPS (int choice)
{
    vid_showfps ^= 1;
}

static void M_ID_PixelScaling (int choice)
{
    vid_smooth_scaling ^= 1;

    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
}

static void M_ID_GfxStartup (int choice)
{
    vid_graphical_startup ^= 1;
}

static void M_ID_ScreenWipe (int choice)
{
    vid_screenwipe ^= 1;
}

static void M_ID_DiskIcon (int choice)
{
    vid_diskicon_st = M_INT_Slider(vid_diskicon_st, 0, 2, choice, false);
    V_EnableLoadingDisk();
}

static void M_ID_ShowExitScreen (int choice)
{
    vid_exit_screen ^= 1;
}

static void M_ID_ShowENDSTRF (int choice)
{
    vid_endoom = M_INT_Slider(vid_endoom, 0, 2, choice, false);
}

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static menuitem_t ID_Menu_Display[]=
{
    { M_LFRT, "GAMMA-CORRECTION",        M_ID_Gamma,             'g' },
    { M_LFRT, "FIELD OF VIEW",           M_ID_FOV,               'f' },
    { M_LFRT, "MENU BACKGROUND SHADING", M_ID_MenuShading,       'm' },
    { M_LFRT, "EXTRA LEVEL BRIGHTNESS",  M_ID_LevelBrightness,   'e' },
    { M_SKIP, "", 0, '\0' },
    { M_LFRT, "SATURATION",              M_ID_Saturation,        's' },
    { M_LFRT, "RED INTENSITY",           M_ID_R_Intensity,       'r' },
    { M_LFRT, "GREEN INTENSITY",         M_ID_G_Intensity,       'g' },
    { M_LFRT, "BLUE INTENSITY",          M_ID_B_Intensity,       'b' },
    // { M_SKIP, "", 0, '\0' },
    // { M_LFRT, "MESSAGES ENABLED",        M_ChangeMessages,       'm' },
    // { M_LFRT, "MESSAGES ALIGNMENT",      M_ID_MessagesAlignment, 'm' },
    // { M_LFRT, "TEXT CASTS SHADOWS",      M_ID_TextShadows,       't' },
    // { M_LFRT, "LOCAL TIME",              M_ID_LocalTime,         'l' },
};

static menu_t ID_Def_Display =
{
    9,
    &ID_Def_Main,
    ID_Menu_Display,
    M_Draw_ID_Display,
    ID_MENU_LEFTOFFSET_BIG, ID_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_Choose_ID_Display (int choice)
{
    M_SetupNextMenu (&ID_Def_Display);
}

static void M_Draw_ID_Display (void)
{
    char str[32];

    M_WriteTextCentered(9, "DISPLAY OPTIONS", cr[CR_ORANGE]);

    // Gamma-correction num
    M_WriteText (M_ItemRightAlign(gammalvls[vid_gamma][1]), 18, gammalvls[vid_gamma][1], 
                 M_Item_Glow(0, GLOW_LIGHTGRAY));

    // Field of View
    sprintf(str, "%d", vid_fov);
    M_WriteText (M_ItemRightAlign(str), 27, str,
                 M_Item_Glow(1, vid_fov == 135 || vid_fov == 45 ? GLOW_YELLOW :
                                vid_fov == 90 ? GLOW_DARKRED : GLOW_GREEN));

    // Background shading
    sprintf(str, dp_menu_shading ? "%d" : "OFF", dp_menu_shading);
    M_WriteText (M_ItemRightAlign(str), 36, str,
                 M_Item_Glow(2, dp_menu_shading == 12 ? GLOW_YELLOW :
                                dp_menu_shading  >  0 ? GLOW_GREEN  : GLOW_DARKRED));

    // Extra level brightness
    sprintf(str, dp_level_brightness ? "%d" : "OFF", dp_level_brightness);
    M_WriteText (M_ItemRightAlign(str), 45, str,
                 M_Item_Glow(3, dp_level_brightness == 8 ? GLOW_YELLOW :
                                dp_level_brightness >  0 ? GLOW_GREEN  : GLOW_DARKRED));

    M_WriteTextCentered(54, "COLOR SETTINGS", cr[CR_ORANGE]);

    // Saturation
    M_snprintf(str, 6, "%d%%", vid_saturation);
    M_WriteText (M_ItemRightAlign(str), 63, str,
                 M_Item_Glow(5, GLOW_LIGHTGRAY));

    // RED intensity
    M_snprintf(str, 6, "%3f", vid_r_intensity);
    M_WriteText (M_ItemRightAlign(str), 72, str,
                 M_Item_Glow(6, GLOW_RED));

    // GREEN intensity
    M_snprintf(str, 6, "%3f", vid_g_intensity);
    M_WriteText (M_ItemRightAlign(str), 81, str,
                 M_Item_Glow(7, GLOW_GREEN));

    // BLUE intensity
    M_snprintf(str, 6, "%3f", vid_b_intensity);
    M_WriteText (M_ItemRightAlign(str), 90, str,
                 M_Item_Glow(8, GLOW_BLUE));
}

static void M_ID_Gamma (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_gamma = M_INT_Slider(vid_gamma, 0, MAXGAMMA-1, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
#else
    I_SetPalette(0);
    R_InitColormaps();
    inhelpscreens = true;
    R_FillBackScreen();
    viewactive = false;
#endif
}

static void M_ID_FOV (int choice)
{
    vid_fov = M_INT_Slider(vid_fov, 45, 135, choice, true);

    // [crispy] re-calculate the zlight[][] array
    R_InitLightTables();
    // [crispy] re-calculate the scalelight[][] array
    R_ExecuteSetViewSize();
    // villsa [STRIFE]
    R_SetupPitch(viewpitch, true);
}

static void M_ID_MenuShading (int choice)
{
    dp_menu_shading = M_INT_Slider(dp_menu_shading, 0, 12, choice, true);
}

static void M_ID_LevelBrightness (int choice)
{
    dp_level_brightness = M_INT_Slider(dp_level_brightness, 0, 8, choice, true);
}

static void M_ID_Saturation (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_saturation = M_INT_Slider(vid_saturation, 0, 100, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    //AM_Init();
    inhelpscreens = true;
#endif
}

static void M_ID_R_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_r_intensity = M_FLOAT_Slider(vid_r_intensity, 0, 1.000000f, 0.025000f, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    //AM_Init();
    inhelpscreens = true;
#endif
}

static void M_ID_G_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_g_intensity = M_FLOAT_Slider(vid_g_intensity, 0, 1.000000f, 0.025000f, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    //AM_Init();
    inhelpscreens = true;
#endif
}

static void M_ID_B_Intensity (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    vid_b_intensity = M_FLOAT_Slider(vid_b_intensity, 0, 1.000000f, 0.025000f, choice, true);

#ifndef CRISPY_TRUECOLOR
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
#else
    R_InitColormaps();
    R_FillBackScreen();
    //AM_Init();
    inhelpscreens = true;
#endif
}

// =============================================================================


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
// [STRIFE]
// haleyjd 20110210: Rewritten to read "name" file in each slot directory
//
void M_ReadSaveStrings(void)
{
    FILE *handle;
    int   i;
    char *fname = NULL;

    for(i = 0; i < load_end; i++)
    {
        int retval;
        if(fname)
            Z_Free(fname);
        fname = M_SafeFilePath(savegamedir, M_MakeStrifeSaveDir(i, "\\name"));

        handle = M_fopen(fname, "rb");
        if(handle == NULL)
        {
            M_StringCopy(savegamestrings[i], DEH_String(EMPTYSTRING),
                         sizeof(savegamestrings[i]));
            LoadMenu[i].status = 0;
            continue;
        }
        retval = fread(savegamestrings[i], 1, SAVESTRINGSIZE, handle);
        fclose(handle);
        LoadMenu[i].status = retval == SAVESTRINGSIZE;
    }

    if(fname)
        Z_Free(fname);
}

//
// M_DrawNameChar
//
// haleyjd 09/22/10: [STRIFE] New function
// Handler for drawing the "Name Your Character" menu.
//
void M_DrawNameChar(void)
{
    int i;

    M_WriteText(72, 28, DEH_String("Name Your Character"), NULL);

    for (i = 0;i < load_end; i++)
    {
        M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
        M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], NULL);
    }

    if (saveStringEnter)
    {
        i = M_StringWidth(savegamestrings[quickSaveSlot]);
        M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*quickSaveSlot,"_", NULL);
    }
}

//
// M_DoNameChar
//
// haleyjd 09/22/10: [STRIFE] New function
// Handler for items in the "Name Your Character" menu.
//
void M_DoNameChar(int choice)
{
    int map;

    // 20130301: clear naming character flag for 1.31 save logic
    if(gameversion == exe_strife_1_31)
        namingCharacter = false;
    sendsave = 1;
    ClearTmp();
    G_WriteSaveName(choice, savegamestrings[choice]);
    quickSaveSlot = choice;  
    SaveDef.lastOn = choice;
    ClearSlot();
    FromCurr();
    
    if(isdemoversion)
        map = 33;
    else
        map = 2;

    G_DeferedInitNew(menuskill, map);
    M_ClearMenus(0);
}

//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
    int             i;

    V_DrawPatch(72, 28, W_CacheLumpName(DEH_String("M_LOADG"), PU_CACHE));

    for (i = 0;i < load_end; i++)
    {
        M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
        M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], NULL);
    }
}



//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;

    V_DrawPatch(x - 8, y + 7, W_CacheLumpName(DEH_String("M_LSLEFT"), PU_CACHE));

    for (i = 0;i < 24;i++)
    {
        V_DrawPatch(x, y + 7, W_CacheLumpName(DEH_String("M_LSCNTR"), PU_CACHE));
        x += 8;
    }

    V_DrawPatch(x, y + 7, W_CacheLumpName(DEH_String("M_LSRGHT"), PU_CACHE));
}



//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
    // [STRIFE]: completely rewritten
    char *name = NULL;

    G_WriteSaveName(choice, savegamestrings[choice]);
    ToCurr();

    // use safe & portable filepath concatenation for Choco
    name = M_SafeFilePath(savegamedir, M_MakeStrifeSaveDir(choice, ""));

    G_ReadCurrent(name);
    quickSaveSlot = choice;
    M_ClearMenus(0);

    Z_Free(name);
}

//
// Selected from DOOM menu
//
// [STRIFE] Verified unmodified
//
void M_LoadGame (int choice)
{
    if (netgame)
    {
        M_StartMessage(DEH_String(LOADNET), NULL, false);
        return;
    }

    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
void M_DrawSave(void)
{
    int             i;

    V_DrawPatch(72, 28, W_CacheLumpName(DEH_String("M_SAVEG"), PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
        M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
        M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], NULL);
    }

    if (saveStringEnter)
    {
        i = M_StringWidth(savegamestrings[quickSaveSlot]);
        M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*quickSaveSlot,"_", NULL);
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    // [STRIFE]: completely rewritten
    if(slot >= 0)
    {
        sendsave = 1;
        G_WriteSaveName(slot, savegamestrings[slot]);
        M_ClearMenus(0);
        quickSaveSlot = slot;        
        // haleyjd 20130922: slight divergence. We clear the destination slot 
        // of files here, which vanilla did not do. As a result, 1.31 had 
        // broken save behavior to the point of unusability. fraggle agrees 
        // this is detrimental enough to be fixed - unconditionally, for now.
        ClearSlot();        
        FromCurr();
    }
    else
        M_StartMessage(DEH_String(QSAVESPOT), NULL, false);
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    int x, y;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * LINEHEIGHT - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + LINEHEIGHT - 2);

    // [STRIFE]
    quickSaveSlot = choice;
    //saveSlot = choice;

    M_StringCopy(saveOldString, savegamestrings[choice], sizeof(saveOldString));
    if (!strcmp(savegamestrings[choice], DEH_String(EMPTYSTRING)))
        savegamestrings[choice][0] = 0;
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
    // [STRIFE]
    if (netgame)
    {
        // haleyjd 20110211: Hooray for Rogue's awesome multiplayer support...
        M_StartMessage(DEH_String("You can't save a netgame"), NULL, false);
        return;
    }
    if (!usergame)
    {
        M_StartMessage(DEH_String(SAVEDEAD),NULL,false);
        return;
    }

    if (gamestate != GS_LEVEL)
        return;

    // [STRIFE]
    if(gameversion == exe_strife_1_31)
    {
        // haleyjd 20130301: in 1.31, we can choose a slot again.
        M_SetupNextMenu(&SaveDef);
        M_ReadSaveStrings();
    }
    else
    {
        // In 1.2 and lower, you save over your character slot exclusively
        M_ReadSaveStrings();
        M_DoSave(quickSaveSlot);
    }
}



//
//      M_QuickSave
//
char    tempstring[80];

void M_QuickSaveResponse(int key)
{
    if (key == key_menu_confirm)
    {
        M_DoSave(quickSaveSlot);
        S_StartSound(NULL, sfx_mtalht); // [STRIFE] sound
    }
}

void M_QuickSave(void)
{
    if (netgame)
    {
        // haleyjd 20110211 [STRIFE]: More fun...
        M_StartMessage(DEH_String("You can't save a netgame"), NULL, false);
        return;
    }

    if (!usergame)
    {
        S_StartSound(NULL, sfx_oof);
        return;
    }

    if (gamestate != GS_LEVEL)
        return;

    if (quickSaveSlot < 0)
    {
        M_StartControlPanel();
        M_ReadSaveStrings();
        M_SetupNextMenu(&SaveDef);
        quickSaveSlot = -2;	// means to pick a slot now
        return;
    }
    DEH_snprintf(tempstring, 80, QSPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickSaveResponse,true);
}



//
// M_QuickLoadResponse
//
void M_QuickLoadResponse(int key)
{
    if (key == key_menu_confirm)
    {
        M_LoadSelect(quickSaveSlot);
        S_StartSound(NULL, sfx_mtalht); // [STRIFE] sound
    }
}

//
// M_QuickLoad
//
// [STRIFE] Verified unmodified
//
void M_QuickLoad(void)
{
    if (netgame)
    {
        M_StartMessage(DEH_String(QLOADNET),NULL,false);
        return;
    }

    if (quickSaveSlot < 0)
    {
        M_StartMessage(DEH_String(QSAVESPOT),NULL,false);
        return;
    }
    DEH_snprintf(tempstring, 80, QLPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring,M_QuickLoadResponse,true);
}




//
// Read This Menus
// Had a "quick hack to fix romero bug"
// haleyjd 08/28/10: [STRIFE] Draw HELP1, unconditionally.
//
void M_DrawReadThis1(void)
{
    inhelpscreens = true;

    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP1"), PU_CACHE));
}



//
// Read This Menus
// haleyjd 08/28/10: [STRIFE] Not optional, draws HELP2
//
void M_DrawReadThis2(void)
{
    inhelpscreens = true;

    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP2"), PU_CACHE));
}


//
// Read This Menus
// haleyjd 08/28/10: [STRIFE] New function to draw HELP3.
//
void M_DrawReadThis3(void)
{
    inhelpscreens = true;
    
    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP3"), PU_CACHE));
}

//
// Change Sfx & Music volumes
//
// haleyjd 08/29/10: [STRIFE]
// * Changed title graphic coordinates
// * Added voice volume and sensitivity sliders
//
void M_DrawSound(void)
{
    V_DrawPatch(100, 10, W_CacheLumpName(DEH_String("M_SVOL"), PU_CACHE));

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_vol+1),
                 16,sfxVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(music_vol+1),
                 16,musicVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(voice_vol+1),
                 16,voiceVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_mouse+1),
                 16,mouseSensitivity);
}

void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice)
{
    switch(choice)
    {
    case 0:
        if (sfxVolume)
            sfxVolume--;
        break;
    case 1:
        if (sfxVolume < 15)
            sfxVolume++;
        break;
    }

    S_SetSfxVolume(sfxVolume * 8);
}

//
// M_VoiceVol
//
// haleyjd 08/29/10: [STRIFE] New function
// Sets voice volume level.
//
void M_VoiceVol(int choice)
{
    switch(choice)
    {
    case 0:
        if (voiceVolume)
            voiceVolume--;
        break;
    case 1:
        if (voiceVolume < 15)
            voiceVolume++;
        break;
    }

    S_SetVoiceVolume(voiceVolume * 8);
}

void M_MusicVol(int choice)
{
    switch(choice)
    {
    case 0:
        if (musicVolume)
            musicVolume--;
        break;
    case 1:
        if (musicVolume < 15)
            musicVolume++;
        break;
    }

    S_SetMusicVolume(musicVolume * 8);
}




//
// M_DrawMainMenu
//
// haleyjd 08/27/10: [STRIFE] Changed x coordinate; M_DOOM -> M_STRIFE
//
void M_DrawMainMenu(void)
{
    V_DrawPatch(84, 2, W_CacheLumpName(DEH_String("M_STRIFE"), PU_CACHE));
}




//
// M_NewGame
//
// haleyjd 08/31/10: [STRIFE] Changed M_NEWG -> M_NGAME
//
void M_DrawNewGame(void)
{
    V_DrawPatch(96, 14, W_CacheLumpName(DEH_String("M_NGAME"), PU_CACHE));
    V_DrawPatch(54, 38, W_CacheLumpName(DEH_String("M_SKILL"), PU_CACHE));
}

void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
        M_StartMessage(DEH_String(NEWGAME),NULL,false);
        return;
    }
    // haleyjd 09/07/10: [STRIFE] Removed Chex Quest and DOOM gamemodes
    if(gameversion == exe_strife_1_31)
       namingCharacter = true; // for 1.31 save logic
    M_SetupNextMenu(&NewDef);
}


//
//      M_Episode
//

// haleyjd: [STRIFE] Unused
/*
int     epi;

void M_DrawEpisode(void)
{
    V_DrawPatchDirect(54, 38, W_CacheLumpName(DEH_String("M_EPISOD"), PU_CACHE));
}

void M_VerifyNightmare(int key)
{
    if (key != key_menu_confirm)
        return;

    G_DeferedInitNew(nightmare, 1);
    M_ClearMenus (0);
}
*/

void M_ChooseSkill(int choice)
{
    // haleyjd 09/07/10: Removed nightmare confirmation
    // [STRIFE]: start "Name Your Character" menu
    menuskill = choice;
    currentMenu = &NameCharDef;
    itemOn = NameCharDef.lastOn;
    M_ReadSaveStrings();
}

/*
// haleyjd [STRIFE] Unused
void M_Episode(int choice)
{
    if ( (gamemode == shareware)
	 && choice)
    {
	M_StartMessage(DEH_String(SWSTRING),NULL,false);
	M_SetupNextMenu(&ReadDef1);
	return;
    }

    // Yet another hack...
    if ( (gamemode == registered)
	 && (choice > 2))
    {
      fprintf( stderr,
	       "M_Episode: 4th episode requires UltimateDOOM\n");
      choice = 0;
    }
	 
    epi = choice;
    M_SetupNextMenu(&NewDef);
}
*/


//
// M_Options
//
char    detailNames[2][9]	= {"M_GDHIGH","M_GDLOW"};
char	msgNames[2][9]		= {"M_MSGOFF","M_MSGON"};


void M_DrawOptions(void)
{
    // haleyjd 08/27/10: [STRIFE] M_OPTTTL -> M_OPTION
    V_DrawPatch(108, 15, W_CacheLumpName(DEH_String("M_OPTION"), PU_CACHE));

    // haleyjd 08/26/10: [STRIFE] Removed messages, sensitivity, detail.

    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
                 9,screenSize);
}

void M_Options(int choice)
{
    M_SetupNextMenu(&OptionsDef);
}

//
// M_AutoUseHealth
//
// [STRIFE] New function
// haleyjd 20110211: toggle autouse health state
//
void M_AutoUseHealth(void)
{
    if(!netgame && usergame)
    {
        players[consoleplayer].cheats ^= CF_AUTOHEALTH;

        if(players[consoleplayer].cheats & CF_AUTOHEALTH)
            players[consoleplayer].message = DEH_String("Auto use health ON");
        else
            players[consoleplayer].message = DEH_String("Auto use health OFF");
    }
}

//
// M_ChangeShowText
//
// [STRIFE] New function
//
void M_ChangeShowText(void)
{
    dialogshowtext ^= true;

    if(dialogshowtext)
        players[consoleplayer].message = DEH_String("Conversation Text On");
    else
        players[consoleplayer].message = DEH_String("Conversation Text Off");
}

//
//      Toggle messages on/off
//
// [STRIFE] Messages cannot be disabled in Strife
/*
void M_ChangeMessages(int choice)
{
    // warning: unused parameter `int choice'
    choice = 0;
    showMessages = 1 - showMessages;

    if (!showMessages)
        players[consoleplayer].message = DEH_String(MSGOFF);
    else
        players[consoleplayer].message = DEH_String(MSGON);

    message_dontfuckwithme = true;
}
*/


//
// M_EndGame
//
void M_EndGameResponse(int key)
{
    if (key != key_menu_confirm)
        return;

    currentMenu->lastOn = itemOn;
    M_ClearMenus (0);
    D_StartTitle ();
}

void M_EndGame(int choice)
{
    choice = 0;
    if (!usergame)
    {
        S_StartSound(NULL,sfx_oof);
        return;
    }

    if (netgame)
    {
        M_StartMessage(DEH_String(NETEND),NULL,false);
        return;
    }

    M_StartMessage(DEH_String(ENDGAME),M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef1);
}

//
// M_ReadThis2
//
// haleyjd 08/28/10: [STRIFE] Eliminated DOOM stuff.
//
void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

//
// M_ReadThis3
//
// haleyjd 08/28/10: [STRIFE] New function.
//
void M_ReadThis3(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef3);
}

/*
// haleyjd 08/28/10: [STRIFE] Not used.
void M_FinishReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&MainDef);
}
*/

#if 0

//
// M_CheckStartCast
//
// [STRIFE] New but unused function. Was going to start a cast
//   call from within the menu system... not functional even in
//   the earliest demo version.
//
void M_CheckStartCast()
{
    if(usergame)
    {
        M_StartMessage(DEH_String("You have to end your game first."), NULL, false);
        return;
    }

    F_StartCast();
    M_ClearMenus(0);
}
#endif

//
// M_QuitResponse
//
// haleyjd 09/11/10: [STRIFE] Modifications to start up endgame
// demosequence.
//
void M_QuitResponse(int key)
{
    char buffer[20];

    if (key != key_menu_confirm)
        return;

    if(!vid_exit_screen || netgame)
        I_Quit();
    else
    {
        DEH_snprintf(buffer, sizeof(buffer), "qfmrm%i", gametic % 8 + 1);
        I_StartVoice(buffer);
        D_QuitGame();
    }
}

/*
// haleyjd 09/11/10: [STRIFE] Unused
static char *M_SelectEndMessage(void)
{
}
*/

//
// M_QuitStrife
//
// [STRIFE] Renamed from M_QuitDOOM
// haleyjd 09/11/10: No randomized text message; that's taken care of
// by the randomized voice message after confirmation.
//
void M_QuitStrife(int choice)
{
    DEH_snprintf(endstring, sizeof(endstring),
                 "Do you really want to leave?\n\n" DOSY);
  
    M_StartMessage(endstring, M_QuitResponse, true);
}




void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
    case 0:
        if (mouseSensitivity)
            mouseSensitivity--;
        break;
    case 1:
        if (mouseSensitivity < 9)
            mouseSensitivity++;
        break;
    }
}

/*
// haleyjd [STRIFE] Unused
void M_ChangeDetail(int choice)
{
    choice = 0;
    detailLevel = 1 - detailLevel;

    R_SetViewSize (screenblocks, detailLevel);

    if (!detailLevel)
	players[consoleplayer].message = DEH_String(DETAILHI);
    else
	players[consoleplayer].message = DEH_String(DETAILLO);
}
*/

// [STRIFE] Verified unmodified
void M_SizeDisplay(int choice)
{
    switch(choice)
    {
    case 0:
        if (screenSize > 0)
        {
            screenblocks--;
            screenSize--;
        }
        break;
    case 1:
        if (screenSize < 8)
        {
            screenblocks++;
            screenSize++;
        }
        break;
    }

    R_SetViewSize (screenblocks, detailLevel);
}




//
//      Menu Functions
//

//
// M_DrawThermo
//
// haleyjd 08/28/10: [STRIFE] Changes to some patch coordinates.
//
void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot )
{
    int         xx;
    int         yy; // [STRIFE] Needs a temp y coordinate variable
    int         i;

    xx = x;
    yy = y + 6; // [STRIFE] +6 to y coordinate
    V_DrawPatch(xx, yy, W_CacheLumpName(DEH_String("M_THERML"), PU_CACHE));
    xx += 8;
    for (i=0;i<thermWidth;i++)
    {
        V_DrawPatch(xx, yy, W_CacheLumpName(DEH_String("M_THERMM"), PU_CACHE));
        xx += 8;
    }
    V_DrawPatch(xx, yy, W_CacheLumpName(DEH_String("M_THERMR"), PU_CACHE));

    // [STRIFE] +2 to initial y coordinate
    V_DrawPatch((x + 8) + thermDot * 8, y + 2,
                      W_CacheLumpName(DEH_String("M_THERMO"), PU_CACHE));
}


// haleyjd: These are from DOOM v0.5 and the prebeta! They drew those ugly red &
// blue checkboxes... preserved for historical interest, as not in Strife.
void
M_DrawEmptyCell
( menu_t*	menu,
  int		item )
{
    V_DrawPatch(menu->x - 10, menu->y + item * LINEHEIGHT - 1, 
                      W_CacheLumpName(DEH_String("M_CELL1"), PU_CACHE));
}

void
M_DrawSelCell
( menu_t*	menu,
  int		item )
{
    V_DrawPatch(menu->x - 10, menu->y + item * LINEHEIGHT - 1,
                      W_CacheLumpName(DEH_String("M_CELL2"), PU_CACHE));
}


void
M_StartMessage
( const char	*string,
  void*		routine,
  boolean	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    return;
}



void M_StopMessage(void)
{
    menuactive = messageLastMenuActive;
    messageToPrint = 0;
}



//
// Find string width from hu_font chars
//
int M_StringWidth(const char *string)
{
    size_t             i;
    int             w = 0;
    int             c;

    for (i = 0;i < strlen(string);i++)
    {
        c = toupper(string[i]) - HU_FONTSTART;
        if (c < 0 || c >= HU_FONTSIZE)
            w += 4;
        else
            w += SHORT (hu_font[c]->width);
    }

    return w;
}



//
//      Find string height from hu_font chars
//
int M_StringHeight(const char *string)
{
    size_t             i;
    int             h;
    int             height = SHORT(hu_font[0]->height);

    h = height;
    for (i = 0;i < strlen(string);i++)
        if (string[i] == '\n')
            h += height;

    return h;
}


//
// M_WriteText
//
// Write a string using the hu_font
// haleyjd 09/04/10: [STRIFE]
// * Rogue made a lot of changes to this for the dialog system.
//
int
M_WriteText
( int           x,
  int           y,
  const char*   string, // haleyjd: made const for safety w/dialog engine
  byte         *table)
{
    int	        w;
    const char* ch;
    int         c;
    int         cx;
    int         cy;

    ch = string;
    cx = x;
    cy = y;

    dp_translation = table;

    while(1)
    {
        c = *ch++;
        if (!c)
            break;

        // haleyjd 09/04/10: [STRIFE] Don't draw spaces at the start of lines.
        if(c == ' ' && cx == x)
            continue;

        if (c == '\n')
        {
            cx = x;
            cy += 11; // haleyjd 09/04/10: [STRIFE]: Changed 12 -> 11
            continue;
        }

        c = toupper(c) - HU_FONTSTART;
        if (c < 0 || c>= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        // haleyjd 09/04/10: [STRIFE] Different linebreak handling
        if (cx + w > SCREENWIDTH - 20)
        {
            cx = x;
            cy += 11;
            --ch;
        }
        else
        {
            V_DrawPatch(cx, cy, hu_font[c]);
            cx += w;
        }
    }

    dp_translation = NULL;

    // [STRIFE] Return final y coordinate.
    return cy + 12;
}

// -----------------------------------------------------------------------------
// M_WriteTextCentered
// [JN] Write a centered string using the hu_font.
// -----------------------------------------------------------------------------

void M_WriteTextCentered (const int y, const char *string, byte *table)
{
    const char *ch;
    const int width = M_StringWidth(string);
    int w, c, cx, cy;

    ch = string;
    cx = ORIGWIDTH/2-width/2;
    cy = y;

    dp_translation = table;

    // find width
    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        c = c - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            continue;
        }

        w = SHORT (hu_font[c]->width);
    }

    // draw it
    cx = ORIGWIDTH/2-width/2;
    ch = string;

    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        if (cx+w > ORIGWIDTH)
        {
            break;
        }
        
        V_DrawShadowedPatchOptional(cx, cy, 0, hu_font[c]);
        cx += w;
    }
    
    dp_translation = NULL;
}

//
// M_DialogDimMsg
//
// [STRIFE] New function
// haleyjd 09/04/10: Painstakingly transformed from the assembly code, as the
// decompiler could not touch it. Redimensions a string to fit on screen, leaving
// at least a 20 pixel margin on the right side. The string passed in must be
// writable.
//
void M_DialogDimMsg(int x, int y, char *str, boolean useyfont)
{
    int rightbound = (SCREENWIDTH - 20) - x;
    patch_t **fontarray;  // ebp
    int linewidth = 0;    // esi
    int i = 0;            // edx
    char *message = str;  // edi
    char  bl;             // bl

    if(useyfont)
       fontarray = yfont;
    else
       fontarray = hu_font;

    bl = toupper(*message);

    if(!bl)
        return;

    // outer loop - run to end of string
    do
    {
        if(bl != '\n')
        {
            int charwidth; // eax
            int tempwidth; // ecx

            if(bl < HU_FONTSTART || bl > HU_FONTEND)
                charwidth = 4;
            else
                charwidth = SHORT(fontarray[bl - HU_FONTSTART]->width);

            tempwidth = linewidth + charwidth;

            // Test if the line still fits within the boundary...
            if(tempwidth >= rightbound)
            {
                // Doesn't fit...
                char *tempptr = &message[i]; // ebx
                char  al;                    // al

                // inner loop - run backward til a space (or the start of the
                // string) is found, subtracting width off the current line.
                // BUG: shouldn't we stop at a previous '\n' too?
                while(*tempptr != ' ' && i > 0)
                {
                    tempptr--;
                    // BUG: they didn't add the first char to linewidth yet...
                    linewidth -= charwidth; 
                    i--;
                    al = toupper(*tempptr);
                    if(al < HU_FONTSTART || al > HU_FONTEND)
                        charwidth = 4;
                    else
                        charwidth = SHORT(fontarray[al - HU_FONTSTART]->width);
                }
                // Replace the space with a linebreak.
                // BUG: what if i is zero? ... infinite loop time!
                message[i] = '\n';
                linewidth = 0;
            }
            else
            {
                // The line does fit.
                // Spaces at the start of a line don't count though.
                if(!(bl == ' ' && linewidth == 0))
                    linewidth += charwidth;
            }
        }
        else
            linewidth = 0; // '\n' seen, so reset the line width
    }
    while((bl = toupper(message[++i])) != 0); // step to the next character
}

// These keys evaluate to a "null" key in Vanilla Doom that allows weird
// jumping in the menus. Preserve this behavior for accuracy.

static boolean IsNullKey(int key)
{
    return key == KEY_PAUSE || key == KEY_CAPSLOCK
        || key == KEY_SCRLCK || key == KEY_NUMLOCK;
}

//
// CONTROL PANEL
//

//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
    int             key;
    int             i;
    static  int     joywait = 0;
    static  int     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    // [JN] Disable menu left/right controls by mouse movement.
    /*
    static  int     mousex = 0;
    static  int     lastx = 0;
    */
    //int dir;

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.

    if (testcontrols)
    {
        if (ev->type == ev_quit
         || (ev->type == ev_keydown
          && (ev->data1 == key_menu_activate || ev->data1 == key_menu_quit)))
        {
            I_Quit();
            return true;
        }

        return false;
    }

    // "close" button pressed on window?
    if (ev->type == ev_quit)
    {
        // First click on close button = bring up quit confirm message.
        // Second click on close button = confirm quit

        if (menuactive && messageToPrint && messageRoutine == M_QuitResponse)
        {
            M_QuitResponse(key_menu_confirm);
        }
        else
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuitStrife(0);
        }

        return true;
    }

    // key is the key pressed, ch is the actual character typed
  
    ch = 0;
    key = -1;

    if (ev->type == ev_joystick && joywait < I_GetTime())
    {
        // [JN] TODO - Joystick?
        /*
        if (JOY_GET_DPAD(ev->data6) != JOY_DIR_NONE)
        {
            dir = JOY_GET_DPAD(ev->data6);
        }
        else if (JOY_GET_LSTICK(ev->data6) != JOY_DIR_NONE)
        {
            dir = JOY_GET_LSTICK(ev->data6);
        }
        else
        {
            dir = JOY_GET_RSTICK(ev->data6);
        }

        if (dir & JOY_DIR_UP)
        {
            key = key_menu_up;
            joywait = I_GetTime() + 5;
        }
        else if (dir & JOY_DIR_DOWN)
        {
            key = key_menu_down;
            joywait = I_GetTime() + 5;
        }
        if (dir & JOY_DIR_LEFT)
        {
            key = key_menu_left;
            joywait = I_GetTime() + 5;
        }
        else if (dir & JOY_DIR_RIGHT)
        {
            key = key_menu_right;
            joywait = I_GetTime() + 5;
        }

#define JOY_BUTTON_MAPPED(x) ((x) >= 0)
#define JOY_BUTTON_PRESSED(x) (JOY_BUTTON_MAPPED(x) && (ev->data1 & (1 << (x))) != 0)

        if (JOY_BUTTON_PRESSED(joybfire))
        {
            // Simulate a 'Y' keypress when Doom show a Y/N dialog with Fire button.
            if (messageToPrint && messageNeedsInput)
            {
                key = key_menu_confirm;
            }
            else
            {
                key = key_menu_forward;
            }
            joywait = I_GetTime() + 5;
        }
        if (JOY_BUTTON_PRESSED(joybuse))
        {
            // Simulate a 'N' keypress when Doom show a Y/N dialog with Use button.
            if (messageToPrint && messageNeedsInput)
            {
                key = key_menu_abort;
            }
            else
            {
                key = key_menu_back;
            }
            joywait = I_GetTime() + 5;
        }
        if (JOY_BUTTON_PRESSED(joybmenu))
        {
            key = key_menu_activate;
            joywait = I_GetTime() + 5;
        }
        */
    }
    else
    {
        if (ev->type == ev_mouse && mousewait < I_GetTime()
        && !ev->data2 && !ev->data3) // [JN] Do not consider movement as pressing.
        {
            // [crispy] mouse_novert disables controlling the menus with the mouse
            // [JN] Not needed, as menu is fully controllable by mouse wheel and buttons.
            /*
            if (!mouse_novert)
            {
            mousey += ev->data3;
            }
            */
            if (mousey < lasty-30)
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 5;
                mousey = lasty -= 30;
            }
            else if (mousey > lasty+30)
            {
                key = key_menu_up;
                mousewait = I_GetTime() + 5;
                mousey = lasty += 30;
            }

            // [JN] Disable menu left/right controls by mouse movement.
            /*
            mousex += ev->data2;
            if (mousex < lastx-30)
            {
                key = key_menu_left;
                mousewait = I_GetTime() + 5;
                mousex = lastx -= 30;
            }
            else if (mousex > lastx+30)
            {
                key = key_menu_right;
                mousewait = I_GetTime() + 5;
                mousex = lastx += 30;
            }
            */

            if (ev->data1&1)
            {
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_confirm;  // [JN] Confirm by left mouse button.
                    mousewait = I_GetTime() + 5;
                }
                else
                {
                    key = key_menu_forward;
                    mousewait = I_GetTime() + 5;
                    if (menuindialog) // [crispy] fix mouse fire delay
                    {
                        mouse_fire_countdown = 5;   // villsa [STRIFE]
                    }
                }
            }

            if (ev->data1&2)
            {
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_abort;  // [JN] Cancel by right mouse button.
                }
                else
                {
                    key = key_menu_back;
                }
                mousewait = I_GetTime() + 5;
            }

            // [crispy] scroll menus with mouse wheel
            // [JN] Hardcoded to always use mouse wheel up/down.
            if (/*mousebprevweapon >= 0 &&*/ ev->data1 & (1 << 4 /*mousebprevweapon*/))
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 1;
            }
            else
            if (/*mousebnextweapon >= 0 &&*/ ev->data1 & (1 << 3 /*mousebnextweapon*/))
            {
                key = key_menu_up;
                mousewait = I_GetTime() + 1;
            }
        }
        else
        {
            if (ev->type == ev_keydown)
            {
                key = ev->data1;
                ch = ev->data2;
            }
        }
    }

    if (key == -1)
        return false;

    // Save Game string input
    if (saveStringEnter)
    {
        switch(key)
        {
        case KEY_BACKSPACE:
            if (saveCharIndex > 0)
            {
                saveCharIndex--;
                savegamestrings[quickSaveSlot][saveCharIndex] = 0;
            }
            break;

        case KEY_ESCAPE:
            saveStringEnter = 0;
            I_StopTextInput();
            M_StringCopy(savegamestrings[quickSaveSlot], saveOldString,
                         sizeof(savegamestrings[quickSaveSlot]));
            break;

        case KEY_ENTER:
            // [STRIFE]
            saveStringEnter = 0;
            I_StopTextInput();
            if(gameversion == exe_strife_1_31 && !namingCharacter)
            {
               // In 1.31, we can be here as a result of normal saving again,
               // whereas in 1.2 this only ever happens when naming your
               // character to begin a new game.
               M_DoSave(quickSaveSlot);
               return true;
            }
            if (savegamestrings[quickSaveSlot][0])
                M_DoNameChar(quickSaveSlot);
            break;

        default:
            // Savegame name entry. This is complicated.
            // Vanilla has a bug where the shift key is ignored when entering
            // a savegame name. If vanilla_keyboard_mapping is on, we want
            // to emulate this bug by using ev->data1. But if it's turned off,
            // it implies the user doesn't care about Vanilla emulation:
            // instead, use ev->data3 which gives the fully-translated and
            // modified key input.

            if (ev->type != ev_keydown)
            {
                break;
            }

            if (vanilla_keyboard_mapping)
            {
                ch = ev->data1;
            }
            else
            {
                ch = ev->data3;
            }

            ch = toupper(ch);

            if (ch != ' '
                && (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE))
            {
                break;
            }

            if (ch >= 32 && ch <= 127 &&
                saveCharIndex < SAVESTRINGSIZE-1 &&
                M_StringWidth(savegamestrings[quickSaveSlot]) <
                (SAVESTRINGSIZE-2)*8)
            {
                savegamestrings[quickSaveSlot][saveCharIndex++] = ch;
                savegamestrings[quickSaveSlot][saveCharIndex] = 0;
            }
            break;
        }
        return true;
    }

    // Take care of any messages that need input
    if (messageToPrint)
    {
        if (messageNeedsInput)
        {
            if (key != ' ' && key != KEY_ESCAPE
                && key != key_menu_confirm && key != key_menu_abort)
            {
                return false;
            }
        }

        menuactive = messageLastMenuActive;
        messageToPrint = 0;
        if (messageRoutine)
            messageRoutine(key);

        menupause = false;                // [STRIFE] unpause
        menuactive = false;
        S_StartSound(NULL, sfx_mtalht);   // [STRIFE] sound
        return true;
    }

    // [STRIFE]:
    // * In v1.2 this is moved to F9 (quickload)
    // * In v1.31 it is moved to F12 with DM spy, and quicksave
    //   functionality is restored separate from normal saving
    /*
    if (devparm && key == key_menu_help)
    {
        G_ScreenShot ();
        return true;
    }
    */

    // F-Keys
    if (!menuactive)
    {
        if (key == key_menu_decscreen)      // Screen size down
        {
            if (automapactive || chat_on)
                return false;
            M_SizeDisplay(0);
            S_StartSound(NULL, sfx_stnmov);
            return true;
        }
        else if (key == key_menu_incscreen) // Screen size up
        {
            if (automapactive || chat_on)
                return false;
            M_SizeDisplay(1);
            S_StartSound(NULL, sfx_stnmov);
            return true;
        }
        else if (key == key_menu_help)     // Help key
        {
            M_StartControlPanel ();
            // haleyjd 08/29/10: [STRIFE] always ReadDef1
            currentMenu = &ReadDef1; 

            itemOn = 0;
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_save)     // Save
        {
            // [STRIFE]: Hub saves
            if(gameversion == exe_strife_1_31)
                namingCharacter = false; // just saving normally, in 1.31

            if(netgame || players[consoleplayer].health <= 0 ||
                players[consoleplayer].cheats & CF_ONFIRE)
            {
                S_StartSound(NULL, sfx_oof);
            }
            else
            {
                M_StartControlPanel();
                S_StartSound(NULL, sfx_swtchn);
                M_SaveGame(0);
            }
            return true;
        }
        else if (key == key_menu_load)     // Load
        {
            // [STRIFE]: Hub saves
            if(gameversion == exe_strife_1_31)
            {
                // 1.31: normal save loading
                namingCharacter = false;
                M_StartControlPanel();
                M_LoadGame(0);
                S_StartSound(NULL, sfx_swtchn);
            }
            else
            {
                // Pre 1.31: quickload only
                S_StartSound(NULL, sfx_swtchn);
                M_QuickLoad();
            }
            return true;
        }
        else if (key == key_menu_volume)   // Sound Volume
        {
            M_StartControlPanel ();
            currentMenu = &SoundDef;
            itemOn = sfx_vol;
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_detail)   // Detail toggle
        {
            //M_ChangeDetail(0);
            M_AutoUseHealth(); // [STRIFE]
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_qsave)    // Quicksave
        {
            // [STRIFE]: Hub saves
            if(gameversion == exe_strife_1_31)
                namingCharacter = false; // for 1.31 save changes

            if(netgame || players[consoleplayer].health <= 0 ||
               players[consoleplayer].cheats & CF_ONFIRE)
            {
                S_StartSound(NULL, sfx_oof);
            }
            else
            {
                S_StartSound(NULL, sfx_swtchn);
                M_QuickSave();
            }
            return true;
        }
        else if (key == key_menu_endgame)  // End game
        {
            S_StartSound(NULL, sfx_swtchn);
            M_EndGame(0);
            return true;
        }
        else if (key == key_menu_messages) // Toggle messages
        {
            //M_ChangeMessages(0);
            M_ChangeShowText(); // [STRIFE]
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_qload)    // Quickload
        {
            // [STRIFE]
            // * v1.2: takes a screenshot
            // * v1.31: does quickload again
            if(gameversion == exe_strife_1_31)
            {
                namingCharacter = false;
                S_StartSound(NULL, sfx_swtchn);
                M_QuickLoad();
            }
            else
                G_ScreenShot();
            return true;
        }
        else if (key == key_menu_quit)     // Quit DOOM
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuitStrife(0);
            return true;
        }
        else if(gameversion == exe_strife_1_31 && key == key_spy)
        {
            // haleyjd 20130301: 1.31 moved screenshots to F12.
            G_ScreenShot();
            return true;
        }
        else if (key != 0 && key == key_menu_screenshot)
        {
            G_ScreenShot();
            return true;
        }
    }

    // [JN] Allow to change gamma while active menu.
    if (key == key_menu_gamma)    // gamma toggle
    {
        vid_gamma = M_INT_Slider(vid_gamma, 0, MAXGAMMA-1, 1 /*right*/, false);
        players[consoleplayer].message = DEH_String(gammalvls[vid_gamma][0]);
#ifndef CRISPY_TRUECOLOR
        I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));
#else
        I_SetPalette(0);
        R_InitColormaps();
        inhelpscreens = true;
        R_FillBackScreen();
        viewactive = false;
#endif
        return true;
    }

    // Pop-up menu?
    if (!menuactive)
    {
        if (key == key_menu_activate)
        {
            M_StartControlPanel ();
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        return false;
    }

    
    // Keys usable within menu

    if (key == key_menu_down)
    {
        // Move down to next item

        do
        {
            if (itemOn+1 > currentMenu->numitems-1)
                itemOn = 0;
            else itemOn++;
            S_StartSound(NULL, sfx_pstop);
        } while(currentMenu->menuitems[itemOn].status==-1);

        return true;
    }
    else if (key == key_menu_up)
    {
        // Move back up to previous item

        do
        {
            if (!itemOn)
                itemOn = currentMenu->numitems-1;
            else itemOn--;
            S_StartSound(NULL, sfx_pstop);
        } while(currentMenu->menuitems[itemOn].status==-1);

        return true;
    }
    else if (key == key_menu_left)
    {
        // Slide slider left

        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(0);
        }
        return true;
    }
    else if (key == key_menu_right)
    {
        // Slide slider right

        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL, sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(1);
        }
        return true;
    }
    else if (key == key_menu_forward)
    {
        // Activate menu item

        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status)
        {
            currentMenu->lastOn = itemOn;
            if (currentMenu->menuitems[itemOn].status == 2)
            {
                currentMenu->menuitems[itemOn].routine(1);      // right arrow
                S_StartSound(NULL, sfx_stnmov);
            }
            else
            {
                currentMenu->menuitems[itemOn].routine(itemOn);
                //S_StartSound(NULL, sfx_swish); [STRIFE] No sound is played here.
            }
        }
        return true;
    }
    else if (key == key_menu_activate)
    {
        // Deactivate menu
        if(gameversion == exe_strife_1_31) // [STRIFE]: 1.31 saving
            namingCharacter = false;

        if(menuindialog) // [STRIFE] - Get out of dialog engine semi-gracefully
            P_DialogDoChoice(-1);

        currentMenu->lastOn = itemOn;
        M_ClearMenus (0);
        S_StartSound(NULL, sfx_mtalht); // villsa [STRIFE]: sounds
        return true;
    }
    else if (key == key_menu_back)
    {
        // Go back to previous menu

        currentMenu->lastOn = itemOn;
        if (currentMenu->prevMenu)
        {
            currentMenu = currentMenu->prevMenu;
            M_Reset_Line_Glow();
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL, sfx_swtchn);
        }
        return true;
    }

    // Keyboard shortcut?
    // Vanilla Strife has a weird behavior where it jumps to the scroll bars
    // when certain keys are pressed, so emulate this.

    else if (ch != 0 || IsNullKey(key))
    {
        // Keyboard shortcut?

        for (i = itemOn+1;i < currentMenu->numitems;i++)
        {
            if (currentMenu->menuitems[i].alphaKey == ch)
            {
                itemOn = i;
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }

        for (i = 0;i <= itemOn;i++)
        {
            if (currentMenu->menuitems[i].alphaKey == ch)
            {
                itemOn = i;
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }
    }

    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
        return;
    
    menuactive = 1;
    menupause = true;
    currentMenu = &MainDef;         // JDC
    M_Reset_Line_Glow();
    itemOn = currentMenu->lastOn;   // JDC
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static short	x;
    static short	y;
    unsigned int	i;
    unsigned int	max;
    char		string[80];
    const char          *name;
    int			start;

    inhelpscreens = false;
    
    // [JN] Shade background while active menu.
    if (menuactive)
    {
        // Temporary unshade while changing certain settings.
        if (shade_wait < I_GetTime())
        {
            M_ShadeBackground();
        }
        // Always redraw status bar background.
        inhelpscreens = true;
    }

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
        start = 0;
        y = 100 - M_StringHeight(messageString) / 2;
        while (messageString[start] != '\0')
        {
            int foundnewline = 0;

            for (i = 0; i < strlen(messageString + start); i++)
            {
                if (messageString[start + i] == '\n')
                {
                    M_StringCopy(string, messageString + start,
                                 sizeof(string));
                    if (i < sizeof(string))
                    {
                        string[i] = '\0';
                    }

                    foundnewline = 1;
                    start += i + 1;
                    break;
                }
            }

            if (!foundnewline)
            {
                M_StringCopy(string, messageString + start,
                             sizeof(string));
                start += strlen(string);
            }

            x = 160 - M_StringWidth(string) / 2;
            M_WriteText(x, y, string, NULL);
            y += SHORT(hu_font[0]->height);
        }

        return;
    }

    if (!menuactive)
        return;

    if (currentMenu->routine)
        currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    if (currentMenu->smallFont)
    {
        // [JN] Draw glowing * symbol.
        M_WriteText(x - ID_MENU_CURSOR_OFFSET, y + itemOn * ID_MENU_LINEHEIGHT_SMALL, "*",
                    M_Cursor_Glow(cursor_tics));

        for (i = 0 ; i < max ; i++)
        {
            M_WriteText (x, y, currentMenu->menuitems[i].name,
                         M_Small_Line_Glow(currentMenu->menuitems[i].tics));
            y += ID_MENU_LINEHEIGHT_SMALL;
        }
    }
    else
    {
        // haleyjd 08/27/10: [STRIFE] Adjust to draw spinning Sigil
        // DRAW SIGIL
        V_DrawPatch(x + CURSORXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
                          W_CacheLumpName(DEH_String(cursorName[whichCursor]),
                                          PU_CACHE));

        for (i=0;i<max;i++)
        {
            name = DEH_String(currentMenu->menuitems[i].name);

            if (name[0])
            {
                dp_translation = M_Big_Line_Glow(currentMenu->menuitems[i].tics);
                V_DrawPatch(x, y, W_CacheLumpName(name, PU_CACHE));
                dp_translation = NULL;
            }
            y += LINEHEIGHT;
        }
    }
    


}


//
// M_ClearMenus
//
// haleyjd 08/28/10: [STRIFE] Added an int param so this can be called by menus.
//         09/08/10: Added menupause.
//
void M_ClearMenus (int choice)
{
    choice = 0;     // haleyjd: for no warning; not from decompilation.
    menuactive = 0;
    menupause = 0;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    M_Reset_Line_Glow();
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
// haleyjd 08/27/10: [STRIFE] Rewritten for Sigil cursor
//
void M_Ticker (void)
{
    if (--cursorAnimCounter <= 0)
    {
        whichCursor = (whichCursor + 1) % 8;
        cursorAnimCounter = 5;
    }

    // [JN] Menu glowing animation:
    
    // Brightening
    if (!cursor_direction && ++cursor_tics == 8)
    {
        cursor_direction = true;
    }
    // Darkening
    else
    if (cursor_direction && --cursor_tics == -8)
    {
        cursor_direction = false;
    }

    // [JN] Menu item fading effect:

    for (int i = 0 ; i < currentMenu->numitems ; i++)
    {
        if (itemOn == i)
        {
            // Keep menu item bright
            currentMenu->menuitems[i].tics = 5;
        }
        else
        {
            // Decrease tics for glowing effect
            currentMenu->menuitems[i].tics--;
        }
    }
}


//
// M_Init
//
// haleyjd 08/27/10: [STRIFE] Removed DOOM gamemode stuff
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichCursor = 0;
    cursorAnimCounter = 10;
    screenSize = screenblocks - 3;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive; // STRIFE-FIXME: assigns 0 here...
    quickSaveSlot = -1;

    // [STRIFE]: Initialize savegame paths and clear temporary directory
    G_WriteSaveName(5, "ME");
    ClearTmp();

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.
}

