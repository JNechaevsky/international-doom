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


// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "p_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void RunThinkers(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int leveltime;
int TimerGame;
thinker_t thinkercap;           // The head and tail of the thinker list

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_Ticker
//
//==========================================================================

void P_Ticker(void)
{
    int i;

    if (players[displayplayer].targetsheathTics > 0)
    {
        players[displayplayer].targetsheathTics--;
    }
    if (players[consoleplayer].cheatTics > 0)
    {
        players[consoleplayer].cheatTics--;
    }
    if (paused)
    {
        return;
    }
    for (i = 0; i < maxplayers; i++)
    {
        if (playeringame[i])
        {
            P_PlayerThink(&players[i]);
        }
    }
    if (TimerGame)
    {
        if (!--TimerGame)
        {
            G_Completed(P_TranslateMap(P_GetMapNextMap(gamemap)), 0);
        }
    }
    RunThinkers();
    // [JN] CRL - do not update mobjs and thinkers in freeze mode.
    if (!crl_freeze)
    {
        P_UpdateSpecials();
        P_AnimateSurfaces();
        leveltime++;
    }

    realleveltime++;
}

//==========================================================================
//
// RunThinkers
//
//==========================================================================

static void RunThinkers(void)
{
    thinker_t *currentthinker, *nextthinker;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
        // [JN] CRL - do not run other than player thinkers in freeze mode.
        if (crl_freeze)
        {
            const mobj_t *const mo = (mobj_t *)currentthinker;

            if ((mo->type != MT_PLAYER_FIGHTER
            &&   mo->type != MT_PLAYER_CLERIC
            &&   mo->type != MT_PLAYER_MAGE)
            ||   currentthinker->function != P_MobjThinker)
            {
                goto skip;
            }
        }

        if (currentthinker->function == (think_t) - 1)
        {                       // Time to remove it
            nextthinker = currentthinker->next;
            currentthinker->next->prev = currentthinker->prev;
            currentthinker->prev->next = currentthinker->next;
            Z_Free(currentthinker);
        }
        else
        {
            if (currentthinker->function)
                currentthinker->function(currentthinker);

            skip:
            nextthinker = currentthinker->next;
        }

        currentthinker = nextthinker;
    }
}

//==========================================================================
//
// P_InitThinkers
//
//==========================================================================

void P_InitThinkers(void)
{
    thinkercap.prev = thinkercap.next = &thinkercap;
}

//==========================================================================
//
// P_AddThinker
//
// Adds a new thinker at the end of the list.
//
//==========================================================================

void P_AddThinker(thinker_t * thinker)
{
    thinkercap.prev->next = thinker;
    thinker->next = &thinkercap;
    thinker->prev = thinkercap.prev;
    thinkercap.prev = thinker;
}

//==========================================================================
//
// P_RemoveThinker
//
// Deallocation is lazy -- it will not actually be freed until its
// thinking turn comes up.
//
//==========================================================================

void P_RemoveThinker(thinker_t * thinker)
{
    thinker->function = (think_t) - 1;
}
