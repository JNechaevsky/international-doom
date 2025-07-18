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
//	Archiving: SaveGame I/O.
//	Thinker, Ticker.
//


#include "z_zone.h"
#include "p_local.h"
#include "doomstat.h"
#include "ct_chat.h"
#include "m_random.h"

#include "id_vars.h"


// [JN] CRL - now there are two timers:
// 1) leveltime: used by all mobjs, thinkers and level time.
//    Not ticking in Freeze mode.
// 2) realleveltime: used by player, render and interpolation.
//    Always ticking.
int	leveltime;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//



// Both the head and tail of the thinker list.
thinker_t	thinkercap;


//
// P_InitThinkers
//
void P_InitThinkers (void)
{
    thinkercap.prev = thinkercap.next  = &thinkercap;
}




//
// P_AddThinker
// Adds a new thinker at the end of the list.
//
void P_AddThinker (thinker_t* thinker)
{
    thinkercap.prev->next = thinker;
    thinker->next = &thinkercap;
    thinker->prev = thinkercap.prev;
    thinkercap.prev = thinker;
}



//
// P_RemoveThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void P_RemoveThinker (thinker_t* thinker)
{
  // FIXME: NOP.
  thinker->function.acv = (actionf_v)(-1);
}



//
// P_RunThinkers
//
static void P_RunThinkers (void)
{
    thinker_t *currentthinker, *nextthinker;
    static int bmap_count_common;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
	mobj_t *mo = (mobj_t *)currentthinker;

	// [JN] CRL - do not run other than player thinkers in freeze mode.
	if (crl_freeze)
	{
	    if (mo->type != MT_PLAYER || currentthinker->function.acp1 != (actionf_p1) P_MobjThinker)
	    {
	        goto skip;
	    }
	}
	// [JN] Animate brightmaps.
	// Note: not exactly a good place for handling render-specific properties,
	// but we don't want make a separate run through all the thinkers and
	// animation must be framerate independent.
	if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker)
	{
	    if (vis_brightmaps)
	    {
	        if (bmap_count_common == 1)
	        {
	            // [JN] Random brightmap flickering effect.
	            if (mo->sprite == SPR_CAND  // Candestick
	            ||  mo->sprite == SPR_CBRA  // Candelabra
	            ||  mo->sprite == SPR_FCAN  // Flaming Barrel
	            ||  mo->sprite == SPR_TBLU  // Tall Blue Torch
	            ||  mo->sprite == SPR_TGRN  // Tall Green Torch
	            ||  mo->sprite == SPR_TRED  // Tall Red Torch
	            ||  mo->sprite == SPR_SMBT  // Short Blue Torch
	            ||  mo->sprite == SPR_SMGT  // Short Green Torch
	            ||  mo->sprite == SPR_SMRT  // Short Red Torch
	            ||  mo->sprite == SPR_POL3) // Pile of Skulls and Candles
	            {
	                mo->bmap_flick = ID_RealRandom() % 16;
	            }
	            // [JN] Smooth brightmap glowing effect.
	            if (mo->sprite == SPR_FCAN  // Flaming Barrel
	            ||  mo->sprite == SPR_CEYE  // Evil Eye
	            ||  mo->sprite == SPR_FSKU) // Floating Skull Rock
	            {
	                mo->bmap_glow = ID_RealRandom() % 6;
	            }
	        }
	    }
	    else
	    {
	    	mo->bmap_flick = 0;
	    	mo->bmap_glow = 0;
	    }
	}

	if ( currentthinker->function.acv == (actionf_v)(-1) )
	{
	    // time to remove it
            nextthinker = currentthinker->next;
	    currentthinker->next->prev = currentthinker->prev;
	    currentthinker->prev->next = currentthinker->next;
	    Z_Free(currentthinker);
	}
	else
	{
	    if (currentthinker->function.acp1)
		currentthinker->function.acp1 (currentthinker);

            skip:
            nextthinker = currentthinker->next;
	}
	currentthinker = nextthinker;
    }
    
	// [JN] Reset brightmap timer.
	if (++bmap_count_common == 4)
	{
	    bmap_count_common = 0;
	}
}



//
// P_Ticker
//

void P_Ticker (void)
{
    int		i;
    
    if (players[displayplayer].targetsheathTics > 0)
    {
        players[displayplayer].targetsheathTics--;
    }
    if (players[consoleplayer].cheatTics > 0)
    {
        players[consoleplayer].cheatTics--;
    }

    
    // run the tic
    if (paused)
	return;
		
    // pause if in menu and at least one tic has been run
    if ( !netgame
	 && menuactive
	 && !demoplayback
	 && players[consoleplayer].viewz != 1)
    {
	return;
    }
    
		
    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i])
	    P_PlayerThink (&players[i]);
			
    P_RunThinkers ();
    
    // [JN] CRL - do not update mobjs and thinkers in freeze mode.
    if (!crl_freeze)
    {
    P_UpdateSpecials ();
    P_RespawnSpecials ();

    // for par times
    leveltime++;	
    }

    realleveltime++;
}
