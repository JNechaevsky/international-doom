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

// DStrings.h

//---------------------------------------------------------------------------
//
// P_inter.c
//
//---------------------------------------------------------------------------

// Keys

#define TXT_GOTBLUEKEY			"BLUE KEY"
#define TXT_GOTYELLOWKEY		"YELLOW KEY"
#define TXT_GOTGREENKEY			"GREEN KEY"

// Artifacts

#define TXT_ARTIHEALTH			"QUARTZ FLASK"
#define TXT_ARTIFLY				"WINGS OF WRATH"
#define TXT_ARTIINVULNERABILITY	"RING OF INVINCIBILITY"
#define TXT_ARTITOMEOFPOWER		"TOME OF POWER"
#define TXT_ARTIINVISIBILITY	"SHADOWSPHERE"
#define TXT_ARTIEGG				"MORPH OVUM"
#define TXT_ARTISUPERHEALTH		"MYSTIC URN"
#define TXT_ARTITORCH			"TORCH"
#define TXT_ARTIFIREBOMB		"TIME BOMB OF THE ANCIENTS"
#define TXT_ARTITELEPORT		"CHAOS DEVICE"

// Items

#define TXT_ITEMHEALTH			"CRYSTAL VIAL"
#define TXT_ITEMBAGOFHOLDING	"BAG OF HOLDING"
#define TXT_ITEMSHIELD1			"SILVER SHIELD"
#define TXT_ITEMSHIELD2			"ENCHANTED SHIELD"
#define TXT_ITEMSUPERMAP		"MAP SCROLL"

// Ammo

#define TXT_AMMOGOLDWAND1		"WAND CRYSTAL"
#define TXT_AMMOGOLDWAND2		"CRYSTAL GEODE"
#define TXT_AMMOMACE1			"MACE SPHERES"
#define TXT_AMMOMACE2			"PILE OF MACE SPHERES"
#define TXT_AMMOCROSSBOW1		"ETHEREAL ARROWS"
#define TXT_AMMOCROSSBOW2		"QUIVER OF ETHEREAL ARROWS"
#define TXT_AMMOBLASTER1		"CLAW ORB"
#define TXT_AMMOBLASTER2		"ENERGY ORB"
#define TXT_AMMOSKULLROD1		"LESSER RUNES"
#define TXT_AMMOSKULLROD2		"GREATER RUNES"
#define TXT_AMMOPHOENIXROD1		"FLAME ORB"
#define TXT_AMMOPHOENIXROD2		"INFERNO ORB"

// Weapons

#define TXT_WPNMACE				"FIREMACE"
#define TXT_WPNCROSSBOW			"ETHEREAL CROSSBOW"
#define TXT_WPNBLASTER			"DRAGON CLAW"
#define TXT_WPNSKULLROD			"HELLSTAFF"
#define TXT_WPNPHOENIXROD		"PHOENIX ROD"
#define TXT_WPNGAUNTLETS		"GAUNTLETS OF THE NECROMANCER"

//---------------------------------------------------------------------------
//
// SB_bar.c
//
//---------------------------------------------------------------------------

#define TXT_CHEATGODON			"GOD MODE ON"
#define TXT_CHEATGODOFF			"GOD MODE OFF"
#define TXT_CHEATNOCLIPON		"NO CLIPPING ON"
#define TXT_CHEATNOCLIPOFF		"NO CLIPPING OFF"
#define TXT_CHEATWEAPONS		"ALL WEAPONS"
#define TXT_CHEATWEAPKEYS		"ALL WEAPONS AND KEYS"
#define TXT_CHEATFLIGHTON		"FLIGHT ON"
#define TXT_CHEATFLIGHTOFF		"FLIGHT OFF"
#define TXT_CHEATPOWERON		"POWER ON"
#define TXT_CHEATPOWEROFF		"POWER OFF"
#define TXT_CHEATHEALTH			"FULL HEALTH"
#define TXT_CHEATKEYS			"ALL KEYS"
#define TXT_CHEATARTIFACTS1		"CHOOSE AN ARTIFACT ( A - J )"
#define TXT_CHEATARTIFACTS2		"HOW MANY ( 1 - 9 )"
#define TXT_CHEATARTIFACTS3		"YOU GOT IT"
#define TXT_CHEATARTIFACTS4		"ALL ARTIFACTS"
#define TXT_CHEATARTIFACTSFAIL	"BAD INPUT"
#define TXT_CHEATSCREENSHOT		"SCREENSHOT"
#define TXT_CHEATCHICKENON		"CHICKEN ON"
#define TXT_CHEATCHICKENOFF		"CHICKEN OFF"
#define TXT_CHEATMASSACRE		"MASSACRE"
#define TXT_MUS				"MUSIC CHANGE"
#define TXT_NOMUS			"IMPOSSIBLE SELECTION"
#define TXT_CHOPPERS			"... DOESN'T SUCK - GM"

//---------------------------------------------------------------------------
//
// P_doors.c
//
//---------------------------------------------------------------------------

#define TXT_NEEDBLUEKEY			"YOU NEED A BLUE KEY TO OPEN THIS DOOR"
#define TXT_NEEDGREENKEY		"YOU NEED A GREEN KEY TO OPEN THIS DOOR"
#define TXT_NEEDYELLOWKEY		"YOU NEED A YELLOW KEY TO OPEN THIS DOOR"

//---------------------------------------------------------------------------
//
// G_game.c
//
//---------------------------------------------------------------------------

#define TXT_GAMESAVED			"GAME SAVED"

//---------------------------------------------------------------------------
//
// AM_map.c
//
//---------------------------------------------------------------------------

#define AMSTR_FOLLOWON		"FOLLOW MODE ON"
#define AMSTR_FOLLOWOFF		"FOLLOW MODE OFF"

#define AMSTR_GRIDON		"GRID ON"
#define AMSTR_GRIDOFF		"GRID OFF"

#define AMSTR_MARKEDSPOT	"MARKED SPOT"
#define AMSTR_MARKCLEARED	"CLEARED SPOT"
#define AMSTR_MARKSCLEARED	"ALL MARKS CLEARED"

//---------------------------------------------------------------------------
//
// F_finale.c
//
//---------------------------------------------------------------------------

#define E1TEXT	"with the destruction of the iron\n"\
					"liches and their minions, the last\n"\
					"of the undead are cleared from this\n"\
					"plane of existence.\n\n"\
					"those creatures had to come from\n"\
					"somewhere, though, and you have the\n"\
					"sneaky suspicion that the fiery\n"\
					"portal of hell's maw opens onto\n"\
					"their home dimension.\n\n"\
					"to make sure that more undead\n"\
					"(or even worse things) don't come\n"\
					"through, you'll have to seal hell's\n"\
					"maw from the other side. of course\n"\
					"this means you may get stuck in a\n"\
					"very unfriendly world, but no one\n"\
					"ever said being a Heretic was easy!"

#define E2TEXT "the mighty maulotaurs have proved\n"\
					"to be no match for you, and as\n"\
					"their steaming corpses slide to the\n"\
					"ground you feel a sense of grim\n"\
					"satisfaction that they have been\n"\
					"destroyed.\n\n"\
					"the gateways which they guarded\n"\
					"have opened, revealing what you\n"\
					"hope is the way home. but as you\n"\
					"step through, mocking laughter\n"\
					"rings in your ears.\n\n"\
					"was some other force controlling\n"\
					"the maulotaurs? could there be even\n"\
					"more horrific beings through this\n"\
					"gate? the sweep of a crystal dome\n"\
					"overhead where the sky should be is\n"\
					"certainly not a good sign...."

#define E3TEXT	"the death of d'sparil has loosed\n"\
					"the magical bonds holding his\n"\
					"creatures on this plane, their\n"\
					"dying screams overwhelming his own\n"\
					"cries of agony.\n\n"\
					"your oath of vengeance fulfilled,\n"\
					"you enter the portal to your own\n"\
					"world, mere moments before the dome\n"\
					"shatters into a million pieces.\n\n"\
					"but if d'sparil's power is broken\n"\
					"forever, why don't you feel safe?\n"\
					"was it that last shout just before\n"\
					"his death, the one that sounded\n"\
					"like a curse? or a summoning? you\n"\
					"can't really be sure, but it might\n"\
					"just have been a scream.\n\n"\
					"then again, what about the other\n"\
					"serpent riders?"

#define E4TEXT		"you thought you would return to your\n"\
					"own world after d'sparil died, but\n"\
					"his final act banished you to his\n"\
					"own plane. here you entered the\n"\
					"shattered remnants of lands\n"\
					"conquered by d'sparil. you defeated\n"\
					"the last guardians of these lands,\n"\
					"but now you stand before the gates\n"\
					"to d'sparil's stronghold. until this\n"\
					"moment you had no doubts about your\n"\
					"ability to face anything you might\n"\
					"encounter, but beyond this portal\n"\
					"lies the very heart of the evil\n"\
					"which invaded your world. d'sparil\n"\
					"might be dead, but the pit where he\n"\
					"was spawned remains. now you must\n"\
					"enter that pit in the hopes of\n"\
					"finding a way out. and somewhere,\n"\
					"in the darkest corner of d'sparil's\n"\
					"demesne, his personal bodyguards\n"\
					"await your arrival ..."

#define E5TEXT		"as the final maulotaur bellows his\n"\
					"death-agony, you realize that you\n"\
					"have never come so close to your own\n"\
					"destruction. not even the fight with\n"\
					"d'sparil and his disciples had been\n"\
					"this desperate. grimly you stare at\n"\
					"the gates which open before you,\n"\
					"wondering if they lead home, or if\n"\
					"they open onto some undreamed-of\n"\
					"horror. you find yourself wondering\n"\
					"if you have the strength to go on,\n"\
					"if nothing but death and pain await\n"\
					"you. but what else can you do, if\n"\
					"the will to fight is gone? can you\n"\
					"force yourself to continue in the\n"\
					"face of such despair? do you have\n"\
					"the courage? you find, in the end,\n"\
					"that it is not within you to\n"\
					"surrender without a fight. eyes\n"\
					"wide, you go to meet your fate."

// -----------------------------------------------------------------------------
// [JN] ID-specific strings.
// -----------------------------------------------------------------------------

#define ID_EXTHUD_ON        "EXTENDED HUD ON"
#define ID_EXTHUD_OFF       "EXTENDED HUD OFF"

#define ID_SPECTATOR_ON     "SPECTATOR MODE ON"
#define ID_SPECTATOR_OFF    "SPECTATOR MODE OFF"

#define ID_FREEZE_ON        "FREEZE MODE ON"
#define ID_FREEZE_OFF       "FREEZE MODE OFF"
#define ID_FREEZE_NA_R      "FREEZE NOT AVAILABLE IN DEMO RECORDING"
#define ID_FREEZE_NA_P      "FREEZE NOT AVAILABLE IN DEMO PLAYING"
#define ID_FREEZE_NA_N      "FREEZE NOT AVAILABLE IN MULTIPLAYER GAME"

#define ID_NOTARGET_ON      "NOTARGET MODE ON"
#define ID_NOTARGET_OFF     "NOTARGET MODE OFF"
#define ID_NOTARGET_NA_R    "NOTARGET NOT AVAILABLE IN DEMO RECORDING"
#define ID_NOTARGET_NA_P    "NOTARGET NOT AVAILABLE IN DEMO PLAYING"
#define ID_NOTARGET_NA_N    "NOTARGET NOT AVAILABLE IN MULTIPLAYER GAME"

#define ID_BUDDHA_ON        "BUDDHA MODE ON"
#define ID_BUDDHA_OFF       "BUDDHA MODE OFF"
#define ID_BUDDHA_NA_R      "BUDDHA NOT AVAILABLE IN DEMO RECORDING"
#define ID_BUDDHA_NA_P      "BUDDHA NOT AVAILABLE IN DEMO PLAYING"
#define ID_BUDDHA_NA_N      "BUDDHA NOT AVAILABLE IN MULTIPLAYER GAME"

#define ID_AUTOMAPROTATE_ON     "ROTATE MODE ON"
#define ID_AUTOMAPROTATE_OFF    "ROTATE MODE OFF"
#define ID_AUTOMAPOVERLAY_ON    "OVERLAY MODE ON"
#define ID_AUTOMAPOVERLAY_OFF   "OVERLAY MODE OFF"
#define ID_AUTOMAPMOUSEPAN_ON   "MOUSE PANNING MODE ON"
#define ID_AUTOMAPMOUSEPAN_OFF  "MOUSE PANNING MODE OFF"

#define ID_AUTORUN_ON      "ALWAYS RUN ON"
#define ID_AUTORUN_OFF     "ALWAYS RUN OFF"

#define ID_MLOOK_ON        "MOUSE LOOK ON"
#define ID_MLOOK_OFF       "MOUSE LOOK OFF"

#define ID_NOVERT_ON       "VERTICAL MOUSE MOVEMENT OFF"
#define ID_NOVERT_OFF      "VERTICAL MOUSE MOVEMENT ON"

#define ID_SECRET_FOUND     "A SECRET IS REVEALED!"

#define DETAILHI    "HIGH DETAIL"
#define DETAILLO    "LOW DETAIL"

#define GAMMALVL_N050   "GAMMA CORRECTION LEVEL -0.50"
#define GAMMALVL_N055   "GAMMA CORRECTION LEVEL -0.55"
#define GAMMALVL_N060   "GAMMA CORRECTION LEVEL -0.60"
#define GAMMALVL_N065   "GAMMA CORRECTION LEVEL -0.65"
#define GAMMALVL_N070   "GAMMA CORRECTION LEVEL -0.70"
#define GAMMALVL_N075   "GAMMA CORRECTION LEVEL -0.75"
#define GAMMALVL_N080   "GAMMA CORRECTION LEVEL -0.80"
#define GAMMALVL_N085   "GAMMA CORRECTION LEVEL -0.85"
#define GAMMALVL_N090   "GAMMA CORRECTION LEVEL -0.90"
#define GAMMALVL_N095   "GAMMA CORRECTION LEVEL -0.95"
#define GAMMALVL_OFF    "GAMMA CORRECTION OFF"
#define GAMMALVL_010    "GAMMA CORRECTION LEVEL 0.1"
#define GAMMALVL_020    "GAMMA CORRECTION LEVEL 0.2"
#define GAMMALVL_030    "GAMMA CORRECTION LEVEL 0.3"
#define GAMMALVL_040    "GAMMA CORRECTION LEVEL 0.4"
#define GAMMALVL_050    "GAMMA CORRECTION LEVEL 0.5"
#define GAMMALVL_060    "GAMMA CORRECTION LEVEL 0.6"
#define GAMMALVL_070    "GAMMA CORRECTION LEVEL 0.7"
#define GAMMALVL_080    "GAMMA CORRECTION LEVEL 0.8"
#define GAMMALVL_090    "GAMMA CORRECTION LEVEL 0.9"
#define GAMMALVL_100    "GAMMA CORRECTION LEVEL 1.0"
#define GAMMALVL_110    "GAMMA CORRECTION LEVEL 1.1"
#define GAMMALVL_120    "GAMMA CORRECTION LEVEL 1.2"
#define GAMMALVL_130    "GAMMA CORRECTION LEVEL 1.3"
#define GAMMALVL_140    "GAMMA CORRECTION LEVEL 1.4"
#define GAMMALVL_150    "GAMMA CORRECTION LEVEL 1.5"
#define GAMMALVL_160    "GAMMA CORRECTION LEVEL 1.6"
#define GAMMALVL_170    "GAMMA CORRECTION LEVEL 1.7"
#define GAMMALVL_180    "GAMMA CORRECTION LEVEL 1.8"
#define GAMMALVL_190    "GAMMA CORRECTION LEVEL 1.9"
#define GAMMALVL_200    "GAMMA CORRECTION LEVEL 2.0"
#define GAMMALVL_220    "GAMMA CORRECTION LEVEL 2.2"
#define GAMMALVL_240    "GAMMA CORRECTION LEVEL 2.4"
#define GAMMALVL_260    "GAMMA CORRECTION LEVEL 2.6"
#define GAMMALVL_280    "GAMMA CORRECTION LEVEL 2.8"
#define GAMMALVL_300    "GAMMA CORRECTION LEVEL 3.0"
#define GAMMALVL_320    "GAMMA CORRECTION LEVEL 3.2"
#define GAMMALVL_340    "GAMMA CORRECTION LEVEL 3.4"
#define GAMMALVL_360    "GAMMA CORRECTION LEVEL 3.6"
#define GAMMALVL_380    "GAMMA CORRECTION LEVEL 3.8"
#define GAMMALVL_400    "GAMMA CORRECTION LEVEL 4.0"

#define ID_OST_NA_1        "HERETIC[MUS[REMIX.WAD AND"
#define ID_OST_NA_2        "HERETIC[MUS[ORIG.WAD ARE NOT LOADED"
#define ID_OST_OFF         "PREFERRED SOUNDTRACK: OFF"
#define ID_OST_REMIX       "PREFERRED SOUNDTRACK: REMIX"
#define ID_OST_ORIGINAL    "PREFERRED SOUNDTRACK: ORIGINAL"
