//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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


// sounds.c

#include "doomdef.h"
#include "i_sound.h"
#include "sounds.h"

// Music info

#define MUSIC(name) \
    { name, 0, NULL, NULL }

// [crispy] support dedicated music tracks for each map
musicinfo_t S_music[][2] = {
    {MUSIC("MUS_E1M1"), {NULL}},           // 1-1
    {MUSIC("MUS_E1M2"), {NULL}},
    {MUSIC("MUS_E1M3"), {NULL}},
    {MUSIC("MUS_E1M4"), {NULL}},
    {MUSIC("MUS_E1M5"), {NULL}},
    {MUSIC("MUS_E1M6"), {NULL}},
    {MUSIC("MUS_E1M7"), {NULL}},
    {MUSIC("MUS_E1M8"), {NULL}},
    {MUSIC("MUS_E1M9"), {NULL}},

    {MUSIC("MUS_E2M1"), {NULL}},            // 2-1
    {MUSIC("MUS_E2M2"), {NULL}},
    {MUSIC("MUS_E2M3"), {NULL}},
    {MUSIC("MUS_E2M4"), {NULL}},
    {MUSIC("MUS_E1M4"), MUSIC("MUS_E2M5")},
    {MUSIC("MUS_E2M6"), {NULL}},
    {MUSIC("MUS_E2M7"), {NULL}},
    {MUSIC("MUS_E2M8"), {NULL}},
    {MUSIC("MUS_E2M9"), {NULL}},

    {MUSIC("MUS_E1M1"), MUSIC("MUS_E3M1")}, // 3-1
    {MUSIC("MUS_E3M2"), {NULL}},
    {MUSIC("MUS_E3M3"), {NULL}},
    {MUSIC("MUS_E1M6"), MUSIC("MUS_E3M4")},
    {MUSIC("MUS_E1M3"), MUSIC("MUS_E3M5")},
    {MUSIC("MUS_E1M2"), MUSIC("MUS_E3M6")},
    {MUSIC("MUS_E1M5"), MUSIC("MUS_E3M7")},
    {MUSIC("MUS_E1M9"), MUSIC("MUS_E3M8")},
    {MUSIC("MUS_E2M6"), MUSIC("MUS_E3M9")},

    {MUSIC("MUS_E1M6"), MUSIC("MUS_E4M1")}, // 4-1
    {MUSIC("MUS_E1M2"), MUSIC("MUS_E4M2")},
    {MUSIC("MUS_E1M3"), MUSIC("MUS_E4M3")},
    {MUSIC("MUS_E1M4"), MUSIC("MUS_E4M4")},
    {MUSIC("MUS_E1M5"), MUSIC("MUS_E4M5")},
    {MUSIC("MUS_E1M1"), MUSIC("MUS_E4M6")},
    {MUSIC("MUS_E1M7"), MUSIC("MUS_E4M7")},
    {MUSIC("MUS_E1M8"), MUSIC("MUS_E4M8")},
    {MUSIC("MUS_E1M9"), MUSIC("MUS_E4M9")},

    {MUSIC("MUS_E2M1"), MUSIC("MUS_E5M1")}, // 5-1
    {MUSIC("MUS_E2M2"), MUSIC("MUS_E5M2")},
    {MUSIC("MUS_E2M3"), MUSIC("MUS_E5M3")},
    {MUSIC("MUS_E2M4"), MUSIC("MUS_E5M4")},
    {MUSIC("MUS_E1M4"), MUSIC("MUS_E5M5")},
    {MUSIC("MUS_E2M6"), MUSIC("MUS_E5M6")},
    {MUSIC("MUS_E2M7"), MUSIC("MUS_E5M7")},
    {MUSIC("MUS_E2M8"), MUSIC("MUS_E5M8")},
    {MUSIC("MUS_E2M9"), MUSIC("MUS_E5M9")},

    {MUSIC("MUS_E3M2"), MUSIC("MUS_E6M1")}, // 6-1
    {MUSIC("MUS_E3M3"), MUSIC("MUS_E6M2")}, // 6-2
    {MUSIC("MUS_E1M6"), MUSIC("MUS_E6M3")}, // 6-3

    {MUSIC("MUS_TITL"), {NULL}},
    {MUSIC("MUS_INTR"), {NULL}},
    {MUSIC("MUS_CPTD"), {NULL}}
};

 // [JN] Support Remastered/Original sound track from H+H rerelease.
musicinfo_t S_music_Remaster[][2] = {
    {MUSIC("H_DARK"),   MUSIC("O_DARK")},   // 1-1
    {MUSIC("H_FRED"),   MUSIC("O_FRED")},   // 1-2
    {MUSIC("H_DED"),    MUSIC("O_DED")},    // 1-3
    {MUSIC("H_ELF"),    MUSIC("O_ELF")},    // 1-4
    {MUSIC("H_MAREK"),  MUSIC("O_MAREK")},  // 1-5
    {MUSIC("H_MIST"),   MUSIC("O_MIST")},   // 1-6
    {MUSIC("H_MOLE"),   MUSIC("O_MOLE")},   // 1-7
    {MUSIC("H_JOHN"),   MUSIC("O_JOHN")},   // 1-8
    {MUSIC("H_ATCOTY"), MUSIC("O_ATCOTY")}, // 1-9

    {MUSIC("H_ACACIA"), MUSIC("O_ACACIA")}, // 2-1
    {MUSIC("H_ATLAS"),  MUSIC("O_ATLAS")},  // 2-2
    {MUSIC("H_BLUROC"), MUSIC("O_BLUROC")}, // 2-3
    {MUSIC("H_BUILD"),  MUSIC("O_BUILD")},  // 2-4
    {MUSIC("H_ELF"),    MUSIC("O_ELF")},    // 2-5
    {MUSIC("H_ONYX"),   MUSIC("O_ONYX")},   // 2-6
    {MUSIC("H_WAIT"),   MUSIC("O_WAIT")},   // 2-7
    {MUSIC("H_ARPO"),   MUSIC("O_ARPO")},   // 2-8
    {MUSIC("H_OLKIN"),  MUSIC("O_OLKIN")},  // 2-9

    {MUSIC("H_DARK"),   MUSIC("O_DARK")},   // 3-1
    {MUSIC("H_DRIVE"),  MUSIC("O_DRIVE")},  // 3-2
    {MUSIC("H_WATCH"),  MUSIC("O_WATCH")},  // 3-3
    {MUSIC("H_MIST"),   MUSIC("O_MIST")},   // 3-4
    {MUSIC("H_DED"),    MUSIC("O_DED")},    // 3-5
    {MUSIC("H_FRED"),   MUSIC("O_FRED")},   // 3-6
    {MUSIC("H_MAREK"),  MUSIC("O_MAREK")},  // 3-7
    {MUSIC("H_ATCOTY"), MUSIC("O_ATCOTY")}, // 3-8
    {MUSIC("H_ONYX"),   MUSIC("O_ONYX")},   // 3-9

    {MUSIC("H_MIST"),   MUSIC("O_MIST")},   // 4-1
    {MUSIC("H_FRED"),   MUSIC("O_FRED")},   // 4-2
    {MUSIC("H_DED"),    MUSIC("O_DED")},    // 4-3
    {MUSIC("H_ELF"),    MUSIC("O_ELF")},    // 4-4
    {MUSIC("H_MAREK"),  MUSIC("O_MAREK")},  // 4-5
    {MUSIC("H_DARK"),   MUSIC("O_DARK")},   // 4-6
    {MUSIC("H_MOLE"),   MUSIC("O_MOLE")},   // 4-7
    {MUSIC("H_JOHN"),   MUSIC("O_JOHN")},   // 4-8
    {MUSIC("H_ATCOTY"), MUSIC("O_ATCOTY")}, // 4-9

    {MUSIC("H_ACACIA"), MUSIC("O_ACACIA")}, // 5-1
    {MUSIC("H_ATLAS"),  MUSIC("O_ATLAS")},  // 5-2
    {MUSIC("H_BLUROC"), MUSIC("O_BLUROC")}, // 5-3
    {MUSIC("H_BUILD"),  MUSIC("O_BUILD")},  // 5-4
    {MUSIC("H_ELF"),    MUSIC("O_ELF")},    // 5-5
    {MUSIC("H_ONYX"),   MUSIC("O_ONYX")},   // 5-6
    {MUSIC("H_WAIT"),   MUSIC("O_WAIT")},   // 5-7
    {MUSIC("H_ARPO"),   MUSIC("O_ARPO")},   // 5-8
    {MUSIC("H_OLKIN"),  MUSIC("O_OLKIN")},  // 5-9

    {MUSIC("H_DRIVE"),  MUSIC("O_DRIVE")},  // 6-1
    {MUSIC("H_WATCH"),  MUSIC("O_WATCH")},  // 6-2
    {MUSIC("H_MIST"),   MUSIC("O_MIST")},   // 6-3

    {MUSIC("H_OPEN"),   MUSIC("O_OPEN")},   // MUS_TITL
    {MUSIC("H_INTER"),  MUSIC("O_INTER")},  // MUS_INTR
    {MUSIC("H_DEMO"),   MUSIC("O_DEMO")}    // MUS_CPTD
};

// Sound info

    /* Macro for original heretic sfxinfo_t 
#define SOUND(name, priority, numchannels) \
    { name, NULL, priority, -1, NULL, 0, numchannels }
#define SOUND_LINK(name, link_id, priority, numchannels) \
    { name, &S_sfx[link_id], priority, -1, NULL, 0, numchannels }
    */

#define SOUND(name, priority, numchannels) \
    { NULL, name, priority, NULL, -1, -1, -1, 0, numchannels, NULL }
#define SOUND_LINK(name, link_id, priority, numchannels) \
    { NULL, name, priority, &S_sfx[link_id], 0, 0, -1, 0, numchannels, NULL }

sfxinfo_t S_sfx[] = {
    SOUND("",        0,   0),
    SOUND("gldhit",  32,  2),
    SOUND("gntful",  32,  -1),
    SOUND("gnthit",  32,  -1),
    SOUND("gntpow",  32,  -1),
    SOUND("gntact",  32,  -1),
    SOUND("gntuse",  32,  -1),
    SOUND("phosht",  32,  2),
    SOUND("phohit",  32,  -1),
    SOUND_LINK("-phopow", sfx_hedat1, 32, 1),
    SOUND("lobsht",  20,  2),
    SOUND("lobhit",  20,  2),
    SOUND("lobpow",  20,  2),
    SOUND("hrnsht",  32,  2),
    SOUND("hrnhit",  32,  2),
    SOUND("hrnpow",  32,  2),
    SOUND("ramphit", 32,  2),
    SOUND("ramrain", 10,  2),
    SOUND("bowsht",  32,  2),
    SOUND("stfhit",  32,  2),
    SOUND("stfpow",  32,  2),
    SOUND("stfcrk",  32,  2),
    SOUND("impsit",  32,  2),
    SOUND("impat1",  32,  2),
    SOUND("impat2",  32,  2),
    SOUND("impdth",  80,  2),
    SOUND_LINK("-impact", sfx_impsit, 20, 2),
    SOUND("imppai",  32,  2),
    SOUND("mumsit",  32,  2),
    SOUND("mumat1",  32,  2),
    SOUND("mumat2",  32,  2),
    SOUND("mumdth",  80,  2),
    SOUND_LINK("-mumact", sfx_mumsit, 20, 2),
    SOUND("mumpai",  32,  2),
    SOUND("mumhed",  32,  2),
    SOUND("bstsit",  32,  2),
    SOUND("bstatk",  32,  2),
    SOUND("bstdth",  80,  2),
    SOUND("bstact",  20,  2),
    SOUND("bstpai",  32,  2),
    SOUND("clksit",  32,  2),
    SOUND("clkatk",  32,  2),
    SOUND("clkdth",  80,  2),
    SOUND("clkact",  20,  2),
    SOUND("clkpai",  32,  2),
    SOUND("snksit",  32,  2),
    SOUND("snkatk",  32,  2),
    SOUND("snkdth",  80,  2),
    SOUND("snkact",  20,  2),
    SOUND("snkpai",  32,  2),
    SOUND("kgtsit",  32,  2),
    SOUND("kgtatk",  32,  2),
    SOUND("kgtat2",  32,  2),
    SOUND("kgtdth",  80,  2),
    SOUND_LINK("-kgtact", sfx_kgtsit, 20, 2),
    SOUND("kgtpai",  32,  2),
    SOUND("wizsit",  32,  2),
    SOUND("wizatk",  32,  2),
    SOUND("wizdth",  80,  2),
    SOUND("wizact",  20,  2),
    SOUND("wizpai",  32,  2),
    SOUND("minsit",  32,  2),
    SOUND("minat1",  32,  2),
    SOUND("minat2",  32,  2),
    SOUND("minat3",  32,  2),
    SOUND("mindth",  80,  2),
    SOUND("minact",  20,  2),
    SOUND("minpai",  32,  2),
    SOUND("hedsit",  32,  2),
    SOUND("hedat1",  32,  2),
    SOUND("hedat2",  32,  2),
    SOUND("hedat3",  32,  2),
    SOUND("heddth",  80,  2),
    SOUND("hedact",  20,  2),
    SOUND("hedpai",  32,  2),
    SOUND("sorzap",  32,  2),
    SOUND("sorrise", 32,  2),
    SOUND("sorsit",  200, 2),
    SOUND("soratk",  32,  2),
    SOUND("soract",  200, 2),
    SOUND("sorpai",  200, 2),
    SOUND("sordsph", 200, 2),
    SOUND("sordexp", 200, 2),
    SOUND("sordbon", 200, 2),
    SOUND_LINK("-sbtsit", sfx_bstsit, 32, 2),
    SOUND_LINK("-sbtatk", sfx_bstatk, 32, 2),
    SOUND("sbtdth",  80,  2),
    SOUND("sbtact",  20,  2),
    SOUND("sbtpai",  32,  2),
    SOUND("plroof",  32,  2),
    SOUND("plrpai",  32,  2),
    SOUND("plrdth",  80,  2),
    SOUND("gibdth",  100, 2),
    SOUND("plrwdth", 80,  2),
    SOUND("plrcdth", 100, 2),
    SOUND("itemup",  32,  2),
    SOUND("wpnup",   32,  2),
    SOUND("telept",  50,  2),
    SOUND("doropn",  40,  2),
    SOUND("dorcls",  40,  2),
    SOUND("dormov",  40,  2),
    SOUND("artiup",  32,  2),
    SOUND("switch",  40,  2),
    SOUND("pstart",  40,  2),
    SOUND("pstop",   40,  2),
    SOUND("stnmov",  40,  2),
    SOUND("chicpai", 32,  2),
    SOUND("chicatk", 32,  2),
    SOUND("chicdth", 40,  2),
    SOUND("chicact", 32,  2),
    SOUND("chicpk1", 32,  2),
    SOUND("chicpk2", 32,  2),
    SOUND("chicpk3", 32,  2),
    SOUND("keyup",   50,  2),
    SOUND("ripslop", 16,  2),
    SOUND("newpod",  16,  -1),
    SOUND("podexp",  40,  -1),
    SOUND("bounce",  16,  2),
    SOUND_LINK("-volsht", sfx_bstatk, 16, 2),
    SOUND_LINK("-volhit", sfx_lobhit, 16, 2),
    SOUND("burn",    10,  2),
    SOUND("splash",  10,  1),
    SOUND("gloop",   10,  2),
    SOUND("respawn", 10,  1),
    SOUND("blssht",  32,  2),
    SOUND("blshit",  32,  2),
    SOUND("chat",    100, 1),
    SOUND("artiuse", 32,  1),
    SOUND("gfrag",   100, 1),
    SOUND("waterfl", 16,  2),

    // Monophonic sounds

    SOUND("wind",    16,  1),
    SOUND("amb1",    1,   1),
    SOUND("amb2",    1,   1),
    SOUND("amb3",    1,   1),
    SOUND("amb4",    1,   1),
    SOUND("amb5",    1,   1),
    SOUND("amb6",    1,   1),
    SOUND("amb7",    1,   1),
    SOUND("amb8",    1,   1),
    SOUND("amb9",    1,   1),
    SOUND("amb10",   1,   1),
    SOUND("amb11",   1,   0)
};
