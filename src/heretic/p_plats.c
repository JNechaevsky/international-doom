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

// P_plats.c

#include "doomdef.h"
#include "i_system.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_video.h"

static void P_ActivateInStasis(int tag);
static void P_RemoveActivePlat(const plat_t *plat);

plat_t *activeplats[MAXPLATS];

//==================================================================
//
//      Move a plat up and down
//
//==================================================================
void T_PlatRaise(thinker_t *thinker)
{
    plat_t *plat = (plat_t *) thinker;
    result_e res;

    // [JN] Z-axis sfx distance: sound invoked from the floor.
    plat->sector->soundorg.z = plat->sector->floorheight;

    switch (plat->status)
    {
        case up:
            res = T_MovePlane(plat->sector, plat->speed,
                              plat->high, plat->crush, 0, 1);
            if (!(leveltime & 31))
            {
                S_StartSound(&plat->sector->soundorg, sfx_stnmov);
            }
            if (plat->type == raiseAndChange
                || plat->type == raiseToNearestAndChange)
            {
                if (!(leveltime & 7))
                {
                    S_StartSound(&plat->sector->soundorg,
                                 sfx_stnmov);
                }
            }
            if (res == crushed && (!plat->crush))
            {
                plat->count = plat->wait;
                plat->status = down;
                S_StartSound(&plat->sector->soundorg, sfx_pstart);
            }
            else if (res == pastdest)
            {
                plat->count = plat->wait;
                plat->status = waiting;
                S_StartSound(&plat->sector->soundorg, sfx_pstop);
                switch (plat->type)
                {
                    case downWaitUpStay:
                        P_RemoveActivePlat(plat);
                        break;
                    case raiseAndChange:
                        P_RemoveActivePlat(plat);
                        break;
                    default:
                        break;
                }
            }
            break;
        case down:
            res =
                T_MovePlane(plat->sector, plat->speed, plat->low, false, 0,
                            -1);
            if (res == pastdest)
            {
                plat->count = plat->wait;
                plat->status = waiting;
                S_StartSound(&plat->sector->soundorg, sfx_pstop);
            }
            else
            {
                if (!(leveltime & 31))
                {
                    S_StartSound(&plat->sector->soundorg,
                                 sfx_stnmov);
                }
            }
            break;
        case waiting:
            if (!--plat->count)
            {
                if (plat->sector->floorheight == plat->low)
                    plat->status = up;
                else
                    plat->status = down;
                S_StartSound(&plat->sector->soundorg, sfx_pstart);
            }
        case in_stasis:
            break;
    }
}

//==================================================================
//
//      Do Platforms
//      "amount" is only used for SOME platforms.
//
//==================================================================
int EV_DoPlat(line_t * line, plattype_e type, int amount)
{
    plat_t *plat;
    int secnum;
    int rtn;
    sector_t *sec;

    secnum = -1;
    rtn = 0;

    //
    //      Activate all <type> plats that are in_stasis
    //
    switch (type)
    {
        case perpetualRaise:
            P_ActivateInStasis(line->tag);
            break;
        default:
            break;
    }

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = &sectors[secnum];
        if (sec->specialdata)
            continue;

        //
        // Find lowest & highest floors around sector
        //
        rtn = 1;
        plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
        P_AddThinker(&plat->thinker);

        plat->type = type;
        plat->sector = sec;
        plat->sector->specialdata = plat;
        plat->thinker.function = T_PlatRaise;
        plat->crush = false;
        plat->tag = line->tag;
        // [JN] Z-axis sfx distance: sound invoked from the floor.
        sec->soundorg.z = sec->floorheight;
        switch (type)
        {
            case raiseToNearestAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
                plat->high = P_FindNextHighestFloor(sec, sec->floorheight);
                plat->wait = 0;
                plat->status = up;
                sec->special = 0;       // NO MORE DAMAGE, IF APPLICABLE
                S_StartSound(&sec->soundorg, sfx_stnmov);
                break;
            case raiseAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
                plat->high = sec->floorheight + amount * FRACUNIT;
                plat->wait = 0;
                plat->status = up;
                S_StartSound(&sec->soundorg, sfx_stnmov);
                break;
            case downWaitUpStay:
                plat->speed = PLATSPEED * 4;
                plat->low = P_FindLowestFloorSurrounding(sec);
                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;
                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = down;
                S_StartSound(&sec->soundorg, sfx_pstart);
                break;
            case perpetualRaise:
                plat->speed = PLATSPEED;
                plat->low = P_FindLowestFloorSurrounding(sec);
                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;
                plat->high = P_FindHighestFloorSurrounding(sec);
                if (plat->high < sec->floorheight)
                    plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = P_Random() & 1;
                S_StartSound(&sec->soundorg, sfx_pstart);
                break;
        }
        P_AddActivePlat(plat);
    }
    return rtn;
}

static void P_ActivateInStasis(int tag)
{
    int i;

    for (i = 0; i < MAXPLATS; i++)
        if (activeplats[i] &&
            (activeplats[i])->tag == tag &&
            (activeplats[i])->status == in_stasis)
        {
            (activeplats[i])->status = (activeplats[i])->oldstatus;
            (activeplats[i])->thinker.function = T_PlatRaise;
        }
}

void EV_StopPlat(const line_t *line)
{
    int j;

    for (j = 0; j < MAXPLATS; j++)
        if (activeplats[j] && ((activeplats[j])->status != in_stasis) &&
            ((activeplats[j])->tag == line->tag))
        {
            (activeplats[j])->oldstatus = (activeplats[j])->status;
            (activeplats[j])->status = in_stasis;
            (activeplats[j])->thinker.function = NULL;
        }
}

void P_AddActivePlat(plat_t * plat)
{
    int i;
    for (i = 0; i < MAXPLATS; i++)
        if (activeplats[i] == NULL)
        {
            activeplats[i] = plat;
            return;
        }
    I_Error("P_AddActivePlat: no more plats!");
}

static void P_RemoveActivePlat(const plat_t *plat)
{
    int i;
    for (i = 0; i < MAXPLATS; i++)
        if (plat == activeplats[i])
        {
            (activeplats[i])->sector->specialdata = NULL;
            P_RemoveThinker(&(activeplats[i])->thinker);
            activeplats[i] = NULL;
            return;
        }
    I_Error("P_RemoveActivePlat: can't find plat!");
}
