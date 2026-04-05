//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
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
//	Teleportation.
//


#include "doomstat.h"
#include "s_sound.h"
#include "p_local.h"
#include <stdlib.h>


//
// TELEPORTATION
//
int
EV_Teleport
( const line_t*	line,
  int		side,
  mobj_t*	thing )
{
    int		i;
    int		tag;
    mobj_t*	m;
    mobj_t*	fog;
    unsigned	an;
    thinker_t*	thinker;
    const sector_t*	sector;
    fixed_t	oldx;
    fixed_t	oldy;
    fixed_t	oldz;

    // don't teleport missiles
    if (thing->flags & MF_MISSILE)
	return 0;		

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side == 1)		
	return 0;	

    
    tag = line->tag;
    for (i = 0; i < numsectors; i++)
    {
	if (sectors[ i ].tag == tag )
	{
	    for (thinker = thinkercap.next;
		 thinker != &thinkercap;
		 thinker = thinker->next)
	    {
		// not a mobj
		if (thinker->function.acp1 != (actionf_p1)P_MobjThinker)
		    continue;	

		m = (mobj_t *)thinker;
		
		// not a teleportman
		if (m->type != MT_TELEPORTMAN )
		    continue;		

		sector = m->subsector->sector;
		// wrong sector
		if (sector-sectors != i )
		    continue;	

		oldx = thing->x;
		oldy = thing->y;
		oldz = thing->z;
				
		if (!P_TeleportMove (thing, m->x, m->y))
		    return 0;

                // The first Final Doom executable does not set thing->z
                // when teleporting. This quirk is unique to this
                // particular version; the later version included in
                // some versions of the Id Anthology fixed this.

                if (gameversion != exe_final)
		    thing->z = thing->floorz;

		if (thing->player)
		{
		    thing->player->viewz = thing->z+thing->player->viewheight;
		    // [crispy] center view after teleporting
		    // thing->player->centering = true;
            // [JN] Center view immediately.
            thing->player->lookdir = 0;
		}

		// spawn teleport fog at source and destination
		fog = P_SpawnMobj (oldx, oldy, oldz, MT_TFOG);
		S_StartSound (fog, sfx_telept);
		an = m->angle >> ANGLETOFINESHIFT;
		fog = P_SpawnMobj (m->x+20*finecosine[an], m->y+20*finesine[an]
				   , thing->z, MT_TFOG);

		// emit sound, where?
		S_StartSound (fog, sfx_telept);
		
		// don't move for a bit
		if (thing->player)
		    thing->reactiontime = 18;	

		thing->angle = m->angle;
		thing->momx = thing->momy = thing->momz = 0;
		return 1;
	    }	
	}
    }
    return 0;
}

// -----------------------------------------------------------------------------
// EV_SilentTeleport
//  [PN] Boom silent teleporter with teleport destination thing.
// -----------------------------------------------------------------------------
int EV_SilentTeleport(const line_t* line, int side, mobj_t* thing)
{
    int i;
    thinker_t* thinker;

    if (side || (thing->flags & MF_MISSILE))
    {
        return 0;
    }

    for (i = -1; (i = P_FindSectorFromLineTag(line, i)) >= 0;)
    {
        for (thinker = thinkercap.next; thinker != &thinkercap; thinker = thinker->next)
        {
            mobj_t* m;
            fixed_t z;
            angle_t angle;
            fixed_t s;
            fixed_t c;
            fixed_t momx;
            fixed_t momy;
            player_t* player;

            if (thinker->function.acp1 != (actionf_p1) P_MobjThinker)
            {
                continue;
            }

            m = (mobj_t*) thinker;

            if (m->type != MT_TELEPORTMAN || m->subsector->sector - sectors != i)
            {
                continue;
            }

            z = thing->z - thing->floorz;
            angle = R_PointToAngle2(0, 0, line->dx, line->dy) - m->angle + ANG90;
            s = finesine[angle >> ANGLETOFINESHIFT];
            c = finecosine[angle >> ANGLETOFINESHIFT];
            momx = thing->momx;
            momy = thing->momy;
            player = (thing->player && thing->player->mo == thing) ? thing->player : NULL;

            if (!P_TeleportMove(thing, m->x, m->y))
            {
                return 0;
            }

            thing->angle += angle;
            thing->z = z + thing->floorz;
            thing->momx = FixedMul(momx, c) - FixedMul(momy, s);
            thing->momy = FixedMul(momy, c) + FixedMul(momx, s);

            if (player)
            {
                player->viewz = thing->z + player->viewheight;
            }

            return 1;
        }
    }

    return 0;
}

// -----------------------------------------------------------------------------
// EV_SilentLineTeleport
//  [PN] Boom silent line-to-line teleporter (normal and reversed variants).
// -----------------------------------------------------------------------------
int EV_SilentLineTeleport(line_t* line, int side, mobj_t* thing, boolean reverse)
{
    int i;

    if (side || (thing->flags & MF_MISSILE))
    {
        return 0;
    }

    for (i = 0; i < numlines; ++i)
    {
        line_t* dstline = &lines[i];
        fixed_t pos;
        angle_t angle;
        fixed_t x;
        fixed_t y;
        fixed_t s;
        fixed_t c;
        int fudge;
        player_t* player;
        int stepdown;
        fixed_t z;
        int exitside;

        if (dstline == line || dstline->tag != line->tag || !dstline->backsector)
        {
            continue;
        }

        pos = abs(line->dx) > abs(line->dy)
            ? FixedDiv(thing->x - line->v1->x, line->dx)
            : FixedDiv(thing->y - line->v1->y, line->dy);

        if (reverse)
        {
            pos = FRACUNIT - pos;
            angle = R_PointToAngle2(0, 0, dstline->dx, dstline->dy)
                  - R_PointToAngle2(0, 0, line->dx, line->dy);
        }
        else
        {
            angle = ANG180
                  + R_PointToAngle2(0, 0, dstline->dx, dstline->dy)
                  - R_PointToAngle2(0, 0, line->dx, line->dy);
        }

        x = dstline->v2->x - FixedMul(pos, dstline->dx);
        y = dstline->v2->y - FixedMul(pos, dstline->dy);
        s = finesine[angle >> ANGLETOFINESHIFT];
        c = finecosine[angle >> ANGLETOFINESHIFT];
        fudge = 10;
        player = (thing->player && thing->player->mo == thing) ? thing->player : NULL;
        stepdown = dstline->frontsector->floorheight < dstline->backsector->floorheight;
        z = thing->z - thing->floorz;
        exitside = reverse || (player && stepdown);

        while (P_PointOnLineSide(x, y, dstline) != exitside && --fudge >= 0)
        {
            if (abs(dstline->dx) > abs(dstline->dy))
            {
                y -= ((dstline->dx < 0) != exitside) ? -1 : 1;
            }
            else
            {
                x += ((dstline->dy < 0) != exitside) ? -1 : 1;
            }
        }

        if (!P_TeleportMove(thing, x, y))
        {
            return 0;
        }

        thing->z = z + sides[dstline->sidenum[stepdown]].sector->floorheight;
        thing->angle += angle;

        x = thing->momx;
        y = thing->momy;
        thing->momx = FixedMul(x, c) - FixedMul(y, s);
        thing->momy = FixedMul(y, c) + FixedMul(x, s);

        if (player)
        {
            player->viewz = thing->z + player->viewheight;
        }

        return 1;
    }

    return 0;
}
