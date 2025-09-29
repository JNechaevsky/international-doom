//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2024-2025 Polina "Aura" N.
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

#include <time.h>      // (uint32_t)time(NULL);
#include "m_random.h"


// LUT for Doom and Heretic
static const unsigned char rndtable_doom_heretic[256] = {
      0,   8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,  66,
     74,  21, 211,  47,  80, 242, 154,  27, 205, 128, 161,  89,  77,  36,
     95, 110,  85,  48, 212, 140, 211, 249,  22,  79, 200,  50,  28, 188,
     52, 140, 202, 120,  68, 145,  62,  70, 184, 190,  91, 197, 152, 224,
    149, 104,  25, 178, 252, 182, 202, 182, 141, 197,   4,  81, 181, 242,
    145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0,
    175, 143,  70, 239,  46, 246, 163,  53, 163, 109, 168, 135,   2, 235,
     25,  92,  20, 145, 138,  77,  69, 166,  78, 176, 173, 212, 166, 113,
     94, 161,  41,  50, 239,  49, 111, 164,  70,  60,   2,  37, 171,  75,
    136, 156,  11,  56,  42, 146, 138, 229,  73, 146,  77,  61,  98, 196,
    135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113,
     80, 250, 108,   7, 255, 237, 129, 226,  79, 107, 112, 166, 103, 241,
     24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224,
    145, 224,  81, 206, 163,  45,  63,  90, 168, 114,  59,  33, 159,  95,
     28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226,
     71,  17, 161,  93, 186,  87, 244, 138,  20,  52, 123, 251,  26,  36,
     17,  46,  52, 231, 232,  76,  31, 221,  84,  37, 216, 165, 212, 106,
    197, 242,  98,  43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136,
    120, 163, 236, 249
};

// LUT for Hexen
static const unsigned char rndtable_hexen[256] = {
    201,   1, 243,  19,  18,  42, 183, 203, 101, 123, 154, 137,  34, 118,
     10, 216, 135, 246,   0, 107, 133, 229,  35, 113, 177, 211, 110,  17,
    139,  84, 251, 235, 182, 166, 161, 230, 143,  91,  24,  81,  22,  94,
      7,  51, 232, 104, 122, 248, 175, 138, 127, 171, 222, 213,  44,  16,
      9,  33,  88, 102, 170, 150, 136, 114,  62,   3, 142, 237,   6, 252,
    249,  56,  74,  30,  13,  21, 180, 199,  32, 132, 187, 234,  78, 210,
     46, 131, 197,   8, 206, 244,  73,   4, 236, 178, 195,  70, 121,  97,
    167, 217, 103,  40, 247, 186, 105,  39,  95, 163,  99, 149, 253,  29,
    119,  83, 254,  26, 202,  65, 130, 155,  60,  64, 184, 106, 221,  93,
    164, 196, 112, 108, 179, 141,  54, 109,  11, 126,  75, 165, 191, 227,
     87, 225, 156,  15,  98, 162, 116,  79, 169, 140, 190, 205, 168, 194,
     41, 250,  27,  20,  14, 241,  50, 214,  72, 192, 220, 233,  67, 148,
     96, 185, 176, 181, 215, 207, 172,  85,  89,  90, 209, 128, 124,   2,
     55, 173,  66, 152,  47, 129,  59,  43, 159, 240, 239,  12, 189, 212,
    144,  28, 200,  77, 219, 198, 134, 228,  45,  92, 125, 151,   5,  53,
    255,  52,  68, 245, 160, 158,  61,  86,  58,  82, 117,  37, 242, 145,
     69, 188, 115,  76,  63, 100,  49, 111, 153,  80,  38,  57, 174, 224,
     71, 231,  23,  25,  48, 218, 120, 147, 208,  36, 226, 223, 193, 238,
    157, 204, 146,  31
};

// Pointer to LUT
static const uint8_t *rndtable;

int        m_rndindex  = 0;
static int p_rndindex  = 0;
static int id_rndindex = 0;

// [PN] Our private random seed value.
// Initialized once at M_InitRandom() and used only by ID_RealRandom().
static uint32_t id_rand_seed = 1;


// -----------------------------------------------------------------------------
// M_InitRandom
// -----------------------------------------------------------------------------

void M_InitRandom (GameMission_t mission)
{
    // Initialize pointer to LUT depending on the game.
    if (mission == doom || mission == heretic)
    {
        rndtable = rndtable_doom_heretic;
    }
    else
    {
        rndtable = rndtable_hexen;
    }

    // Use current time as a seed for own random generator.
    id_rand_seed = (uint32_t)time(NULL);
}

// -----------------------------------------------------------------------------
// M_Random
//  Returns a number from 0 to 255.
// -----------------------------------------------------------------------------

int M_Random (void)
{
    m_rndindex = (m_rndindex + 1) & 0xff;
    return rndtable[m_rndindex];
}

// -----------------------------------------------------------------------------
// P_Random
//  As M_Random, but used only by the play simulation.
// -----------------------------------------------------------------------------

int P_Random (void)
{
    p_rndindex = (p_rndindex + 1) & 0xff;
    return rndtable[p_rndindex];
}

// -----------------------------------------------------------------------------
// M_ClearRandom
//  Fix randoms for demos.
// -----------------------------------------------------------------------------

void M_ClearRandom (void)
{
    m_rndindex = p_rndindex = id_rndindex = 0;
}

// -----------------------------------------------------------------------------
// P_SubRandom
//  Defined version of P_Random() - P_Random().
//  Inspired by the same routine in Eternity, thanks haleyjd.
// -----------------------------------------------------------------------------

int P_SubRandom (void)
{
    const int r = P_Random();
    return r - P_Random();
}

// -----------------------------------------------------------------------------
// Our own random functions.
// -----------------------------------------------------------------------------

int ID_Random (void)
{
    id_rndindex = (id_rndindex + 1) & 0xff;
    return rndtable[id_rndindex];
}

int ID_SubRandom (void)
{
    const int r = ID_Random();
    return r - ID_Random();
}

// -----------------------------------------------------------------------------
// ID_RealRandom
//  [PN] Lightweight local random number generator.
//  A private, self-contained linear congruential generator.
//  Mirrors the algorithm historically used in many C runtimes,
//  but keeps the state locally and avoids thread-local storage
//  or other runtime overhead. Faster than the standard rand() in tight loops.
//  Returns [0...32767]. May return [0...255] as well by using >> 23 shift.
// -----------------------------------------------------------------------------

int ID_RealRandom (void)
{
    return (id_rand_seed = id_rand_seed * 214013u + 2531011u) >> 17;
}
