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
//	Handling interactions (i.e., collisions).
//


#include "dstrings.h"
#include "deh_main.h"
#include "deh_misc.h"
#include "doomstat.h"
#include "m_random.h"
#include "i_system.h"
#include "am_map.h"
#include "p_local.h"
#include "s_sound.h"
#include "ct_chat.h"

#include "id_vars.h"
#include "id_func.h"






// a weapon is found with two clip loads,
// a big item has five clip loads
int	maxammo[NUMAMMO] = {200, 50, 300, 50};
int	clipammo[NUMAMMO] = {10, 4, 20, 1};


//
// GET STUFF
//

//
// P_GiveAmmo
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//

boolean
P_GiveAmmo
( player_t*	player,
  ammotype_t	ammo,
  int		num,
  boolean	dropped ) // [NS] Dropped ammo/weapons give half as much.
{
    int		oldammo;
	
    if (ammo == am_noammo)
	return false;
		
    if (ammo >= NUMAMMO)
	I_Error ("P_GiveAmmo: bad type %i", ammo);
		
    if ( player->ammo[ammo] == player->maxammo[ammo]  )
	return false;
		
    if (num)
	num *= clipammo[ammo];
    else
	num = clipammo[ammo]/2;
    
    if (gameskill == sk_baby
	|| gameskill == sk_nightmare)
    {
	// give double ammo in trainer mode,
	// you'll need in nightmare
	num <<= 1;
    }
    
	// [NS] Halve if needed.
	if (dropped)
	{
		num >>= 1;
		// Don't round down to 0.
		if (!num)
			num = 1;
	}
		
    oldammo = player->ammo[ammo];
    player->ammo[ammo] += num;

    if (player->ammo[ammo] > player->maxammo[ammo])
	player->ammo[ammo] = player->maxammo[ammo];

    // If non zero ammo, 
    // don't change up weapons,
    // player was lower on purpose.
    if (oldammo)
	return true;	

    // We were down to zero,
    // so select a new weapon.
    // Preferences are not user selectable.
    switch (ammo)
    {
      case am_clip:
	if (player->readyweapon == wp_fist)
	{
	    if (player->weaponowned[wp_chaingun])
		player->pendingweapon = wp_chaingun;
	    else
		player->pendingweapon = wp_pistol;
	}
	break;
	
      case am_shell:
	if (player->readyweapon == wp_fist
	    || player->readyweapon == wp_pistol)
	{
	    if (player->weaponowned[wp_shotgun])
		player->pendingweapon = wp_shotgun;
	}
	break;
	
      case am_cell:
	if (player->readyweapon == wp_fist
	    || player->readyweapon == wp_pistol)
	{
	    if (player->weaponowned[wp_plasma])
		player->pendingweapon = wp_plasma;
	}
	break;
	
      case am_misl:
	if (player->readyweapon == wp_fist)
	{
	    if (player->weaponowned[wp_missile])
		player->pendingweapon = wp_missile;
	}
      default:
	break;
    }
	
    return true;
}


// [crispy] show weapon pickup messages in multiplayer games
const char *const WeaponPickupMessages[NUMWEAPONS] =
{
	NULL, // wp_fist
	NULL, // wp_pistol
	GOTSHOTGUN,
	GOTCHAINGUN,
	GOTLAUNCHER,
	GOTPLASMA,
	GOTBFG9000,
	GOTCHAINSAW,
	GOTSHOTGUN2,
};

//
// P_GiveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//
boolean
P_GiveWeapon
( player_t*	player,
  weapontype_t	weapon,
  boolean	dropped )
{
    boolean	gaveammo;
    boolean	gaveweapon;
	
    if (netgame
	&& (deathmatch!=2)
	 && !dropped )
    {
	// leave placed weapons forever on net games
	if (player->weaponowned[weapon])
	    return false;

	player->bonuscount += BONUSADD;
	player->weaponowned[weapon] = true;

	if (deathmatch)
	    P_GiveAmmo (player, weaponinfo[weapon].ammo, 5, false);
	else
	    P_GiveAmmo (player, weaponinfo[weapon].ammo, 2, false);
	player->pendingweapon = weapon;
	// [crispy] show weapon pickup messages in multiplayer games
	CT_SetMessage(player, DEH_String(WeaponPickupMessages[weapon]), false, NULL);

	if (player == &players[displayplayer])
	    S_StartSound (NULL, sfx_wpnup);
	return false;
    }
	
    if (weaponinfo[weapon].ammo != am_noammo)
    {
	// give one clip with a dropped weapon,
	// two clips with a found weapon
	// [NS] Just need to pass that it's dropped.
	gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 2, dropped);
	/*
	if (dropped)
	    gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 1);
	else
	    gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, 2);
	*/
    }
    else
	gaveammo = false;
	
    if (player->weaponowned[weapon])
	gaveweapon = false;
    else
    {
	gaveweapon = true;
	player->weaponowned[weapon] = true;
	player->pendingweapon = weapon;
    }
	
    return (gaveweapon || gaveammo);
}

 

//
// P_GiveBody
// Returns false if the body isn't needed at all
//
boolean
P_GiveBody
( player_t*	player,
  int		num )
{
    if (player->health >= MAXHEALTH)
	return false;
		
    player->health += num;
    if (player->health > MAXHEALTH)
	player->health = MAXHEALTH;
    player->mo->health = player->health;
	
    return true;
}



//
// P_GiveArmor
// Returns false if the armor is worse
// than the current armor.
//
boolean
P_GiveArmor
( player_t*	player,
  int		armortype )
{
    int		hits;
	
    hits = armortype*100;
    if (player->armorpoints >= hits)
	return false;	// don't pick up
		
    player->armortype = armortype;
    player->armorpoints = hits;
	
    return true;
}



//
// P_GiveCard
//
void
P_GiveCard
( player_t*	player,
  card_t	card )
{
    if (player->cards[card])
	return;
    
    player->bonuscount += netgame ? BONUSADD : 0; // [crispy] Fix "Key pickup resets palette"
    player->cards[card] = 1;
}


//
// P_GivePower
//
boolean
P_GivePower
( player_t*	player,
  int /*powertype_t*/	power )
{
    if (power == pw_invulnerability)
    {
	player->powers[power] = INVULNTICS;
	return true;
    }
    
    if (power == pw_invisibility)
    {
	player->powers[power] = INVISTICS;
	player->mo->flags |= MF_SHADOW;
	return true;
    }
    
    if (power == pw_infrared)
    {
	player->powers[power] = INFRATICS;
	return true;
    }
    
    if (power == pw_ironfeet)
    {
	player->powers[power] = IRONTICS;
	return true;
    }
    
    if (power == pw_strength)
    {
	P_GiveBody (player, 100);
	player->powers[power] = 1;
	return true;
    }
	
    if (player->powers[power])
	return false;	// already got it
		
    player->powers[power] = 1;
    return true;
}



//
// P_TouchSpecialThing
//
void
P_TouchSpecialThing
( mobj_t*	special,
  mobj_t*	toucher )
{
    player_t*	player;
    int		i;
    fixed_t	delta;
    int		sound;
    const boolean dropped = ((special->flags & MF_DROPPED) != 0);
		
    delta = special->z - toucher->z;

    if (delta > toucher->height
	|| delta < -8*FRACUNIT)
    {
	// out of reach
	return;
    }
    
	
    sound = sfx_itemup;	
    player = toucher->player;

    // Dead thing touching.
    // Can happen with a sliding player corpse.
    if (toucher->health <= 0)
	return;

    // Identify by sprite.
    switch (special->sprite)
    {
	// armor
      case SPR_ARM1:
	if (!P_GiveArmor (player, deh_green_armor_class))
	    return;
	CT_SetMessage(player, DEH_String(GOTARMOR), false, NULL);
	break;
		
      case SPR_ARM2:
	if (!P_GiveArmor (player, deh_blue_armor_class))
	    return;
	CT_SetMessage(player, DEH_String(GOTMEGA), false, NULL);
	break;
	
	// bonus items
      case SPR_BON1:
	player->health++;		// can go over 100%
	if (player->health > deh_max_health)
	    player->health = deh_max_health;
	player->mo->health = player->health;
	CT_SetMessage(player, DEH_String(GOTHTHBONUS), false, NULL);
	break;
	
      case SPR_BON2:
	player->armorpoints++;		// can go over 100%
	if (player->armorpoints > deh_max_armor && gameversion > exe_doom_1_2)
	    player->armorpoints = deh_max_armor;
        // deh_green_armor_class only applies to the green armor shirt;
        // for the armor helmets, armortype 1 is always used.
	if (!player->armortype)
	    player->armortype = 1;
	CT_SetMessage(player, DEH_String(GOTARMBONUS), false, NULL);
	break;
	
      case SPR_SOUL:
	player->health += deh_soulsphere_health;
	if (player->health > deh_max_soulsphere)
	    player->health = deh_max_soulsphere;
	player->mo->health = player->health;
	CT_SetMessage(player, DEH_String(GOTSUPER), false, NULL);
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
      case SPR_MEGA:
	if (gamemode != commercial)
	    return;
	player->health = deh_megasphere_health;
	player->mo->health = player->health;
        // We always give armor type 2 for the megasphere; dehacked only 
        // affects the MegaArmor.
	P_GiveArmor (player, 2);
	CT_SetMessage(player, DEH_String(GOTMSPHERE), false, NULL);
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
	// cards
	// leave cards for everyone
      case SPR_BKEY:
	if (!player->cards[it_bluecard])
	    CT_SetMessage(player, DEH_String(GOTBLUECARD), false, NULL);
	P_GiveCard (player, it_bluecard);
	if (!netgame)
	    break;
	return;
	
      case SPR_YKEY:
	if (!player->cards[it_yellowcard])
	    CT_SetMessage(player, DEH_String(GOTYELWCARD), false, NULL);
	P_GiveCard (player, it_yellowcard);
	if (!netgame)
	    break;
	return;
	
      case SPR_RKEY:
	if (!player->cards[it_redcard])
	    CT_SetMessage(player, DEH_String(GOTREDCARD), false, NULL);
	P_GiveCard (player, it_redcard);
	if (!netgame)
	    break;
	return;
	
      case SPR_BSKU:
	if (!player->cards[it_blueskull])
	    CT_SetMessage(player, DEH_String(GOTBLUESKUL), false, NULL);
	P_GiveCard (player, it_blueskull);
	if (!netgame)
	    break;
	return;
	
      case SPR_YSKU:
	if (!player->cards[it_yellowskull])
	    CT_SetMessage(player, DEH_String(GOTYELWSKUL), false, NULL);
	P_GiveCard (player, it_yellowskull);
	if (!netgame)
	    break;
	return;
	
      case SPR_RSKU:
	if (!player->cards[it_redskull])
	    CT_SetMessage(player, DEH_String(GOTREDSKULL), false, NULL);
	P_GiveCard (player, it_redskull);
	if (!netgame)
	    break;
	return;
	
	// medikits, heals
      case SPR_STIM:
	if (!P_GiveBody (player, 10))
	    return;
	CT_SetMessage(player, DEH_String(GOTSTIM), false, NULL);
	break;
	
      case SPR_MEDI:
	if (!P_GiveBody (player, 25))
	    return;

	// [JN] Fix for "Medikit that you really needed!"
	if (player->health < 50)
	    CT_SetMessage(player, DEH_String(GOTMEDINEED), false, NULL);
	else
	    CT_SetMessage(player, DEH_String(GOTMEDIKIT), false, NULL);
	break;

	
	// power ups
      case SPR_PINV:
	if (!P_GivePower (player, pw_invulnerability))
	    return;
	CT_SetMessage(player, DEH_String(GOTINVUL), false, NULL);
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
      case SPR_PSTR:
	if (!P_GivePower (player, pw_strength))
	    return;
	CT_SetMessage(player, DEH_String(GOTBERSERK), false, NULL);
	if (player->readyweapon != wp_fist)
	    player->pendingweapon = wp_fist;
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
      case SPR_PINS:
	if (!P_GivePower (player, pw_invisibility))
	    return;
	CT_SetMessage(player, DEH_String(GOTINVIS), false, NULL);
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
      case SPR_SUIT:
	if (!P_GivePower (player, pw_ironfeet))
	    return;
	CT_SetMessage(player, DEH_String(GOTSUIT), false, NULL);
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
      case SPR_PMAP:
	if (!P_GivePower (player, pw_allmap))
	    return;
	CT_SetMessage(player, DEH_String(GOTMAP), false, NULL);
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
      case SPR_PVIS:
	if (!P_GivePower (player, pw_infrared))
	    return;
	CT_SetMessage(player, DEH_String(GOTVISOR), false, NULL);
	if (gameversion > exe_doom_1_2)
	    sound = sfx_getpow;
	break;
	
	// ammo
	// [NS] Give half ammo for drops of all types.
      case SPR_CLIP:
	/*
	if (special->flags & MF_DROPPED)
	{
	    if (!P_GiveAmmo (player,am_clip,0))
		return;
	}
	else
	{
	    if (!P_GiveAmmo (player,am_clip,1))
		return;
	}
	*/
	    if (!P_GiveAmmo (player,am_clip,1,dropped))
		return;
	CT_SetMessage(player, DEH_String(GOTCLIP), false, NULL);
	break;
	
      case SPR_AMMO:
	if (!P_GiveAmmo (player, am_clip,5,dropped))
	    return;
	CT_SetMessage(player, DEH_String(GOTCLIPBOX), false, NULL);
	break;
	
      case SPR_ROCK:
	if (!P_GiveAmmo (player, am_misl,1,dropped))
	    return;
	CT_SetMessage(player, DEH_String(GOTROCKET), false, NULL);
	break;
	
      case SPR_BROK:
	if (!P_GiveAmmo (player, am_misl,5,dropped))
	    return;
	CT_SetMessage(player, DEH_String(GOTROCKBOX), false, NULL);
	break;
	
      case SPR_CELL:
	if (!P_GiveAmmo (player, am_cell,1,dropped))
	    return;
	CT_SetMessage(player, DEH_String(GOTCELL), false, NULL);
	break;
	
      case SPR_CELP:
	if (!P_GiveAmmo (player, am_cell,5,dropped))
	    return;
	CT_SetMessage(player, DEH_String(GOTCELLBOX), false, NULL);
	break;
	
      case SPR_SHEL:
	if (!P_GiveAmmo (player, am_shell,1,dropped))
	    return;
	CT_SetMessage(player, DEH_String(GOTSHELLS), false, NULL);
	break;
	
      case SPR_SBOX:
	if (!P_GiveAmmo (player, am_shell,5,dropped))
	    return;
	CT_SetMessage(player, DEH_String(GOTSHELLBOX), false, NULL);
	break;
	
      case SPR_BPAK:
	if (!player->backpack)
	{
	    for (i=0 ; i<NUMAMMO ; i++)
		player->maxammo[i] *= 2;
	    player->backpack = true;
	}
	for (i=0 ; i<NUMAMMO ; i++)
	    P_GiveAmmo (player, i, 1, false);
	CT_SetMessage(player, DEH_String(GOTBACKPACK), false, NULL);
	break;
	
	// weapons
	// [NS] Give half ammo for all dropped weapons.
      case SPR_BFUG:
	if (!P_GiveWeapon (player, wp_bfg, dropped) )
	    return;
	CT_SetMessage(player, DEH_String(GOTBFG9000), false, NULL);
	sound = sfx_wpnup;	
	break;
	
      case SPR_MGUN:
        if (!P_GiveWeapon(player, wp_chaingun,
                          (special->flags & MF_DROPPED) != 0))
            return;
	CT_SetMessage(player, DEH_String(GOTCHAINGUN), false, NULL);
	sound = sfx_wpnup;	
	break;
	
      case SPR_CSAW:
	if (!P_GiveWeapon (player, wp_chainsaw, dropped) )
	    return;
	CT_SetMessage(player, DEH_String(GOTCHAINSAW), false, NULL);
	sound = sfx_wpnup;	
	break;
	
      case SPR_LAUN:
	if (!P_GiveWeapon (player, wp_missile, dropped) )
	    return;
	CT_SetMessage(player, DEH_String(GOTLAUNCHER), false, NULL);
	sound = sfx_wpnup;	
	break;
	
      case SPR_PLAS:
	if (!P_GiveWeapon (player, wp_plasma, dropped) )
	    return;
	CT_SetMessage(player, DEH_String(GOTPLASMA), false, NULL);
	sound = sfx_wpnup;	
	break;
	
      case SPR_SHOT:
        if (!P_GiveWeapon(player, wp_shotgun,
                          (special->flags & MF_DROPPED) != 0))
            return;
	CT_SetMessage(player, DEH_String(GOTSHOTGUN), false, NULL);
	sound = sfx_wpnup;	
	break;
		
      case SPR_SGN2:
        if (!P_GiveWeapon(player, wp_supershotgun,
                          (special->flags & MF_DROPPED) != 0))
            return;
	CT_SetMessage(player, DEH_String(GOTSHOTGUN2), false, NULL);
	sound = sfx_wpnup;	
	break;
		
	// [NS] Beta pickups.
      case SPR_BON3:
	player->message = DEH_String(BETA_BONUS3);
	break;

      case SPR_BON4:
	player->message = DEH_String(BETA_BONUS4);
	break;

      default:
	I_Error ("P_SpecialThing: Unknown gettable thing");
    }
	
    if (special->flags & MF_COUNTITEM)
	player->itemcount++;
    P_RemoveMobj (special);

    player->bonuscount += BONUSADD;
    // [JN] Limit bonus palette duration to 4 seconds.
    if (player->bonuscount > 4 * TICRATE)
	player->bonuscount = 4 * TICRATE;

    if (player == &players[displayplayer])
	S_StartSound (NULL, sound);
}


//
// KillMobj
//
void
P_KillMobj
( mobj_t*	source,
  mobj_t*	target )
{
    mobjtype_t	item;
    mobj_t*	mo;
	
    target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);

    if (target->type != MT_SKULL)
	target->flags &= ~MF_NOGRAVITY;

    target->flags |= MF_CORPSE|MF_DROPOFF;
    target->height >>= 2;
    target->geartics = 15 * TICRATE;  // [JN] Limit torque to 15 seconds.

    if (source && source->player)
    {
        // [JN] Ressurected monsters counter.
        if (target->resurrected == true)
        {
            source->player->extrakillcount++;
        }
        // count for intermission
        else if (target->flags & MF_COUNTKILL)
        {
            source->player->killcount++;	
        }

        if (target->player)
        {
            source->player->frags[target->player-players]++;
        }
    }
    else if (!netgame/* && (target->flags & MF_COUNTKILL)*/)
    {
        // count all monster deaths,
        // even those caused by other monsters
        // [JN] ... but still count ressurected monsters separatelly.
        if (target->resurrected == true)
        {
            players[0].extrakillcount++;
        }
        else if (target->flags & MF_COUNTKILL)
        {
            players[0].killcount++;	
        }
    }
    
    if (target->player)
    {
	// count environment kills against you
	if (!source)	
	    target->player->frags[target->player-players]++;
			
	target->flags &= ~MF_SOLID;
	target->player->playerstate = PST_DEAD;
	P_DropWeapon (target->player);
	// [crispy] center view when dying
	target->player->centering = true;
	// [JN] & [crispy] Reset the yellow bonus palette when the player dies
	target->player->bonuscount = 0;
	// [JN] & [crispy] Remove the effect of the inverted palette when the player dies
	target->player->fixedcolormap = target->player->powers[pw_infrared] ? 1 : 0;

	if (target->player == &players[consoleplayer]
	    && automapactive
	    && !demoplayback) // [crispy] killough 11/98: don't switch out in demos, though
	{
	    // don't die in auto map,
	    // switch view prior to dying
	    AM_Stop ();
	}
	
    }

    /*
    // [crispy] Lost Soul, Pain Elemental and Barrel explosions are translucent
    if (target->type == MT_SKULL ||
        target->type == MT_PAIN ||
        target->type == MT_BARREL)
        target->flags |= MF_TRANSLUCENT;
    */

    if (target->health < -target->info->spawnhealth 
	&& target->info->xdeathstate)
    {
	P_SetMobjState (target, target->info->xdeathstate);
    }
    else
	P_SetMobjState (target, target->info->deathstate);
    target->tics -= P_Random()&3;

    // [crispy] randomly flip corpse, blood and death animation sprites
    if (target->flags & MF_FLIPPABLE)
    {
	target->health = (target->health & (int)~1) - (ID_RealRandom() & 1);
    }

    if (target->tics < 1)
	target->tics = 1;
		
    //	I_StartSound (&actor->r, actor->info->deathsound);

    // In Chex Quest, monsters don't drop items.

    if (gameversion == exe_chex)
    {
        return;
    }

    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    if (target->info->droppeditem != MT_NULL) // [crispy] drop generalization
    {
        item = target->info->droppeditem;
    }
    else
        return;

    // [JN] Tossing of dropped items feature (from DOOM Retro).
    if (phys_toss_drop && singleplayer)
    {
    mo = P_SpawnMobj(target->x, target->y, target->floorz
                    + target->height * 3 / 2 - 3 * FRACUNIT, item);
    mo->momx = (target->momx >> 1) + (ID_Random() << 8);
    mo->momy = (target->momy >> 1) + (ID_Random() << 8);
    mo->momz = 2 * FRACUNIT + (M_Random() << 9);
    }
    else
    {
    mo = P_SpawnMobj (target->x,target->y,ONFLOORZ, item);
    }
    mo->flags |= MF_DROPPED;	// special versions of items
}




//
// P_DamageMobj
// Damages both enemies and players
// "inflictor" is the thing that caused the damage
//  creature or missile, can be NULL (slime, etc)
// "source" is the thing to target after taking damage
//  creature or NULL
// Source and inflictor are the same for melee attacks.
// Source can be NULL for slime, barrel explosions
// and other environmental stuff.
//
void
P_DamageMobj
( mobj_t*	target,
  mobj_t*	inflictor,
  mobj_t*	source,
  int 		damage )
{
    unsigned	ang;
    int		saved;
    player_t*	player;
    fixed_t	thrust;
	
    if ( !(target->flags & MF_SHOOTABLE) )
	return;	// shouldn't happen...
		
    if (target->health <= 0)
	return;

    if ( target->flags & MF_SKULLFLY )
    {
	target->momx = target->momy = target->momz = 0;
    }
	
    player = target->player;
    if (player && gameskill == sk_baby)
	damage >>= 1; 	// take half damage in trainer mode
		

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if (inflictor
	&& !(target->flags & MF_NOCLIP)
	&& (!source
	    || !source->player
	    || source->player->readyweapon != wp_chainsaw))
    {
	ang = R_PointToAngle2 ( inflictor->x,
				inflictor->y,
				target->x,
				target->y);
		
	thrust = damage*(FRACUNIT>>3)*100/target->info->mass;

	// make fall forwards sometimes
	if ( damage < 40
	     && damage > target->health
	     && target->z - inflictor->z > 64*FRACUNIT
	     && (P_Random ()&1) )
	{
	    ang += ANG180;
	    thrust *= 4;
	}
		
	ang >>= ANGLETOFINESHIFT;
	target->momx += FixedMul (thrust, finecosine[ang]);
	target->momy += FixedMul (thrust, finesine[ang]);
    }
    
    // player specific
    if (player)
    {
	// end of game hell hack
	if (target->subsector->sector->special == 11
	    && damage >= target->health)
	{
	    damage = target->health - 1;
	}
	

	// Below certain threshold,
	// ignore damage in GOD mode, or with INVUL power.
	if ( damage < 1000
	     && ( (player->cheats&CF_GODMODE)
		  || player->powers[pw_invulnerability] ) )
	{
	    return;
	}
	
	if (player->armortype)
	{
	    if (player->armortype == 1)
		saved = damage/3;
	    else
		saved = damage/2;
	    
	    if (player->armorpoints <= saved)
	    {
		// armor is used up
		saved = player->armorpoints;
		player->armortype = 0;
	    }
	    player->armorpoints -= saved;
	    damage -= saved;
	}
	player->health -= damage; 	// mirror mobj health here for Dave
	player->health_negative = player->health;  // [JN] Update negative health value.
    // [JN] BUDDHA cheat.
	if (player->cheats & CF_BUDDHA && player->health < 1)
        player->health = 1;
	else
	if (player->health < 0)
	    player->health = 0;
	
	player->attacker = source;
	player->damagecount += damage;	// add damage after armor / invuln

	// [JN] Smooth palette. Avoid using too low red values, since palette
	// is counted from alpha values, not from palette indexes.
	if (vis_smooth_palette && player->damagecount < 16) 
	    player->damagecount = 16;
	if (player->damagecount > 100)
	    player->damagecount = 100;	// teleport stomp does 10k points...
    }
    
    // do the damage	
    target->health -= damage;	
    // [JN] BUDDHA cheat.
    if (player && player->cheats & CF_BUDDHA && target->health < 1)
    {
	    target->health = 1;
    }
    else
    if (target->health <= 0)
    {
    // [crispy] the lethal pellet of a point-blank SSG blast
    // gets an extra damage boost for the occasional gib chance
    if (singleplayer && phys_ssg_tear_monsters /*&& !strict_mode*/)
    {
        if (source && source->player
        && source->player->readyweapon == wp_supershotgun
        && target->info->xdeathstate && P_CheckMeleeRange(target) && damage >= 10)
        {
            target->health -= target->info->spawnhealth;
        }
    }
	P_KillMobj (source, target);
	return;
    }

    if ( (P_Random () < target->info->painchance)
	 && !(target->flags&MF_SKULLFLY) )
    {
	target->flags |= MF_JUSTHIT;	// fight back!
	
	P_SetMobjState (target, target->info->painstate);
    }
			
    target->reactiontime = 0;		// we're awake now...	

    if ( (!target->threshold || target->type == MT_VILE)
	 && source && (source != target || gameversion < exe_doom_1_5)
	 && source->type != MT_VILE)
    {
	// if not intent on another player,
	// chase after this one
	target->target = source;
	target->threshold = BASETHRESHOLD;
	if (target->state == &states[target->info->spawnstate]
	    && target->info->seestate != S_NULL)
	    P_SetMobjState (target, target->info->seestate);
    }
			
}

