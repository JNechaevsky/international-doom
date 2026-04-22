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
//      Timer functions.
//

#include "SDL.h"

#include "i_timer.h"
#include "m_fixed.h" // [crispy]
#include "doomtype.h"

//
// I_GetTime
// returns time in 1/35th second tics
//

static Uint32 basetime = 0;
static uint64_t basecounter = 0; // [crispy]
static uint64_t basecounter_scaled = 0; // [PN]
static uint64_t basefreq = 0; // [crispy]
static int time_scale = 100; // [PN]

static uint64_t GetScaledPerfCounter(void)
{
    uint64_t counter;

    if (basefreq == 0)
        basefreq = SDL_GetPerformanceFrequency();

    counter = SDL_GetPerformanceCounter() * (uint64_t)time_scale / 100;

    if (basecounter_scaled == 0)
        basecounter_scaled = counter;

    return counter - basecounter_scaled;
}

static int I_GetTimeMSScaled(void)
{
    return (GetScaledPerfCounter() * 1000ull) / basefreq;
}

int I_GetTime (void)
{
    return (I_GetTimeMSScaled() * TICRATE) / 1000;
}

//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void)
{
    Uint32 ticks;

    ticks = SDL_GetTicks();

    if (basetime == 0)
        basetime = ticks;

    return ticks - basetime;
}

// [crispy] Get time in microseconds

uint64_t I_GetTimeUS(void)
{
    uint64_t counter;

    counter = SDL_GetPerformanceCounter();

    if (basecounter == 0)
        basecounter = counter;

    return ((counter - basecounter) * 1000000ull) / basefreq;
}

// Sleep for a specified number of ms

void I_Sleep(int ms)
{
    SDL_Delay(ms);
}

void I_WaitVBL(int count)
{
    I_Sleep((count * 1000) / 70);
}


void I_InitTimer(void)
{
    // initialize timer

    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "1");

    SDL_Init(SDL_INIT_TIMER);

    basefreq = SDL_GetPerformanceFrequency(); // [crispy]
}

// [PN] Adjust the game clock while preserving its current elapsed value.
void I_SetTimeScale(int scale)
{
    uint64_t counter;

    if (scale < 3)
        scale = 3;
    else if (scale > 10000)
        scale = 10000;

    counter = GetScaledPerfCounter();
    time_scale = scale;
    basecounter_scaled += GetScaledPerfCounter() - counter;
}

// [crispy]

fixed_t I_GetFracRealTime(void)
{
    return (int64_t)I_GetTimeMSScaled() * TICRATE % 1000 * FRACUNIT / 1000;
}
