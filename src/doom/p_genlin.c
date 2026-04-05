
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
//  Boom generalized linedef handlers (Doom runtime port).
//

#include <limits.h>

#include "z_zone.h"
#include "doomstat.h"
#include "deh_main.h"
#include "d_englsh.h"
#include "ct_chat.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "r_local.h"

typedef enum
{
    gen_none = 0,
    gen_floor,
    gen_ceiling,
    gen_door,
    gen_locked,
    gen_lift,
    gen_stairs,
    gen_crusher
} genlineclass_t;

typedef enum
{
    SpeedSlow = 0,
    SpeedNormal,
    SpeedFast,
    SpeedTurbo
} motionspeed_e;

typedef enum
{
    FtoHnF = 0,
    FtoLnF,
    FtoNnF,
    FtoLnC,
    FtoC,
    FbyST,
    Fby24,
    Fby32
} floortarget_e;

typedef enum
{
    FNoChg = 0,
    FChgZero,
    FChgTxt,
    FChgTyp
} floorchange_e;

typedef enum
{
    FTriggerModel = 0,
    FNumericModel
} floormodel_e;

typedef enum
{
    CtoHnC = 0,
    CtoLnC,
    CtoNnC,
    CtoHnF,
    CtoF,
    CbyST,
    Cby24,
    Cby32
} ceilingtarget_e;

typedef enum
{
    CNoChg = 0,
    CChgZero,
    CChgTxt,
    CChgTyp
} ceilingchange_e;

typedef enum
{
    CTriggerModel = 0,
    CNumericModel
} ceilingmodel_e;

typedef enum
{
    F2LnF = 0,
    F2NnF,
    F2LnC,
    LnF2HnF
} lifttarget_e;

typedef enum
{
    OdCDoor = 0,
    ODoor,
    CdODoor,
    CDoor
} doorkind_e;

typedef enum
{
    AnyKey = 0,
    RCard,
    BCard,
    YCard,
    RSkull,
    BSkull,
    YSkull,
    AllKeys
} keykind_e;

// -----------------------------------------------------------------------------
// [PN] Generalized linedefs are active only in Boom/MBF complevels.
// -----------------------------------------------------------------------------
static boolean P_GenEnabled(void)
{
    return gamecomplevel >= COMPLEVEL_BOOM;
}

// -----------------------------------------------------------------------------
// [PN] Classify a linedef special into a Boom generalized action class.
// -----------------------------------------------------------------------------
static genlineclass_t P_GenClass(short special)
{
    unsigned int s = (unsigned short) special;

    if (s >= GenFloorBase)
    {
        return gen_floor;
    }

    if (s >= GenCeilingBase)
    {
        return gen_ceiling;
    }

    if (s >= GenDoorBase)
    {
        return gen_door;
    }

    if (s >= GenLockedBase)
    {
        return gen_locked;
    }

    if (s >= GenLiftBase)
    {
        return gen_lift;
    }

    if (s >= GenStairsBase)
    {
        return gen_stairs;
    }

    if (s >= GenCrusherBase)
    {
        return gen_crusher;
    }

    return gen_none;
}

// -----------------------------------------------------------------------------
// [PN] Floor, ceiling and texture model helpers used by generalized movers.
// -----------------------------------------------------------------------------
static fixed_t P_GenFindShortestTextureAround(int secnum, boolean upper)
{
    const sector_t *sec = &sectors[secnum];
    int minsize = INT_MAX;
    int i;

    for (i = 0; i < sec->linecount; ++i)
    {
        if (twoSided(secnum, i))
        {
            const side_t *side = getSide(secnum, i, 0);
            short texture = upper ? side->toptexture : side->bottomtexture;

            if (texture >= 0 && textureheight[texture] < minsize)
            {
                minsize = textureheight[texture];
            }

            side = getSide(secnum, i, 1);
            texture = upper ? side->toptexture : side->bottomtexture;

            if (texture >= 0 && textureheight[texture] < minsize)
            {
                minsize = textureheight[texture];
            }
        }
    }

    return minsize == INT_MAX ? 0 : minsize;
}

// -----------------------------------------------------------------------------
// [PN] Find a neighboring sector with a matching floor height.
// -----------------------------------------------------------------------------
static sector_t *P_GenFindModelFloorSector(fixed_t floordestheight, int secnum)
{
    sector_t *base = &sectors[secnum];
    int i;

    for (i = 0; i < base->linecount; ++i)
    {
        if (twoSided(secnum, i))
        {
            sector_t *sec = getSector(secnum, i,
                                      getSide(secnum, i, 0)->sector - sectors == secnum);

            if (sec->floorheight == floordestheight)
            {
                return sec;
            }
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// [PN] Find a neighboring sector with a matching ceiling height.
// -----------------------------------------------------------------------------
static sector_t *P_GenFindModelCeilingSector(fixed_t ceildestheight, int secnum)
{
    sector_t *base = &sectors[secnum];
    int i;

    for (i = 0; i < base->linecount; ++i)
    {
        if (twoSided(secnum, i))
        {
            sector_t *sec = getSector(secnum, i,
                                      getSide(secnum, i, 0)->sector - sectors == secnum);

            if (sec->ceilingheight == ceildestheight)
            {
                return sec;
            }
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// [PN] Spawn helper for floor movers.
// -----------------------------------------------------------------------------
static floormove_t *P_GenSpawnFloor(sector_t *sec)
{
    floormove_t *floor;

    if (sec->specialdata)
    {
        return NULL;
    }

    floor = Z_Malloc(sizeof(*floor), PU_LEVSPEC, 0);
    P_AddThinker(&floor->thinker);

    sec->specialdata = floor;

    floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
    floor->sector = sec;
    floor->type = lowerFloor;
    floor->crush = false;
    floor->direction = 1;
    floor->newspecial = sec->special;
    floor->texture = sec->floorpic;
    floor->floordestheight = sec->floorheight;
    floor->speed = FLOORSPEED;

    return floor;
}

// -----------------------------------------------------------------------------
// [PN] Spawn helper for ceiling movers.
// -----------------------------------------------------------------------------
static ceiling_t *P_GenSpawnCeiling(sector_t *sec)
{
    ceiling_t *ceiling;

    if (sec->specialdata)
    {
        return NULL;
    }

    ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
    P_AddThinker(&ceiling->thinker);

    sec->specialdata = ceiling;

    ceiling->thinker.function.acp1 = (actionf_p1) T_MoveCeiling;
    ceiling->sector = sec;
    ceiling->crush = false;
    ceiling->direction = -1;
    ceiling->bottomheight = sec->ceilingheight;
    ceiling->topheight = sec->ceilingheight;
    ceiling->speed = CEILSPEED;
    ceiling->tag = sec->tag;
    ceiling->olddirection = 0;
    ceiling->type = lowerToFloor;
    ceiling->newspecial = sec->special;
    ceiling->texture = sec->ceilingpic;

    P_AddActiveCeiling(ceiling);
    return ceiling;
}

// -----------------------------------------------------------------------------
// [PN] Spawn helper for doors.
// -----------------------------------------------------------------------------
static vldoor_t *P_GenSpawnDoor(sector_t *sec)
{
    vldoor_t *door;

    if (sec->specialdata)
    {
        return NULL;
    }

    door = Z_Malloc(sizeof(*door), PU_LEVSPEC, 0);
    P_AddThinker(&door->thinker);

    sec->specialdata = door;

    door->thinker.function.acp1 = (actionf_p1) T_VerticalDoor;
    door->sector = sec;
    door->topheight = sec->ceilingheight;
    door->speed = VDOORSPEED;
    door->direction = 1;
    door->topwait = VDOORWAIT;
    door->topcountdown = 0;
    door->type = vld_normal;
    door->line = NULL;
    door->lighttag = 0;

    return door;
}

// -----------------------------------------------------------------------------
// [PN] Spawn helper for platforms/lifts.
// -----------------------------------------------------------------------------
static plat_t *P_GenSpawnPlat(sector_t *sec, int tag)
{
    plat_t *plat;

    if (sec->specialdata)
    {
        return NULL;
    }

    plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
    P_AddThinker(&plat->thinker);

    sec->specialdata = plat;

    plat->thinker.function.acp1 = (actionf_p1) T_PlatRaise;
    plat->sector = sec;
    plat->speed = PLATSPEED;
    plat->low = sec->floorheight;
    plat->high = sec->floorheight;
    plat->wait = TICRATE * PLATWAIT;
    plat->count = 0;
    plat->status = down;
    plat->oldstatus = down;
    plat->crush = false;
    plat->tag = tag;
    plat->type = downWaitUpStay;

    P_AddActivePlat(plat);
    return plat;
}
// -----------------------------------------------------------------------------
// [PN] Decode generalized motion speed into floor speed scale.
// -----------------------------------------------------------------------------
static fixed_t P_GenFloorSpeed(int speed)
{
    switch ((motionspeed_e) speed)
    {
        case SpeedSlow:   return FLOORSPEED;
        case SpeedNormal: return FLOORSPEED * 2;
        case SpeedFast:   return FLOORSPEED * 4;
        case SpeedTurbo:  return FLOORSPEED * 8;
        default:          return FLOORSPEED;
    }
}

// -----------------------------------------------------------------------------
// [PN] Decode generalized motion speed into ceiling speed scale.
// -----------------------------------------------------------------------------
static fixed_t P_GenCeilingSpeed(int speed)
{
    switch ((motionspeed_e) speed)
    {
        case SpeedSlow:   return CEILSPEED;
        case SpeedNormal: return CEILSPEED * 2;
        case SpeedFast:   return CEILSPEED * 4;
        case SpeedTurbo:  return CEILSPEED * 8;
        default:          return CEILSPEED;
    }
}

// -----------------------------------------------------------------------------
// [PN] Decode generalized door speed.
// -----------------------------------------------------------------------------
static fixed_t P_GenDoorSpeed(int speed)
{
    switch ((motionspeed_e) speed)
    {
        case SpeedSlow:   return VDOORSPEED;
        case SpeedNormal: return VDOORSPEED * 2;
        case SpeedFast:   return VDOORSPEED * 4;
        case SpeedTurbo:  return VDOORSPEED * 8;
        default:          return VDOORSPEED;
    }
}

// -----------------------------------------------------------------------------
// [PN] Decode generalized lift speed.
// -----------------------------------------------------------------------------
static fixed_t P_GenLiftSpeed(int speed)
{
    switch ((motionspeed_e) speed)
    {
        case SpeedSlow:   return PLATSPEED * 2;
        case SpeedNormal: return PLATSPEED * 4;
        case SpeedFast:   return PLATSPEED * 8;
        case SpeedTurbo:  return PLATSPEED * 16;
        default:          return PLATSPEED * 2;
    }
}

// -----------------------------------------------------------------------------
// [PN] Decode generalized stair speed.
// -----------------------------------------------------------------------------
static fixed_t P_GenStairSpeed(int speed)
{
    switch ((motionspeed_e) speed)
    {
        case SpeedSlow:   return FLOORSPEED / 4;
        case SpeedNormal: return FLOORSPEED / 2;
        case SpeedFast:   return FLOORSPEED * 2;
        case SpeedTurbo:  return FLOORSPEED * 4;
        default:          return FLOORSPEED / 4;
    }
}

// -----------------------------------------------------------------------------
// [PN] Decode generalized lift delay (in tics).
// -----------------------------------------------------------------------------
static int P_GenLiftDelay(int delay)
{
    switch (delay)
    {
        case 0:  return TICRATE;
        case 1:  return TICRATE * PLATWAIT;
        case 2:  return TICRATE * 5;
        case 3:  return TICRATE * 10;
        default: return TICRATE * PLATWAIT;
    }
}

// -----------------------------------------------------------------------------
// [PN] Decode generalized door delay (in tics).
// -----------------------------------------------------------------------------
static int P_GenDoorDelay(int delay)
{
    switch (delay)
    {
        case 0:  return TICRATE;
        case 1:  return VDOORWAIT;
        case 2:  return VDOORWAIT * 2;
        case 3:  return VDOORWAIT * 7;
        default: return VDOORWAIT;
    }
}

// -----------------------------------------------------------------------------
// [PN] MBF-style gradual door lighting for manual generalized doors.
// Doom map format has no args[] field, so line tag is used as light tag.
// -----------------------------------------------------------------------------
static int P_GenDoorLightTag(const line_t *line)
{
    if ((line->special & 6) == 6
     && (unsigned short) line->special > GenLockedBase
     && line->tag)
    {
        return line->tag;
    }

    return 0;
}

// -----------------------------------------------------------------------------
// [PN] Apply generalized floor behavior to one sector.
// -----------------------------------------------------------------------------
static int P_GenDoFloorSector(line_t *line, sector_t *sec, int secnum)
{
    unsigned int value = (unsigned short) line->special - GenFloorBase;
    int crush = (value & FloorCrush) >> FloorCrushShift;
    int chgtype = (value & FloorChange) >> FloorChangeShift;
    int target = (value & FloorTarget) >> FloorTargetShift;
    int dir = (value & FloorDirection) >> FloorDirectionShift;
    int chgmodel = (value & FloorModel) >> FloorModelShift;
    int speed = (value & FloorSpeed) >> FloorSpeedShift;
    floormove_t *floor = P_GenSpawnFloor(sec);
    sector_t *model = NULL;

    if (!floor)
    {
        return 0;
    }

    floor->crush = crush != 0;
    floor->direction = dir ? 1 : -1;
    floor->speed = P_GenFloorSpeed(speed);

    switch ((floortarget_e) target)
    {
        case FtoHnF:
            floor->floordestheight = P_FindHighestFloorSurrounding(sec);
            break;
        case FtoLnF:
            floor->floordestheight = P_FindLowestFloorSurrounding(sec);
            break;
        case FtoNnF:
            floor->floordestheight = floor->direction > 0
                                   ? P_FindNextHighestFloor(sec, sec->floorheight)
                                   : P_FindNextLowestFloor(sec, sec->floorheight);
            break;
        case FtoLnC:
            floor->floordestheight = P_FindLowestCeilingSurrounding(sec);
            break;
        case FtoC:
            floor->floordestheight = sec->ceilingheight;
            break;
        case FbyST:
            floor->floordestheight = sec->floorheight
                                   + floor->direction * P_GenFindShortestTextureAround(secnum, false);
            break;
        case Fby24:
            floor->floordestheight = sec->floorheight + floor->direction * 24 * FRACUNIT;
            break;
        case Fby32:
            floor->floordestheight = sec->floorheight + floor->direction * 32 * FRACUNIT;
            break;
    }

    if (chgtype != FNoChg)
    {
        if (chgmodel == FTriggerModel)
        {
            model = line->frontsector;
        }
        else
        {
            model = (target == FtoLnC || target == FtoC)
                  ? P_GenFindModelCeilingSector(floor->floordestheight, secnum)
                  : P_GenFindModelFloorSector(floor->floordestheight, secnum);
        }

        if (model)
        {
            floor->texture = model->floorpic;

            if ((floorchange_e) chgtype == FChgZero)
            {
                floor->newspecial = 0;
            }
            else if ((floorchange_e) chgtype == FChgTyp)
            {
                floor->newspecial = model->special;
            }

            floor->type = floor->direction > 0 ? donutRaise : lowerAndChange;
        }
    }

    return 1;
}

// -----------------------------------------------------------------------------
// [PN] Generalized floor handler.
// -----------------------------------------------------------------------------
static int EV_DoGenFloor(line_t *line)
{
    int secnum;
    int rtn = 0;
    int trigger = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    boolean manual = (trigger == PushOnce || trigger == PushMany);

    if (manual)
    {
        if (!line->backsector)
        {
            return 0;
        }

        return P_GenDoFloorSector(line, line->backsector, (int) (line->backsector - sectors));
    }

    secnum = -1;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        rtn |= P_GenDoFloorSector(line, &sectors[secnum], secnum);
    }

    return rtn;
}

// -----------------------------------------------------------------------------
// [PN] Apply generalized ceiling behavior to one sector.
// -----------------------------------------------------------------------------
static int P_GenDoCeilingSector(line_t *line, sector_t *sec, int secnum)
{
    unsigned int value = (unsigned short) line->special - GenCeilingBase;
    int crush = (value & CeilingCrush) >> CeilingCrushShift;
    int chgtype = (value & CeilingChange) >> CeilingChangeShift;
    int target = (value & CeilingTarget) >> CeilingTargetShift;
    int dir = (value & CeilingDirection) >> CeilingDirectionShift;
    int chgmodel = (value & CeilingModel) >> CeilingModelShift;
    int speed = (value & CeilingSpeed) >> CeilingSpeedShift;
    ceiling_t *ceiling = P_GenSpawnCeiling(sec);
    fixed_t dest = sec->ceilingheight;
    sector_t *model = NULL;

    if (!ceiling)
    {
        return 0;
    }

    switch ((ceilingtarget_e) target)
    {
        case CtoHnC:
            dest = P_FindHighestCeilingSurrounding(sec);
            break;
        case CtoLnC:
            dest = P_FindLowestCeilingSurrounding(sec);
            break;
        case CtoNnC:
            dest = dir
                 ? P_FindNextHighestCeiling(sec, sec->ceilingheight)
                 : P_FindNextLowestCeiling(sec, sec->ceilingheight);
            break;
        case CtoHnF:
            dest = P_FindHighestFloorSurrounding(sec);
            break;
        case CtoF:
            dest = sec->floorheight;
            break;
        case CbyST:
            dest = sec->ceilingheight
                 + (dir ? 1 : -1) * P_GenFindShortestTextureAround(secnum, true);
            break;
        case Cby24:
            dest = sec->ceilingheight + (dir ? 1 : -1) * 24 * FRACUNIT;
            break;
        case Cby32:
            dest = sec->ceilingheight + (dir ? 1 : -1) * 32 * FRACUNIT;
            break;
    }

    ceiling->crush = crush != 0;
    ceiling->speed = P_GenCeilingSpeed(speed);

    if (dir)
    {
        ceiling->direction = 1;
        ceiling->topheight = dest;
        ceiling->bottomheight = sec->ceilingheight;
        ceiling->type = raiseToHighest;
    }
    else
    {
        ceiling->direction = -1;
        ceiling->bottomheight = dest;
        ceiling->topheight = sec->ceilingheight;
        ceiling->type = crush ? lowerAndCrush : lowerToFloor;
    }

    if (chgtype != CNoChg)
    {
        if (chgmodel == CTriggerModel)
        {
            model = line->frontsector;
        }
        else
        {
            model = (target == CtoHnF || target == CtoF)
                  ? P_GenFindModelFloorSector(dest, secnum)
                  : P_GenFindModelCeilingSector(dest, secnum);
        }

        if (model)
        {
            ceiling->texture = model->ceilingpic;

            if ((ceilingchange_e) chgtype == CChgZero)
            {
                ceiling->newspecial = 0;
            }
            else if ((ceilingchange_e) chgtype == CChgTyp)
            {
                ceiling->newspecial = model->special;
            }
        }
    }

    return 1;
}

// -----------------------------------------------------------------------------
// [PN] Generalized ceiling handler.
// -----------------------------------------------------------------------------
static int EV_DoGenCeiling(line_t *line)
{
    int secnum;
    int rtn = 0;
    int trigger = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    boolean manual = (trigger == PushOnce || trigger == PushMany);

    if (manual)
    {
        if (!line->backsector)
        {
            return 0;
        }

        return P_GenDoCeilingSector(line, line->backsector, (int) (line->backsector - sectors));
    }

    secnum = -1;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        rtn |= P_GenDoCeilingSector(line, &sectors[secnum], secnum);
    }

    return rtn;
}
// -----------------------------------------------------------------------------
// [PN] Initialize and start a generalized door thinker for one sector.
// -----------------------------------------------------------------------------
static int P_GenDoDoorSector(line_t *line, sector_t *sec)
{
    unsigned int value = (unsigned short) line->special - GenDoorBase;
    int delay = (value & DoorDelay) >> DoorDelayShift;
    int kind = (value & DoorKind) >> DoorKindShift;
    int speed = (value & DoorSpeed) >> DoorSpeedShift;
    vldoor_t *door = P_GenSpawnDoor(sec);

    if (!door)
    {
        return 0;
    }

    door->line = line;
    door->lighttag = P_GenDoorLightTag(line);
    door->speed = P_GenDoorSpeed(speed);
    door->topwait = P_GenDoorDelay(delay);

    switch ((doorkind_e) kind)
    {
        case OdCDoor:
            door->type = door->speed >= VDOORSPEED * 4 ? vld_blazeRaise : vld_normal;
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;
            if (door->topheight != sec->ceilingheight)
            {
                S_StartSound(&door->sector->soundorg,
                             door->speed >= VDOORSPEED * 4 ? sfx_bdopn : sfx_doropn);
            }
            break;

        case ODoor:
            door->type = door->speed >= VDOORSPEED * 4 ? vld_blazeOpen : vld_open;
            door->direction = 1;
            door->topheight = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;
            if (door->topheight != sec->ceilingheight)
            {
                S_StartSound(&door->sector->soundorg,
                             door->speed >= VDOORSPEED * 4 ? sfx_bdopn : sfx_doropn);
            }
            break;

        case CdODoor:
            door->type = vld_close30ThenOpen;
            door->direction = -1;
            door->topheight = sec->ceilingheight;
            S_StartSound(&door->sector->soundorg,
                         door->speed >= VDOORSPEED * 4 ? sfx_bdcls : sfx_dorcls);
            break;

        case CDoor:
            door->type = door->speed >= VDOORSPEED * 4 ? vld_blazeClose : vld_close;
            door->direction = -1;
            door->topheight = P_FindLowestCeilingSurrounding(sec) - 4 * FRACUNIT;
            S_StartSound(&door->sector->soundorg,
                         door->speed >= VDOORSPEED * 4 ? sfx_bdcls : sfx_dorcls);
            break;
    }

    return 1;
}

// -----------------------------------------------------------------------------
// [PN] Generalized door handler.
// -----------------------------------------------------------------------------
static int EV_DoGenDoor(line_t *line)
{
    int secnum;
    int rtn = 0;
    int trigger = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    boolean manual = (trigger == PushOnce || trigger == PushMany);

    if (manual)
    {
        if (!line->backsector)
        {
            return 0;
        }

        return P_GenDoDoorSector(line, line->backsector);
    }

    secnum = -1;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        rtn |= P_GenDoDoorSector(line, &sectors[secnum]);
    }

    return rtn;
}

// -----------------------------------------------------------------------------
// [PN] Generalized locked door handler.
// -----------------------------------------------------------------------------
static int EV_DoGenLockedDoor(line_t *line)
{
    unsigned int value = (unsigned short) line->special - GenLockedBase;
    int kind = (value & LockedKind) >> LockedKindShift;
    int speed = (value & LockedSpeed) >> LockedSpeedShift;
    int trigger = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    boolean manual = (trigger == PushOnce || trigger == PushMany);
    int secnum;
    int rtn = 0;

    if (manual)
    {
        vldoor_t *door;

        if (!line->backsector)
        {
            return 0;
        }

        door = P_GenSpawnDoor(line->backsector);

        if (!door)
        {
            return 0;
        }

        door->line = line;
        door->lighttag = P_GenDoorLightTag(line);
        door->speed = P_GenDoorSpeed(speed);
        door->topwait = VDOORWAIT;
        door->direction = 1;
        door->topheight = P_FindLowestCeilingSurrounding(line->backsector) - 4 * FRACUNIT;
        door->type = kind
                   ? (door->speed >= VDOORSPEED * 4 ? vld_blazeOpen : vld_open)
                   : (door->speed >= VDOORSPEED * 4 ? vld_blazeRaise : vld_normal);

        if (door->topheight != line->backsector->ceilingheight)
        {
            S_StartSound(&door->sector->soundorg,
                         door->speed >= VDOORSPEED * 4 ? sfx_bdopn : sfx_doropn);
        }

        return 1;
    }

    secnum = -1;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        vldoor_t *door = P_GenSpawnDoor(&sectors[secnum]);

        if (!door)
        {
            continue;
        }

        rtn = 1;
        door->line = line;
        door->lighttag = P_GenDoorLightTag(line);
        door->speed = P_GenDoorSpeed(speed);
        door->topwait = VDOORWAIT;
        door->direction = 1;
        door->topheight = P_FindLowestCeilingSurrounding(&sectors[secnum]) - 4 * FRACUNIT;
        door->type = kind
                   ? (door->speed >= VDOORSPEED * 4 ? vld_blazeOpen : vld_open)
                   : (door->speed >= VDOORSPEED * 4 ? vld_blazeRaise : vld_normal);

        if (door->topheight != sectors[secnum].ceilingheight)
        {
            S_StartSound(&door->sector->soundorg,
                         door->speed >= VDOORSPEED * 4 ? sfx_bdopn : sfx_doropn);
        }
    }

    return rtn;
}

// -----------------------------------------------------------------------------
// [PN] Apply generalized lift behavior to one sector.
// -----------------------------------------------------------------------------
static int P_GenDoLiftSector(line_t *line, sector_t *sec)
{
    unsigned int value = (unsigned short) line->special - GenLiftBase;
    int target = (value & LiftTarget) >> LiftTargetShift;
    int delay = (value & LiftDelay) >> LiftDelayShift;
    int speed = (value & LiftSpeed) >> LiftSpeedShift;
    plat_t *plat = P_GenSpawnPlat(sec, line->tag);

    if (!plat)
    {
        return 0;
    }

    plat->speed = P_GenLiftSpeed(speed);
    plat->wait = P_GenLiftDelay(delay);
    plat->high = sec->floorheight;

    if ((lifttarget_e) target == LnF2HnF)
    {
        plat->type = perpetualRaise;
        plat->low = P_FindLowestFloorSurrounding(sec);
        if (plat->low > sec->floorheight)
        {
            plat->low = sec->floorheight;
        }

        plat->high = P_FindHighestFloorSurrounding(sec);
        if (plat->high < sec->floorheight)
        {
            plat->high = sec->floorheight;
        }

        plat->status = P_Random() & 1;
        S_StartSound(&sec->soundorg, sfx_pstart);
        return 1;
    }

    plat->type = downWaitUpStay;
    plat->status = down;

    switch ((lifttarget_e) target)
    {
        case F2LnF:
            plat->low = P_FindLowestFloorSurrounding(sec);
            if (plat->low > sec->floorheight)
            {
                plat->low = sec->floorheight;
            }
            break;
        case F2NnF:
            plat->low = P_FindNextLowestFloor(sec, sec->floorheight);
            break;
        case F2LnC:
            plat->low = P_FindLowestCeilingSurrounding(sec);
            if (plat->low > sec->floorheight)
            {
                plat->low = sec->floorheight;
            }
            break;
        default:
            plat->low = sec->floorheight;
            break;
    }

    S_StartSound(&sec->soundorg, sfx_pstart);
    return 1;
}

// -----------------------------------------------------------------------------
// [PN] Generalized lift handler.
// -----------------------------------------------------------------------------
static int EV_DoGenLift(line_t *line)
{
    int secnum;
    int rtn = 0;
    int trigger = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    boolean manual = (trigger == PushOnce || trigger == PushMany);

    if (manual)
    {
        if (!line->backsector)
        {
            return 0;
        }

        return P_GenDoLiftSector(line, line->backsector);
    }

    secnum = -1;

    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        rtn |= P_GenDoLiftSector(line, &sectors[secnum]);
    }

    return rtn;
}

// -----------------------------------------------------------------------------
// [PN] Generalized stairs handler.
// -----------------------------------------------------------------------------
static int EV_DoGenStairs(line_t *line)
{
    unsigned int value = (unsigned short) line->special - GenStairsBase;
    int ignoretexture = (value & StairIgnore) >> StairIgnoreShift;
    int direction = (value & StairDirection) >> StairDirectionShift;
    int step = (value & StairStep) >> StairStepShift;
    int speed = (value & StairSpeed) >> StairSpeedShift;
    int trigger = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    boolean manual = (trigger == PushOnce || trigger == PushMany);
    fixed_t stairsize;
    fixed_t movespeed = P_GenStairSpeed(speed);
    int secnum;
    int rtn = 0;

    switch (step)
    {
        case 0: stairsize = 4 * FRACUNIT; break;
        case 1: stairsize = 8 * FRACUNIT; break;
        case 2: stairsize = 16 * FRACUNIT; break;
        default:
        case 3: stairsize = 24 * FRACUNIT; break;
    }

    secnum = manual
           ? (line->backsector ? (int) (line->backsector - sectors) : -1)
           : -1;

    if (manual && secnum < 0)
    {
        return 0;
    }

    while (manual || (secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sector_t *sec = &sectors[secnum];
        int texture = sec->floorpic;
        fixed_t height = sec->floorheight + (direction ? stairsize : -stairsize);
        int buildok;
        floormove_t *floor;

        if (sec->specialdata)
        {
            if (manual)
            {
                return rtn;
            }

            continue;
        }

        floor = P_GenSpawnFloor(sec);
        if (!floor)
        {
            if (manual)
            {
                return rtn;
            }

            continue;
        }

        rtn = 1;
        floor->direction = direction ? 1 : -1;
        floor->speed = movespeed;
        floor->floordestheight = height;
        floor->type = lowerFloor;
        floor->crush = false;

        do
        {
            int i;
            buildok = 0;

            for (i = 0; i < sec->linecount; ++i)
            {
                sector_t *tsec;
                int newsecnum;
                floormove_t *next;

                if (!(sec->lines[i]->flags & ML_TWOSIDED))
                {
                    continue;
                }

                tsec = sec->lines[i]->frontsector;
                newsecnum = (int) (tsec - sectors);

                if (newsecnum != secnum)
                {
                    continue;
                }

                tsec = sec->lines[i]->backsector;
                newsecnum = (int) (tsec - sectors);

                if (!ignoretexture && tsec->floorpic != texture)
                {
                    continue;
                }

                if (tsec->specialdata)
                {
                    continue;
                }

                height += direction ? stairsize : -stairsize;

                next = P_GenSpawnFloor(tsec);
                if (!next)
                {
                    continue;
                }

                next->direction = direction ? 1 : -1;
                next->speed = movespeed;
                next->floordestheight = height;
                next->type = lowerFloor;
                next->crush = false;

                sec = tsec;
                secnum = newsecnum;
                buildok = 1;
                break;
            }
        }
        while (buildok);

        if (manual)
        {
            return rtn;
        }
    }

    if (rtn)
    {
        line->special ^= StairDirection;
    }

    return rtn;
}

// -----------------------------------------------------------------------------
// [PN] Generalized crusher handler.
// -----------------------------------------------------------------------------
static int EV_DoGenCrusher(line_t *line)
{
    unsigned int value = (unsigned short) line->special - GenCrusherBase;
    int silent = (value & CrusherSilent) >> CrusherSilentShift;
    int speed = (value & CrusherSpeed) >> CrusherSpeedShift;
    int trigger = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    boolean manual = (trigger == PushOnce || trigger == PushMany);
    int secnum;
    int rtn = 0;

    P_ActivateInStasisCeiling(line);

    secnum = manual
           ? (line->backsector ? (int) (line->backsector - sectors) : -1)
           : -1;

    if (manual && secnum < 0)
    {
        return 0;
    }

    while (manual || (secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
    {
        sector_t *sec = &sectors[secnum];
        ceiling_t *ceiling = P_GenSpawnCeiling(sec);

        if (!ceiling)
        {
            if (manual)
            {
                return rtn;
            }

            continue;
        }

        rtn = 1;
        ceiling->crush = true;
        ceiling->direction = -1;
        ceiling->topheight = sec->ceilingheight;
        ceiling->bottomheight = sec->floorheight + 8 * FRACUNIT;
        ceiling->speed = P_GenCeilingSpeed(speed);
        ceiling->type = silent ? silentCrushAndRaise : crushAndRaise;

        if (manual)
        {
            return rtn;
        }
    }

    return rtn;
}
// -----------------------------------------------------------------------------
// [PN] Shared key test for generalized locked doors.
// -----------------------------------------------------------------------------
static boolean P_CanUnlockGenDoor(line_t *line, player_t *player)
{
    int skulliscard = ((unsigned short) line->special & LockedNKeys) >> LockedNKeysShift;
    int key = ((unsigned short) line->special & LockedKey) >> LockedKeyShift;
    const char *msg = NULL;
    int blink = -1;

    switch ((keykind_e) key)
    {
        case AnyKey:
            if (!player->cards[it_redcard] && !player->cards[it_redskull]
             && !player->cards[it_bluecard] && !player->cards[it_blueskull]
             && !player->cards[it_yellowcard] && !player->cards[it_yellowskull])
            {
                msg = "You need a key to open this door";
            }
            break;

        case RCard:
            if (!player->cards[it_redcard] && (!skulliscard || !player->cards[it_redskull]))
            {
                msg = DEH_String(PD_REDK);
                blink = it_redcard;
            }
            break;

        case BCard:
            if (!player->cards[it_bluecard] && (!skulliscard || !player->cards[it_blueskull]))
            {
                msg = DEH_String(PD_BLUEK);
                blink = it_bluecard;
            }
            break;

        case YCard:
            if (!player->cards[it_yellowcard] && (!skulliscard || !player->cards[it_yellowskull]))
            {
                msg = DEH_String(PD_YELLOWK);
                blink = it_yellowcard;
            }
            break;

        case RSkull:
            if (!player->cards[it_redskull] && (!skulliscard || !player->cards[it_redcard]))
            {
                msg = DEH_String(PD_REDK);
                blink = it_redcard;
            }
            break;

        case BSkull:
            if (!player->cards[it_blueskull] && (!skulliscard || !player->cards[it_bluecard]))
            {
                msg = DEH_String(PD_BLUEK);
                blink = it_bluecard;
            }
            break;

        case YSkull:
            if (!player->cards[it_yellowskull] && (!skulliscard || !player->cards[it_yellowcard]))
            {
                msg = DEH_String(PD_YELLOWK);
                blink = it_yellowcard;
            }
            break;

        case AllKeys:
            if (!skulliscard
             && (!player->cards[it_redcard] || !player->cards[it_redskull]
              || !player->cards[it_bluecard] || !player->cards[it_blueskull]
              || !player->cards[it_yellowcard] || !player->cards[it_yellowskull]))
            {
                msg = "You need all keys to open this door";
            }
            else if (skulliscard
                  && (!(player->cards[it_redcard] || player->cards[it_redskull])
                   || !(player->cards[it_bluecard] || player->cards[it_blueskull])
                   || !(player->cards[it_yellowcard] || player->cards[it_yellowskull])))
            {
                msg = "You need all key colors to open this door";
            }
            break;
    }

    if (!msg)
    {
        return true;
    }

    CT_SetMessage(player, msg, false, NULL);

    if (blink >= 0)
    {
        player->tryopen[blink] = KEYBLINKTICS;
    }

    if (PTR_NoWayAudible(line))
    {
        S_StartSound(NULL, sfx_oof);
    }

    return false;
}

// -----------------------------------------------------------------------------
// [PN] Resolve generalized action handler by linedef class.
// -----------------------------------------------------------------------------
static int (*P_GenAction(genlineclass_t cls))(line_t *line)
{
    switch (cls)
    {
        case gen_floor:   return EV_DoGenFloor;
        case gen_ceiling: return EV_DoGenCeiling;
        case gen_door:    return EV_DoGenDoor;
        case gen_locked:  return EV_DoGenLockedDoor;
        case gen_lift:    return EV_DoGenLift;
        case gen_stairs:  return EV_DoGenStairs;
        case gen_crusher: return EV_DoGenCrusher;
        default:          return NULL;
    }
}

// -----------------------------------------------------------------------------
// [PN] Monster activation checks for generalized classes.
// -----------------------------------------------------------------------------
static boolean P_GenCanMonsterActivate(genlineclass_t cls, line_t *line)
{
    unsigned int special = (unsigned short) line->special;

    switch (cls)
    {
        case gen_floor:
            return !(special & FloorChange) && (special & FloorModel);
        case gen_ceiling:
            return !(special & CeilingChange) && (special & CeilingModel);
        case gen_door:
            return (special & DoorMonster) && !(line->flags & ML_SECRET);
        case gen_locked:
            return false;
        case gen_lift:
            return (special & LiftMonster) != 0;
        case gen_stairs:
            return (special & StairMonster) != 0;
        case gen_crusher:
            return (special & CrusherMonster) != 0;
        default:
            return false;
    }
}

// -----------------------------------------------------------------------------
// [PN] Common pre-check for generalized linedefs.
// -----------------------------------------------------------------------------
static boolean P_GenAllowed(genlineclass_t cls, mobj_t *thing, line_t *line)
{
    if (thing->player)
    {
        return true;
    }

    return P_GenCanMonsterActivate(cls, line);
}

// -----------------------------------------------------------------------------
// [PN] Walk-over generalized dispatcher.
// -----------------------------------------------------------------------------
boolean P_CrossGeneralizedLine(line_t *line, mobj_t *thing)
{
    genlineclass_t cls;
    int trig;
    int (*linefunc)(line_t *line);

    if (!P_GenEnabled())
    {
        return false;
    }

    cls = P_GenClass(line->special);

    if (cls == gen_none)
    {
        return false;
    }

    if (!P_GenAllowed(cls, thing, line))
    {
        return true;
    }

    trig = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;

    if (trig != WalkOnce && trig != WalkMany)
    {
        return true;
    }

    if (!line->tag)
    {
        return true;
    }

    if (cls == gen_locked && !P_CanUnlockGenDoor(line, thing->player))
    {
        return true;
    }

    linefunc = P_GenAction(cls);

    if (!linefunc)
    {
        return true;
    }

    if (trig == WalkOnce)
    {
        if (linefunc(line))
        {
            line->special = 0;
        }
    }
    else
    {
        linefunc(line);
    }

    return true;
}

// -----------------------------------------------------------------------------
// [PN] Use/switch generalized dispatcher.
// -----------------------------------------------------------------------------
boolean P_UseGeneralizedLine(mobj_t *thing, line_t *line, int side)
{
    genlineclass_t cls;
    int trig;
    int (*linefunc)(line_t *line);
    boolean manual;

    if (!P_GenEnabled())
    {
        return false;
    }

    cls = P_GenClass(line->special);

    if (cls == gen_none)
    {
        return false;
    }

    if (side)
    {
        return true;
    }

    if (!P_GenAllowed(cls, thing, line))
    {
        return true;
    }

    trig = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;
    manual = (trig == PushOnce || trig == PushMany);

    if (trig != PushOnce && trig != PushMany
     && trig != SwitchOnce && trig != SwitchMany)
    {
        return true;
    }

    if (!manual && !line->tag)
    {
        return true;
    }

    if (cls == gen_locked)
    {
        if (!thing->player || !P_CanUnlockGenDoor(line, thing->player))
        {
            return true;
        }
    }

    linefunc = P_GenAction(cls);

    if (!linefunc)
    {
        return true;
    }

    switch (trig)
    {
        case PushOnce:
            if (!side && linefunc(line))
            {
                line->special = 0;
            }
            return true;

        case PushMany:
            if (!side)
            {
                linefunc(line);
            }
            return true;

        case SwitchOnce:
            if (linefunc(line))
            {
                P_ChangeSwitchTexture(line, 0);
            }
            return true;

        case SwitchMany:
            if (linefunc(line))
            {
                P_ChangeSwitchTexture(line, 1);
            }
            return true;

        default:
            return true;
    }
}

// -----------------------------------------------------------------------------
// [PN] Gun-trigger generalized dispatcher.
// -----------------------------------------------------------------------------
boolean P_ShootGeneralizedLine(const mobj_t *thing, line_t *line)
{
    genlineclass_t cls;
    int trig;
    int (*linefunc)(line_t *line);

    if (!P_GenEnabled())
    {
        return false;
    }

    cls = P_GenClass(line->special);

    if (cls == gen_none)
    {
        return false;
    }

    if (!thing->player && !P_GenCanMonsterActivate(cls, line))
    {
        return true;
    }

    trig = ((unsigned short) line->special & TriggerType) >> TriggerTypeShift;

    if (trig != GunOnce && trig != GunMany)
    {
        return true;
    }

    if (!line->tag)
    {
        return true;
    }

    if (cls == gen_locked)
    {
        if (!thing->player || !P_CanUnlockGenDoor(line, thing->player))
        {
            return true;
        }
    }

    linefunc = P_GenAction(cls);

    if (!linefunc)
    {
        return true;
    }

    if (trig == GunOnce)
    {
        if (linefunc(line))
        {
            P_ChangeSwitchTexture(line, 0);
        }
    }
    else
    {
        if (linefunc(line))
        {
            P_ChangeSwitchTexture(line, 1);
        }
    }

    return true;
}
