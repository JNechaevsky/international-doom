//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

#include "doomdef.h"
#include "deh_str.h"
#include "i_system.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_video.h"
#include "i_swap.h" // [crispy] SHORT()
#include "w_wad.h" // [crispy] W_CheckNumForName()
#include "z_zone.h" // [crispy] Z_ChangeTag()

//==================================================================
//
//      CHANGE THE TEXTURE OF A WALL SWITCH TO ITS OPPOSITE
//
//==================================================================
// [crispy] add support for SWITCHES lumps
switchlist_t alphSwitchList_vanilla[] = {
    {"SW1OFF", "SW1ON", 1},
    {"SW2OFF", "SW2ON", 1},

/*
	{"SW1CTY",		"SW2CTY",	1},
	{"SW1ORGRY",	"SW2ORGRY",	1},
	{"SW1GRSTN",	"SW2GRSTN",	1},
	{"SW1SNDP",		"SW2SNDP",	1},
	{"SW1SPINE",	"SW2SPINE",	1},
	{"SW1SQPEB",	"SW2SQPEB",	1},
	{"SW1TRST1",	"SW2TRST1",	1},
	{"SW1CSTL",		"SW2CSTL",	1},
	{"SW1MOSS",		"SW2MOSS",	1},
	{"SW1SNDSQ",	"SW2SNDSQ",	1},
	{"SW1RED",		"SW2RED",	1},
	{"SW1WOOD",		"SW2WOOD",	1},
	{"SW1BROWN",	"SW2BROWN",	1},

	{"SW1TRST2",	"SW2TRST2",	2},
	{"SW1MSC",		"SW2MSC",	2},
	{"SW1MSC2",		"SW2MSC2",	2},
	{"SW1GRDMD",	"SW2GRDMD",	2},
*/

#if 0
    {"SW1BRCOM", "SW2BRCOM", 1},
    {"SW1BRN1", "SW2BRN1", 1},
    {"SW1BRN2", "SW2BRN2", 1},
    {"SW1BRNGN", "SW2BRNGN", 1},
    {"SW1BROWN", "SW2BROWN", 1},
    {"SW1COMM", "SW2COMM", 1},
    {"SW1COMP", "SW2COMP", 1},
    {"SW1DIRT", "SW2DIRT", 1},
    {"SW1EXIT", "SW2EXIT", 1},
    {"SW1GRAY", "SW2GRAY", 1},
    {"SW1GRAY1", "SW2GRAY1", 1},
    {"SW1METAL", "SW2METAL", 1},
    {"SW1PIPE", "SW2PIPE", 1},
    {"SW1SLAD", "SW2SLAD", 1},
    {"SW1STARG", "SW2STARG", 1},
    {"SW1STON1", "SW2STON1", 1},
    {"SW1STON2", "SW2STON2", 1},
    {"SW1STONE", "SW2STONE", 1},
    {"SW1STRTN", "SW2STRTN", 1},

    {"SW1BLUE", "SW2BLUE", 2},
    {"SW1CMT", "SW2CMT", 2},
    {"SW1GARG", "SW2GARG", 2},
    {"SW1GSTON", "SW2GSTON", 2},
    {"SW1HOT", "SW2HOT", 2},
    {"SW1LION", "SW2LION", 2},
    {"SW1SATYR", "SW2SATYR", 2},
    {"SW1SKIN", "SW2SKIN", 2},
    {"SW1VINE", "SW2VINE", 2},
    {"SW1WOOD", "SW2WOOD", 2},
#endif
    // [crispy] SWITCHES lumps are supposed to end like this
    {"\0",		"\0",		0}
};

// [crispy] remove MAXSWITCHES limit
int		*switchlist;
int		numswitches;
static size_t	maxswitches;
button_t *buttonlist; // [crispy] remove MAXBUTTONS limit
int       maxbuttons; // [crispy] remove MAXBUTTONS limit

/*
===============
=
= P_InitSwitchList
=
= Only called at game initialization
=
===============
*/

void P_InitSwitchList(void)
{
    int i, slindex, episode;
    
    // [crispy] add support for SWITCHES lumps
    switchlist_t *alphSwitchList;
    boolean from_lump;

    if ((from_lump = (W_CheckNumForName("SWITCHES") != -1)))
    {
        alphSwitchList = W_CacheLumpName("SWITCHES", PU_STATIC);
    }
    else
    {
        alphSwitchList = alphSwitchList_vanilla;
    }
    
    // Note that this is called "episode" here but it's actually something
    // quite different. As we progress from Shareware->Registered->Doom II
    // we support more switch textures.
    if (gamemode == shareware)
    {
        episode = 1;
    }
    else
    {
        episode = 2;
    }

    slindex = 0;

    for (i = 0; alphSwitchList[i].episode; i++)
    {
	const short alphSwitchList_episode = from_lump ?
	    SHORT(alphSwitchList[i].episode) :
	    alphSwitchList[i].episode;

	// [crispy] remove MAXSWITCHES limit
	if (slindex + 1 >= maxswitches)
	{
	    size_t newmax = maxswitches ? 2 * maxswitches : MAXSWITCHES;
	    switchlist = I_Realloc(switchlist, newmax * sizeof(*switchlist));
	    maxswitches = newmax;
	}
	
	// [crispy] ignore switches referencing unknown texture names,
	// warn if either one is missing, but only add if both are valid
	if (alphSwitchList_episode <= episode)
	{
	    int texture1, texture2;
	    const char *name1 = DEH_String(alphSwitchList[i].name1);
	    const char *name2 = DEH_String(alphSwitchList[i].name2);

	    texture1 = R_CheckTextureNumForName(name1);
	    texture2 = R_CheckTextureNumForName(name2);

	    if (texture1 == -1 || texture2 == -1)
	    {
		fprintf(stderr, "P_InitSwitchList: could not add %s(%d)/%s(%d)\n",
		        name1, texture1, name2, texture2);
	    }
	    else
	    {
		switchlist[slindex++] = texture1;
		switchlist[slindex++] = texture2;
	    }
	}
    }

    numswitches = slindex / 2;
    switchlist[slindex] = -1;
    // [crispy] add support for SWITCHES lumps
    if (from_lump)
    {
        W_ReleaseLumpName("SWITCHES");
    }

    // [crispy] pre-allocate some memory for the buttonlist[] array
    maxbuttons = MAXBUTTONS;
    buttonlist = I_Realloc(NULL, sizeof(*buttonlist) * maxbuttons);
    memset(buttonlist, 0, sizeof(*buttonlist) * maxbuttons);
}

//==================================================================
//
//      Start a button counting down till it turns off.
//
//==================================================================
static void P_StartButton(line_t * line, bwhere_e w, int texture, int time)
{
    int i;

    for (i = 0; i < maxbuttons; i++)
        if (!buttonlist[i].btimer)
        {
            buttonlist[i].line = line;
            buttonlist[i].where = w;
            buttonlist[i].btexture = texture;
            buttonlist[i].btimer = time;
            // [JN] [crispy] Corrected sound source.
            buttonlist[i].soundorg = &line->soundorg;
            return;
        }

    // [crispy] remove MAXBUTTONS limit
    {
        maxbuttons = 2 * maxbuttons;
        buttonlist = I_Realloc(buttonlist, sizeof(*buttonlist) * maxbuttons);
        memset(buttonlist + maxbuttons/2, 0, sizeof(*buttonlist) * maxbuttons/2);
        P_StartButton(line, w, texture, time);
        // [JN] Separate return to fix -Wpedantic GCC compiler warning:
        // ISO C forbids 'return' with expression, in function returning void
        return;
    }

    // I_Error("P_StartButton: no button slots left!");
}

//==================================================================
//
//      Function that changes wall texture.
//      Tell it if switch is ok to use again (1=yes, it's a button).
//
//==================================================================
void P_ChangeSwitchTexture(line_t * line, int useAgain)
{
    int texTop;
    int texMid;
    int texBot;
    int i;
    int sound;
    // [crispy] register up to three buttons at once 
    // for lines with more than one switch texture.
    boolean playsound = false;

    if (!useAgain)
        line->special = 0;

    texTop = sides[line->sidenum[0]].toptexture;
    texMid = sides[line->sidenum[0]].midtexture;
    texBot = sides[line->sidenum[0]].bottomtexture;

    sound = sfx_switch;
    //if (line->special == 11) // EXIT SWITCH?
    //      sound = sfx_swtchx;

    for (i = 0; i < numswitches * 2; i++)
    {
        if (switchlist[i] == texTop)
        {
            playsound = true;
            sides[line->sidenum[0]].toptexture = switchlist[i ^ 1];
            if (useAgain)
                P_StartButton(line, top, switchlist[i], BUTTONTIME);
        }
        else if (switchlist[i] == texMid)
        {
            playsound = true;
            sides[line->sidenum[0]].midtexture = switchlist[i ^ 1];
            if (useAgain)
                P_StartButton(line, middle, switchlist[i], BUTTONTIME);
        }
        else if (switchlist[i] == texBot)
        {
            playsound = true;
            sides[line->sidenum[0]].bottomtexture = switchlist[i ^ 1];
            if (useAgain)
                P_StartButton(line, bottom, switchlist[i], BUTTONTIME);
        }
    }

    // [crispy] corrected sound source
    if (playsound)
    {
        // [JN] Z-axis sfx distance: sound invoked from the floor segmented source
        if (line->backsector 
        &&  line->backsector->floorheight > line->frontsector->floorheight)
        {
            line->soundorg.z = (line->backsector->floorheight 
                             -  line->frontsector->floorheight) / 2;
        }
        // [JN] Z-axis sfx distance: sound invoked from the ceiling segmented source
        else 
        if (line->backsector 
        &&  line->backsector->ceilingheight < line->frontsector->ceilingheight)
        {
            line->soundorg.z = (line->frontsector->ceilingheight
                             +  line->backsector->ceilingheight) / 2;
        }
        // [JN] Z-axis sfx distance: sound invoked from the middle of the line
        else
        {
            line->soundorg.z = (line->frontsector->ceilingheight
                             +  line->frontsector->floorheight) / 2;
        }

        S_StartSound(buttonlist->soundorg, sound);
    }
}

/*
==============================================================================
=
= P_UseSpecialLine
=
= Called when a thing uses a special line
= Only the front sides of lines are usable
===============================================================================
*/

boolean P_UseSpecialLine(mobj_t * thing, line_t * line)
{
    //
    //      Switches that other things can activate
    //
    if (!thing->player)
    {
        if (line->flags & ML_SECRET)
            return false;       // never open secret doors
        switch (line->special)
        {
            case 1:            // MANUAL DOOR RAISE
            case 32:           // MANUAL BLUE
            case 33:           // MANUAL RED
            case 34:           // MANUAL YELLOW
                break;
            default:
                return false;
        }
    }

    //
    // do something
    //      
    switch (line->special)
    {
            //===============================================
            //      MANUALS
            //===============================================
        case 1:                // Vertical Door
        case 26:               // Blue Door/Locked
        case 27:               // Yellow Door /Locked
        case 28:               // Red Door /Locked

        case 31:               // Manual door open
        case 32:               // Blue locked door open
        case 33:               // Red locked door open
        case 34:               // Yellow locked door open
            EV_VerticalDoor(line, thing);
            break;
            //===============================================
            //      SWITCHES
            //===============================================
        case 7:                // Switch_Build_Stairs (8 pixel steps)
            if (EV_BuildStairs(line, 8 * FRACUNIT))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            break;
        case 107:              // Switch_Build_Stairs_16 (16 pixel steps)
            if (EV_BuildStairs(line, 16 * FRACUNIT))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            break;
        case 9:                // Change Donut
            if (EV_DoDonut(line))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 11:               // Exit level
            G_ExitLevel();
            P_ChangeSwitchTexture(line, 0);
            break;
        case 14:               // Raise Floor 32 and change texture
            if (EV_DoPlat(line, raiseAndChange, 32))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 15:               // Raise Floor 24 and change texture
            if (EV_DoPlat(line, raiseAndChange, 24))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 18:               // Raise Floor to next highest floor
            if (EV_DoFloor(line, raiseFloorToNearest))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 20:               // Raise Plat next highest floor and change texture
            if (EV_DoPlat(line, raiseToNearestAndChange, 0))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 21:               // PlatDownWaitUpStay
            if (EV_DoPlat(line, downWaitUpStay, 0))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 23:               // Lower Floor to Lowest
            if (EV_DoFloor(line, lowerFloorToLowest))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 29:               // Raise Door
            if (EV_DoDoor(line, vld_normal, VDOORSPEED))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 41:               // Lower Ceiling to Floor
            if (EV_DoCeiling(line, lowerToFloor))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 71:               // Turbo Lower Floor
            if (EV_DoFloor(line, turboLower))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 49:               // Lower Ceiling And Crush
            if (EV_DoCeiling(line, lowerAndCrush))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 50:               // Close Door
            if (EV_DoDoor(line, vld_close, VDOORSPEED))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 51:               // Secret EXIT
            G_SecretExitLevel();
            P_ChangeSwitchTexture(line, 0);
            break;
        case 55:               // Raise Floor Crush
            if (EV_DoFloor(line, raiseFloorCrush))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 101:              // Raise Floor
            if (EV_DoFloor(line, raiseFloor))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 102:              // Lower Floor to Surrounding floor height
            if (EV_DoFloor(line, lowerFloor))
                P_ChangeSwitchTexture(line, 0);
            break;
        case 103:              // Open Door
            if (EV_DoDoor(line, vld_open, VDOORSPEED))
                P_ChangeSwitchTexture(line, 0);
            break;
            //===============================================
            //      BUTTONS
            //===============================================
        case 42:               // Close Door
            if (EV_DoDoor(line, vld_close, VDOORSPEED))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 43:               // Lower Ceiling to Floor
            if (EV_DoCeiling(line, lowerToFloor))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 45:               // Lower Floor to Surrounding floor height
            if (EV_DoFloor(line, lowerFloor))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 60:               // Lower Floor to Lowest
            if (EV_DoFloor(line, lowerFloorToLowest))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 61:               // Open Door
            if (EV_DoDoor(line, vld_open, VDOORSPEED))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 62:               // PlatDownWaitUpStay
            if (EV_DoPlat(line, downWaitUpStay, 1))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 63:               // Raise Door
            if (EV_DoDoor(line, vld_normal, VDOORSPEED))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 64:               // Raise Floor to ceiling
            if (EV_DoFloor(line, raiseFloor))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 66:               // Raise Floor 24 and change texture
            if (EV_DoPlat(line, raiseAndChange, 24))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 67:               // Raise Floor 32 and change texture
            if (EV_DoPlat(line, raiseAndChange, 32))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 65:               // Raise Floor Crush
            if (EV_DoFloor(line, raiseFloorCrush))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 68:               // Raise Plat to next highest floor and change texture
            if (EV_DoPlat(line, raiseToNearestAndChange, 0))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 69:               // Raise Floor to next highest floor
            if (EV_DoFloor(line, raiseFloorToNearest))
                P_ChangeSwitchTexture(line, 1);
            break;
        case 70:               // Turbo Lower Floor
            if (EV_DoFloor(line, turboLower))
                P_ChangeSwitchTexture(line, 1);
            break;

        //
        // [JN] H+H Specials:
        //

        case 159:
            if (EV_DoFloor(line, hh_159))
                P_ChangeSwitchTexture(line, 0);
            break;

        case 161:               
            if (EV_DoFloor(line, hh_161))
                P_ChangeSwitchTexture(line, 0);
            break;

        case 164:
            if (EV_DoCeiling(line, fastCrushAndRaise))
                P_ChangeSwitchTexture(line, 0);
            break;

        case 24722:
            if (EV_DoFloor(line, hh_24722))
                P_ChangeSwitchTexture(line, 0);
            break;
    }

    return true;
}
