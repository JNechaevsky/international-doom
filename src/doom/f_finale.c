//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2025 Julia Nechaevskaya
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
//	Game completion, final screen animation.
//


#include <stdio.h>
#include <ctype.h>

// Functions.
#include "ct_chat.h"
#include "deh_main.h"
#include "i_system.h"
#include "i_swap.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"
#include "s_sound.h"
#include "m_menu.h"

// Data.
#include "d_main.h"
#include "dstrings.h"
#include "sounds.h"
#include "doomstat.h"
#include "r_local.h"
#include "m_misc.h" // [crispy] M_StringDuplicate()

#include "id_func.h"

typedef enum
{
    F_STAGE_TEXT,
    F_STAGE_ARTSCREEN,
    F_STAGE_CAST,
} finalestage_t;

// ?
//#include "doomstat.h"
//#include "r_local.h"
//#include "f_finale.h"

// Stage of animation:
static finalestage_t finalestage;

static unsigned int finalecount;
static unsigned int finaleendcount;

// [JN] Do screen wipe only once after text skipping.
static boolean finale_wipe_done;

#define	TEXTSPEED	3
#define	TEXTWAIT	250
#define	TEXTEND		25

typedef struct
{
    GameMission_t mission;
    int episode, level;
    const char *background;
    const char *text;
} textscreen_t;

static textscreen_t textscreens[] =
{
    { doom,      1, 8,  "FLOOR4_8",  E1TEXT},
    { doom,      2, 8,  "SFLR6_1",   E2TEXT},
    { doom,      3, 8,  "MFLR8_4",   E3TEXT},
    { doom,      4, 8,  "MFLR8_3",   E4TEXT},
    { doom,      5, 8,  "FLOOR7_2",  E5TEXT}, // [crispy] Sigil
    { doom,      6, 8,  "FLOOR7_2",  E6TEXT}, // [crispy] Sigil II

    { doom2,     1, 6,  "SLIME16",   C1TEXT},
    { doom2,     1, 8,  "SLIME16",   N1TEXT}, // [JN] NERVE
    { doom2,     1, 11, "RROCK14",   C2TEXT},
    { doom2,     1, 20, "RROCK07",   C3TEXT},
    { doom2,     1, 30, "RROCK17",   C4TEXT},
    { doom2,     1, 15, "RROCK13",   C5TEXT},
    { doom2,     1, 31, "RROCK19",   C6TEXT},

    { pack_tnt,  1, 6,  "SLIME16",   T1TEXT},
    { pack_tnt,  1, 11, "RROCK14",   T2TEXT},
    { pack_tnt,  1, 20, "RROCK07",   T3TEXT},
    { pack_tnt,  1, 30, "RROCK17",   T4TEXT},
    { pack_tnt,  1, 15, "RROCK13",   T5TEXT},
    { pack_tnt,  1, 31, "RROCK19",   T6TEXT},

    { pack_plut, 1, 6,  "SLIME16",   P1TEXT},
    { pack_plut, 1, 11, "RROCK14",   P2TEXT},
    { pack_plut, 1, 20, "RROCK07",   P3TEXT},
    { pack_plut, 1, 30, "RROCK17",   P4TEXT},
    { pack_plut, 1, 15, "RROCK13",   P5TEXT},
    { pack_plut, 1, 31, "RROCK19",   P6TEXT},
};

static const char *finaletext;
static const char *finaleflat;
static char *finaletext_rw;

static void F_StartCast (void);
static void F_CastTicker (void);
static void F_CastDrawer (void);
static boolean F_CastResponder (const event_t *ev);

//
// F_StartFinale
//
void F_StartFinale (void)
{
    size_t i;

    gameaction = ga_nothing;
    gamestate = GS_FINALE;
    automapactive = false;
    finale_wipe_done = false;
    players[consoleplayer].cheatTics = 1;
    players[consoleplayer].messageTics = 1;
    players[consoleplayer].message = NULL;
    players[consoleplayer].messageCenteredTics = 1;
    players[consoleplayer].messageCentered = NULL;

    if (logical_gamemission == doom)
    {
        S_ChangeMusic(mus_victor, true);
    }
    else
    {
        S_ID_Change_D2_ReadMusic();
    }

    // Find the right screen and set the text and background

    for (i=0; i<arrlen(textscreens); ++i)
    {
        textscreen_t *screen = &textscreens[i];

        // Hack for Chex Quest

        if (gameversion == exe_chex && screen->mission == doom)
        {
            screen->level = 5;
        }

        if (logical_gamemission == screen->mission
         && (logical_gamemission != doom || gameepisode == screen->episode)
         && gamemap == screen->level)
        {
            finaletext = screen->text;
            finaleflat = screen->background;
        }
    }

    // Do dehacked substitutions of strings
  
    finaletext = DEH_String(finaletext);
    finaleflat = DEH_String(finaleflat);
    // [JN] Count intermission/finale text lenght. Once it's fully printed, 
    // no extra "attack/use" button pressing is needed for skipping.
    finaleendcount = strlen(finaletext) * TEXTSPEED + TEXTEND;
    // [crispy] do the "char* vs. const char*" dance
    if (finaletext_rw)
    {
	free(finaletext_rw);
	finaletext_rw = NULL;
    }
    finaletext_rw = M_StringDuplicate(finaletext);
    
    finalestage = F_STAGE_TEXT;
    finalecount = 0;
	
}



boolean F_Responder (const event_t *event)
{
    if (finalestage == F_STAGE_CAST)
	return F_CastResponder (event);
	
    return false;
}


//
// F_Ticker
//
void F_Ticker (void)
{
    size_t		i;
    
    //
    // [JN] If we are in single player mode, allow double skipping for 
    // intermission text. First skip printing all intermission text,
    // second is advancing to the next state.
    //
    if (singleplayer)
    {
        // [JN] Make PAUSE working properly on text screen
        if (paused)
        {
            return;
        }

        // [JN] Check for skipping. Allow double-press skiping, 
        // but don't skip immediately.
        if (finalecount > 10)
        {
            // go on to the next level
            for (i = 0 ; i < MAXPLAYERS ; i++)
            {
                // [JN] Don't allow to skip bunny screen,
                // and don't allow to skip by pressing "pause" button.
                if ((gameepisode == 3 && finalestage == F_STAGE_ARTSCREEN)
                || players[i].cmd.buttons == (BT_SPECIAL | BTS_PAUSE))
                continue;

                // [JN] Double-skip by pressing "attack" button.
                const boolean old_attackdown = players[i].attackdown;

                if (players[i].cmd.buttons & BT_ATTACK && !menuactive)
                {
                    if (!old_attackdown)
                    {
                        if (finalecount >= finaleendcount)
                        break;
    
                        finalecount += finaleendcount;
                    }
                    players[i].attackdown = true;
                }
                else
                {
                    players[i].attackdown = false;
                }
    
                // [JN] Double-skip by pressing "use" button.
                const boolean old_usedown = players[i].usedown;

                if (players[i].cmd.buttons & BT_USE && !menuactive)
                {
                    if (!old_usedown)
                    {
                        if (finalecount >= finaleendcount)
                        break;
    
                        finalecount += finaleendcount;
                    }
                    players[i].usedown = true;
                }
                else
                {
                    players[i].usedown = false;
                }
            }

            if (i < MAXPLAYERS)
            {
                if (gamemode != commercial)
                {
                    
                    finalestage = F_STAGE_ARTSCREEN;
                    
                    if (!finale_wipe_done)
                    {
                        finale_wipe_done = true;
                        wipegamestate = -1; // force a wipe
                    }

                    if (gameepisode == 3)
                    {
                        finalecount = 0;
                        S_StartMusic (mus_bunny);
                    }
                
                    return;
                }
    
                if (gamemap == 30 || (nerve && gamemap == 8))  // [JN] NERVE
                {
                    F_StartCast ();
                }
                else
                {
                    // [JN] Supress FIRE/USE keystroke upon entering new level.
                    players[i].attackdown = true;
                    players[i].usedown = true;

                    gameaction = ga_worlddone;
                }
            }
        }
    
        // advance animation
        finalecount++;
    
        if (finalestage == F_STAGE_CAST)
        {
            F_CastTicker ();
            return;
        }
    }
    //
    // [JN] Standard Doom routine, safe for network game and demos.
    //        
    else
    {
    // check for skipping
    if ( (gamemode == commercial)
      && ( finalecount > 50) )
    {
      // go on to the next level
      for (i=0 ; i<MAXPLAYERS ; i++)
	if (players[i].cmd.buttons)
	  break;
				
      if (i < MAXPLAYERS)
      {	
	if (gamemap == 30)
	  F_StartCast ();
	else
	  gameaction = ga_worlddone;
      }
    }
    
    // advance animation
    finalecount++;
	
    if (finalestage == F_STAGE_CAST)
    {
	F_CastTicker ();
	return;
    }
	
    if ( gamemode == commercial)
	return;
		
    if (finalestage == F_STAGE_TEXT
     && finalecount>strlen (finaletext)*TEXTSPEED + TEXTWAIT)
    {
	finalecount = 0;
	finalestage = F_STAGE_ARTSCREEN;
	wipegamestate = -1;		// force a wipe
	if (gameepisode == 3)
	    S_StartMusic (mus_bunny);
    }
    }
}



//
// F_TextWrite
//

// [crispy] add line breaks for lines exceeding screenwidth
static inline boolean F_AddLineBreak (char *c)
{
    while (c-- > finaletext_rw)
    {
	if (*c == '\n')
	{
	    return false;
	}
	else
	if (*c == ' ')
	{
	    *c = '\n';
	    return true;
	}
    }

    return false;
}
static void F_TextWrite (void)
{
    byte*	src;
    pixel_t*	dest;

    int		w;
    signed int	count;
    char *ch; // [crispy] un-const
    int		c;
    int		cx;
    int		cy;
    
    // erase the entire screen to a tiled background
    src = W_CacheLumpName ( finaleflat , PU_CACHE);
    dest = I_VideoBuffer;

    // [crispy] use unified flat filling function
    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);
	
    V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);
    
    // draw some of the text onto the screen
    cx = 10;
    cy = 10;
    ch = finaletext_rw;
	
    count = ((signed int) finalecount - 10) / TEXTSPEED;
    if (count < 0)
	count = 0;
    for ( ; count ; count-- )
    {
	c = *ch++;
	if (!c)
	    break;
	if (c == '\n')
	{
	    cx = 10;
	    cy += 11;
	    continue;
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
	    // [crispy] add line breaks for lines exceeding screenwidth
	    if (F_AddLineBreak(ch))
	    {
		continue;
	    }
	    else
	    break;
	}
	// [crispy] prevent text from being drawn off-screen vertically
	if (cy + SHORT(hu_font[c]->height) - SHORT(hu_font[c]->topoffset) >
	    ORIGHEIGHT)
	{
	    break;
	}
	V_DrawShadowedPatchOptional(cx, cy, 0, hu_font[c]);
	cx+=w;
    }
	
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
typedef struct
{
    const char	*name;
    mobjtype_t	type;
} castinfo_t;

castinfo_t	castorder[] = {
    {CC_ZOMBIE, MT_POSSESSED},
    {CC_SHOTGUN, MT_SHOTGUY},
    {CC_HEAVY, MT_CHAINGUY},
    {CC_IMP, MT_TROOP},
    {CC_DEMON, MT_SERGEANT},
    {CC_LOST, MT_SKULL},
    {CC_CACO, MT_HEAD},
    {CC_HELL, MT_KNIGHT},
    {CC_BARON, MT_BRUISER},
    {CC_ARACH, MT_BABY},
    {CC_PAIN, MT_PAIN},
    {CC_REVEN, MT_UNDEAD},
    {CC_MANCU, MT_FATSO},
    {CC_ARCH, MT_VILE},
    {CC_SPIDER, MT_SPIDER},
    {CC_CYBER, MT_CYBORG},
    {CC_HERO, MT_PLAYER},

    {NULL,0}
};

int		castnum;
int		casttics;
state_t*	caststate;
boolean		castdeath;
int		castframes;
int		castonmelee;
boolean		castattacking;


//
// F_StartCast
//
static void F_StartCast (void)
{
    wipegamestate = -1;		// force a screen wipe
    gamestate = GS_THEEND;
    castnum = 0;
    caststate = &states[mobjinfo[castorder[castnum].type].seestate];
    casttics = caststate->tics;
    castdeath = false;
    finalestage = F_STAGE_CAST;
    castframes = 0;
    castonmelee = 0;
    castattacking = false;
    S_ID_Change_D2_CastMusic();
}


//
// F_CastTicker
//
static void F_CastTicker (void)
{
    int		st;
    int		sfx;
	
    if (--casttics > 0)
	return;			// not time to change state yet
		
    if (caststate->tics == -1 || caststate->nextstate == S_NULL)
    {
	// switch from deathstate to next monster
	castnum++;
	castdeath = false;
	if (castorder[castnum].name == NULL)
	    castnum = 0;
	if (mobjinfo[castorder[castnum].type].seesound)
	    S_StartSound (NULL, mobjinfo[castorder[castnum].type].seesound);
	caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	castframes = 0;
    }
    else
    {
	// just advance to next state in animation
	if (caststate == &states[S_PLAY_ATK1])
	    goto stopattack;	// Oh, gross hack!
	st = caststate->nextstate;
	caststate = &states[st];
	castframes++;
	
	// sound hacks....
	switch (st)
	{
	  case S_PLAY_ATK1:	sfx = sfx_dshtgn; break;
	  case S_POSS_ATK2:	sfx = sfx_pistol; break;
	  case S_SPOS_ATK2:	sfx = sfx_shotgn; break;
	  case S_VILE_ATK2:	sfx = sfx_vilatk; break;
	  case S_SKEL_FIST2:	sfx = sfx_skeswg; break;
	  case S_SKEL_FIST4:	sfx = sfx_skepch; break;
	  case S_SKEL_MISS2:	sfx = sfx_skeatk; break;
	  case S_FATT_ATK8:
	  case S_FATT_ATK5:
	  case S_FATT_ATK2:	sfx = sfx_firsht; break;
	  case S_CPOS_ATK2:
	  case S_CPOS_ATK3:
	  case S_CPOS_ATK4:	sfx = sfx_shotgn; break;
	  case S_TROO_ATK3:	sfx = sfx_claw; break;
	  case S_SARG_ATK2:	sfx = sfx_sgtatk; break;
	  case S_BOSS_ATK2:
	  case S_BOS2_ATK2:
	  case S_HEAD_ATK2:	sfx = sfx_firsht; break;
	  case S_SKULL_ATK2:	sfx = sfx_sklatk; break;
	  case S_SPID_ATK2:
	  case S_SPID_ATK3:	sfx = sfx_shotgn; break;
	  case S_BSPI_ATK2:	sfx = sfx_plasma; break;
	  case S_CYBER_ATK2:
	  case S_CYBER_ATK4:
	  case S_CYBER_ATK6:	sfx = sfx_rlaunc; break;
	  case S_PAIN_ATK3:	sfx = sfx_sklatk; break;
	  default: sfx = 0; break;
	}
		
	if (sfx)
	    S_StartSound (NULL, sfx);
    }
	
    if (castframes == 12)
    {
	// go into attack frame
	castattacking = true;
	if (castonmelee)
	    caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
	else
	    caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
	castonmelee ^= 1;
	if (caststate == &states[S_NULL])
	{
	    if (castonmelee)
		caststate=
		    &states[mobjinfo[castorder[castnum].type].meleestate];
	    else
		caststate=
		    &states[mobjinfo[castorder[castnum].type].missilestate];
	}
    }
	
    if (castattacking)
    {
	if (castframes == 24
	    ||	caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
	{
	  stopattack:
	    castattacking = false;
	    castframes = 0;
	    caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	}
    }
	
    casttics = caststate->tics;
    if (casttics == -1)
	casttics = 15;
}


//
// F_CastResponder
//

static boolean F_CastResponder (const event_t *ev)
{
    if (ev->type != ev_keydown)
	return false;
		
    if (castdeath)
	return true;			// already in dying frames
		
    // go into death frame
    castdeath = true;
    caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
    casttics = caststate->tics;
    castframes = 0;
    castattacking = false;
    if (mobjinfo[castorder[castnum].type].deathsound)
	S_StartSound (NULL, mobjinfo[castorder[castnum].type].deathsound);
	
    return true;
}


//
// F_CastDrawer
//

static void F_CastDrawer (void)
{
    spritedef_t*	sprdef;
    spriteframe_t*	sprframe;
    int			lump;
    boolean		flip;
    patch_t*		patch;
    
    // erase the entire screen to a background
    V_DrawPatchFullScreen(W_CacheLumpName(DEH_String("BOSSBACK"), PU_CACHE), false);

    // [JN] Simplify to use common text drawing function.
    M_WriteTextCentered(180, DEH_String(castorder[castnum].name), NULL);
    
    // draw the current frame in the middle of the screen
    sprdef = &sprites[caststate->sprite];
    sprframe = &sprdef->spriteframes[ caststate->frame & FF_FRAMEMASK];
    lump = sprframe->lump[0];
    flip = (boolean)sprframe->flip[0];
			
    patch = W_CacheLumpNum (lump+firstspritelump, PU_CACHE);
    if (flip)
	V_DrawPatchFlipped(ORIGWIDTH/2, 170, patch);
    else
	V_DrawPatch(ORIGWIDTH/2, 170, patch);
}


//
// F_DrawPatchCol
//
static fixed_t dxi, dy, dyi;

static void
F_DrawPatchCol
( int		x,
  patch_t*	patch,
  int		col )
{
    column_t*	column;
    const byte*	source;
    pixel_t*	dest;
    pixel_t*	desttop;
    int		count;
	
    column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));
    desttop = I_VideoBuffer + x;

    // step through the posts in a column
    while (column->topdelta != 0xff )
    {
	int srccol = 0;
	source = (byte *)column + 3;
	dest = desttop + ((column->topdelta * dy) >> FRACBITS)*SCREENWIDTH;
	count = (column->length * dy) >> FRACBITS;
		
	while (count--)
	{
	    *dest = pal_color[source[srccol >> FRACBITS]];
	    srccol += dyi;
	    dest += SCREENWIDTH;
	}
	column = (column_t *)(  (byte *)column + column->length + 4 );
    }
}


//
// F_BunnyScroll
//
static void F_BunnyScroll (void)
{
    signed int  scrolled;
    int		x;
    patch_t*	p1;
    patch_t*	p2;
    char	name[10];
    int		stage;
    static int	laststage;
    int         p2offset, p1offset, pillar_width;
		
    dxi = (ORIGWIDTH << FRACBITS) / NONWIDEWIDTH;
    dy = (SCREENHEIGHT << FRACBITS) / ORIGHEIGHT;
    dyi = (ORIGHEIGHT << FRACBITS) / SCREENHEIGHT;

    p1 = W_CacheLumpName (DEH_String("PFUB2"), PU_LEVEL);
    p2 = W_CacheLumpName (DEH_String("PFUB1"), PU_LEVEL);

    // [crispy] fill pillarboxes in widescreen mode
    pillar_width = (SCREENWIDTH - (SHORT(p1->width) << FRACBITS) / dxi) / 2;

    if (pillar_width > 0)
    {
        V_DrawFilledBox(0, 0, pillar_width, SCREENHEIGHT, 0);
        V_DrawFilledBox(SCREENWIDTH - pillar_width, 0, pillar_width, SCREENHEIGHT, 0);
    }
    else
    {
        pillar_width = 0;
    }

    // Calculate the portion of PFUB2 that would be offscreen at original res.
    p1offset = (ORIGWIDTH - SHORT(p1->width)) / 2;

    if (SHORT(p2->width) == ORIGWIDTH)
    {
        // Unity or original PFUBs.
        // PFUB1 only contains the pixels that scroll off.
        p2offset = ORIGWIDTH - p1offset;
    }
    else
    {
        // Widescreen mod PFUBs.
        // Right side of PFUB2 and left side of PFUB1 are identical.
        p2offset = ORIGWIDTH + p1offset;
    }

    V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);
	
    scrolled = (ORIGWIDTH - ((signed int) finalecount-230)/2);
    if (scrolled > ORIGWIDTH)
	scrolled = ORIGWIDTH;
    if (scrolled < 0)
	scrolled = 0;

    for (x = pillar_width; x < SCREENWIDTH - pillar_width; x++)
    {
        // [JN] Added -1 to support higher rendering resolutions and wide screen modes.
        const int x2 = ((x * dxi) >> FRACBITS) - WIDESCREENDELTA + scrolled - (scrolled ? 1 : 0);

        if (x2 < p2offset)
            F_DrawPatchCol (x, p1, x2 - p1offset);
        else
            F_DrawPatchCol (x, p2, x2 - p2offset);
    }
	
    if (finalecount < 1130)
	return;
    if (finalecount < 1180)
    {
        V_DrawPatch((ORIGWIDTH - 13 * 8) / 2,
                    (ORIGHEIGHT - 8 * 8) / 2,
                    W_CacheLumpName(DEH_String("END0"), PU_CACHE));
	laststage = 0;
	return;
    }
	
    stage = (finalecount-1180) / 5;
    if (stage > 6)
	stage = 6;
    if (stage > laststage)
    {
	S_StartSound (NULL, sfx_pistol);
	laststage = stage;
    }
	
    DEH_snprintf(name, 10, "END%i", stage);
    V_DrawPatch((ORIGWIDTH - 13 * 8) / 2,
                (ORIGHEIGHT - 8 * 8) / 2,
                W_CacheLumpName (name,PU_CACHE));
}

static void F_ArtScreenDrawer(void)
{
    const char *lumpname;
    
    if (gameepisode == 3)
    {
        gamestate = GS_THEEND;
        F_BunnyScroll();
    }
    else
    {
        switch (gameepisode)
        {
            case 1:
                if (gameversion >= exe_ultimate)
                {
                    lumpname = "CREDIT";
                }
                else
                {
                    lumpname = "HELP2";
                }
                break;
            case 2:
                lumpname = "VICTORY2";
                break;
            case 4:
                lumpname = "ENDPIC";
                break;
            // [crispy] Sigil
            case 5:
                lumpname = "SIGILEND";
                if (W_CheckNumForName(DEH_String(lumpname)) == -1)
                {
                    return;
                }
                break;
            // [crispy] Sigil II
            case 6:
                lumpname = "SGL2END";
                if (W_CheckNumForName(DEH_String(lumpname)) == -1)
                {
                    lumpname = "SIGILEND";

                    if (W_CheckNumForName(DEH_String(lumpname)) == -1)
                    {
                        return;
                    }
                }
                break;
            default:
                return;
        }

        lumpname = DEH_String(lumpname);

        V_DrawPatchFullScreen (W_CacheLumpName(lumpname, PU_CACHE), false);
    }
}

//
// F_Drawer
//
void F_Drawer (void)
{
    switch (finalestage)
    {
        case F_STAGE_CAST:
            F_CastDrawer();
            break;
        case F_STAGE_TEXT:
            F_TextWrite();
            break;
        case F_STAGE_ARTSCREEN:
            F_ArtScreenDrawer();
            break;
    }
}


