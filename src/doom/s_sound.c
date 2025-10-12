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
// DESCRIPTION:  none
//

#include <stdio.h>
#include <stdlib.h>

#include "i_sound.h"
#include "i_system.h"

#include "d_main.h"
#include "deh_str.h"

#include "doomstat.h"
#include "doomtype.h"

#include "sounds.h"
#include "s_sound.h"

#include "m_misc.h"
#include "m_random.h"
#include "m_argv.h"

#include "p_local.h"
#include "w_wad.h"
#include "z_zone.h"
#include "g_game.h"  // [JN] demo_gotonextlvl

#include "id_vars.h"
#include "id_func.h"

// when to clip out sounds
// Does not fit the large outdoor areas.

#define S_CLIPPING_DIST (1200 * FRACUNIT)

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// In the source code release: (160*FRACUNIT).  Changed back to the
// Vanilla value of 200 (why was this changed?)

#define S_CLOSE_DIST (200 * FRACUNIT)

// The range over which sound attenuates

#define S_ATTENUATOR ((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)

// Stereo separation

#define S_STEREO_SWING (96 * FRACUNIT)
static int stereo_swing;

#define NORM_PRIORITY 64
#define NORM_SEP 128

typedef struct
{
    // sound information (if null, channel avail.)
    sfxinfo_t *sfxinfo;

    // origin of sound
    mobj_t *origin;

    // handle of the sound being played
    int handle;

    int pitch;

} channel_t;

// The set of channels available

static channel_t *channels;

// Maximum volume of a sound effect.
// Internal default is max out of 0-15.

int sfxVolume = 8;

// Maximum volume of music.

int musicVolume = 8;

// Internal volume level, ranging from 0-127

static int snd_SfxVolume;

// Whether songs are mus_paused

static boolean mus_paused;

// Music currently being played

static musicinfo_t *mus_playing = NULL;

// [JN] Always allocate 8 SFX channels.
// No memory reallocation will be needed upon changing of channels number.

#define MAX_SND_CHANNELS 16

// [JN] External music number, used for music playback hot-swapping.
int current_mus_num;

// [JN] jff 3/17/98 to keep track of last IDMUS specified music num
int idmusnum;

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void S_Init(int sfxVolume, int musicVolume)
{
    int i;

    idmusnum = -1; // [JN] jff 3/17/98 insure idmus number is blank

    if (gameversion == exe_doom_1_666)
    {
        if (logical_gamemission == doom)
        {
            I_SetOPLDriverVer(opl_doom1_1_666);
        }
        else
        {
            I_SetOPLDriverVer(opl_doom2_1_666);
        }
    }
    else
    {
        I_SetOPLDriverVer(opl_doom_1_9);
    }

    I_PrecacheSounds(S_sfx, NUMSFX);

    S_SetSfxVolume(sfxVolume);
    S_SetMusicVolume(musicVolume);

    // Allocating the internal channels for mixing
    // (the maximum numer of sounds rendered
    // simultaneously) within zone memory.
    channels = Z_Malloc(MAX_SND_CHANNELS*sizeof(channel_t), PU_STATIC, 0);

    // Free all channels for use
    for (i=0 ; i<MAX_SND_CHANNELS ; i++)
    {
        channels[i].sfxinfo = 0;
    }

    // no sounds are playing, and they are not mus_paused
    mus_paused = 0;

    // Note that sounds have not been cached (yet).
    for (i=1 ; i<NUMSFX ; i++)
    {
        S_sfx[i].lumpnum = S_sfx[i].usefulness = -1;
    }

    // Doom defaults to pitch-shifting off.
    if (snd_pitchshift == -1)
    {
        snd_pitchshift = 0;
    }

    I_AtExit(S_Shutdown, true);

    // [crispy] initialize dedicated music tracks for the 4th episode
    for (i = mus_e4m1; i <= mus_e6m9; i++)
    {
        musicinfo_t *const music = &S_music[i];
        char namebuf[9];

        M_snprintf(namebuf, sizeof(namebuf), "d_%s", DEH_String(music->name));
        music->lumpnum = W_CheckNumForName(namebuf);
    }

    // [crispy] handle stereo separation for mono-sfx and flipped levels
    S_UpdateStereoSeparation();
}

// -----------------------------------------------------------------------------
// S_ChangeSFXSystem
// [JN] Routine for sfx device hot-swapping.
// -----------------------------------------------------------------------------

void S_ChangeSFXSystem (void)
{
    int i;

    // Free all channels for use
    for (i = 0 ; i < MAX_SND_CHANNELS ; i++)
    {
        channels[i].sfxinfo = 0;
    }

    // Reinitialize sfx usefulness
    for (i = 1 ; i < NUMSFX ; i++)
    {
        S_sfx[i].lumpnum = S_sfx[i].usefulness = -1;
    }
}

// -----------------------------------------------------------------------------
// S_UpdateStereoSeparation
// [JN] Defines stereo separtion for mono sfx mode and flipped levels.
// -----------------------------------------------------------------------------

void S_UpdateStereoSeparation (void)
{
	// [crispy] play all sound effects in mono
	if (snd_monosfx)
	{
		stereo_swing = 0;
	}
	else if (gp_flip_levels)
	{
		stereo_swing = -S_STEREO_SWING;
	}
	else
	{
		stereo_swing = S_STEREO_SWING;
	}
}

void S_Shutdown(void)
{
    I_ShutdownSound();
    I_ShutdownMusic();
}

static void S_StopChannel(int cnum)
{
    channel_t *const c = &channels[cnum];

    if (c->sfxinfo)
    {
        // stop the sound playing

        if (I_SoundIsPlaying(c->handle))
        {
            I_StopSound(c->handle);
        }

        // check to see if other channels are playing the sound

        for (int i=0; i<snd_channels; i++)
        {
            if (cnum != i && c->sfxinfo == channels[i].sfxinfo)
            {
                break;
            }
        }

        // degrade usefulness of sound data

        c->sfxinfo->usefulness--;
        c->sfxinfo = NULL;
        c->origin = NULL;
    }
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//

void S_Start(void)
{
    int cnum;
    int mnum;

    // kill all playing sounds at start of level
    //  (trust me - a good idea)
    for (cnum=0 ; cnum<snd_channels ; cnum++)
    {
        if (channels[cnum].sfxinfo)
        {
            S_StopChannel(cnum);
        }
    }

    // start new music for the level
    if (musicVolume) // [crispy] do not reset pause state at zero music volume
    mus_paused = 0;

    if (idmusnum != -1)
    {
        mnum = idmusnum; //jff 3/17/98 reload IDMUS music if not -1
    }
    else
    if (gamemode == commercial)
    {
      if (remaster_ost)
      {
          mnum = S_ID_Set_D2_RemasteredMusic();
      }
      else
      {
        if (nerve && gamemap <= 9)
        {
            const int nmus[] =
            {
                mus_messag,
                mus_ddtblu,
                mus_doom,
                mus_shawn,
                mus_in_cit,
                mus_the_da,
                mus_in_cit,
                mus_shawn2,
                mus_ddtbl2,
            };

            mnum = nmus[gamemap - 1];
        }
        else
        {
            mnum = mus_runnin + gamemap - 1;
        }
      }
    }
    else
    {
        if (remaster_ost)
        {
            mnum = S_ID_Set_D1_RemasteredMusic();
        }
        else
        {
        const int spmus[]=
        {
            // Song - Who? - Where?

            mus_e3m4,        // American     e4m1
            mus_e3m2,        // Romero       e4m2
            mus_e3m3,        // Shawn        e4m3
            mus_e1m5,        // American     e4m4
            mus_e2m7,        // Tim          e4m5
            mus_e2m4,        // Romero       e4m6
            mus_e2m6,        // J.Anderson   e4m7 CHIRON.WAD
            mus_e2m5,        // Shawn        e4m8
            mus_e1m9,        // Tim          e4m9
        };

        if (gameepisode < 4 || gameepisode == 5 || gameepisode == 6) // [crispy] 
        {
            mnum = mus_e1m1 + (gameepisode-1)*9 + gamemap-1;
        }
        else
        {
            mnum = spmus[gamemap-1];

            // [crispy] support dedicated music tracks for the 4th episode
            {
                const int sp_mnum = mus_e1m1 + 3 * 9 + gamemap - 1;

                if (S_music[sp_mnum].lumpnum > 0)
                {
                    mnum = sp_mnum;
                }
            }
        }
        }
    }

    S_ChangeMusic(mnum, true);
}

void S_StopSound(const mobj_t *origin)
{
    int cnum;

    for (cnum=0 ; cnum<snd_channels ; cnum++)
    {
        if (channels[cnum].sfxinfo && channels[cnum].origin == origin)
        {
            S_StopChannel(cnum);
            break;
        }
    }
}

//
// S_GetChannel :
//   If none available, return -1.  Otherwise channel #.
//

static int S_GetChannel(mobj_t *origin, sfxinfo_t *sfxinfo)
{
    // channel number to use
    int                cnum;

    channel_t*        c;

    // Find an open channel
    for (cnum=0 ; cnum<snd_channels ; cnum++)
    {
        if (!channels[cnum].sfxinfo)
        {
            break;
        }
        else if (origin && channels[cnum].origin == origin)
        {
            S_StopChannel(cnum);
            break;
        }
    }

    // None available
    if (cnum == snd_channels)
    {
        // Look for lower priority
        for (cnum=0 ; cnum<snd_channels ; cnum++)
        {
            if (channels[cnum].sfxinfo->priority >= sfxinfo->priority)
            {
                break;
            }
        }

        if (cnum == snd_channels)
        {
            // FUCK!  No lower priority.  Sorry, Charlie.
            return -1;
        }
        else
        {
            // Otherwise, kick out lower priority.
            S_StopChannel(cnum);
        }
    }

    c = &channels[cnum];

    // channel is decided to be cnum.
    c->sfxinfo = sfxinfo;
    c->origin = origin;

    return cnum;
}

// -----------------------------------------------------------------------------
// P_ApproxDistanceZ
// [JN] Gives an estimation of distance using three axises.
// Adapted from EDGE, converted to fixed point math.
// [PN] Optimized with bitwise shifts (>> 1) for performance.
// Precise results can be obtained with sqrt, but it's computationally expensive:
//   return sqrt(dx * dx + dy * dy + dz * dz);
// -----------------------------------------------------------------------------

static int64_t S_ApproxDistanceZ (int64_t dx, int64_t dy, int64_t dz)
{
	int64_t dxy;

	dx = llabs(dx);
	dy = llabs(dy);
	dz = llabs(dz);

	dxy = (dy > dx) ? dy + (dx >> 1) : dx + (dy >> 1);

	return (dz > dxy) ? dz + (dxy >> 1) : dxy + (dz >> 1);
}

//
// Changes volume and stereo-separation variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//

static int S_AdjustSoundParams(mobj_t *listener, mobj_t *source,
                               int *vol, int *sep)
{
    int64_t        approx_dist;
    int64_t        adx;
    int64_t        ady;
    int64_t        adz; // [JN] Z-axis sfx distance
    angle_t        angle;

    int64_t        listener_x;
    int64_t        listener_y;
    int64_t        listener_z;
    angle_t        listener_ang;

    // [crispy] proper sound clipping in Doom 2 MAP08 and The Ultimate Doom E4M8 / Sigil E5M8
    const boolean doom1map8 = (gamemap == 8 && ((gamemode != commercial && gameepisode < 4)));

    if (!crl_spectating)
    {
        listener_x   = listener->x;
        listener_y   = listener->y;
        listener_z   = listener->z;
        listener_ang = listener->angle;
    }
    else
    {
        listener_x   = CRL_camera_x;
        listener_y   = CRL_camera_y;
        listener_z   = CRL_camera_z;
        listener_ang = CRL_camera_ang;
    }

    // calculate the distance to sound origin
    //  and clip it if necessary
    adx = llabs(listener_x - (int64_t)source->x);
    ady = llabs(listener_y - (int64_t)source->y);
    adz = llabs(listener_z - (int64_t)source->z);

    if (aud_z_axis_sfx)
    {
        // [JN] Use better precision for distance calculation.
        approx_dist = S_ApproxDistanceZ(adx, ady, adz);
    }
    else
    {
        // From _GG1_ p.428. Appox. eucledian distance fast.
        approx_dist = adx + ady - ((adx < ady ? adx : ady)>>1);
    }

    if (!doom1map8 && approx_dist > S_CLIPPING_DIST)
    {
        return 0;
    }

    // angle of source to listener
    angle = R_PointToAngle2(listener_x,
                            listener_y,
                            source->x,
                            source->y);

    if (angle > listener_ang)
    {
        angle = angle - listener_ang;
    }
    else
    {
        angle = angle + (0xffffffff - listener_ang);
    }

    angle >>= ANGLETOFINESHIFT;

    // stereo separation
    *sep = 128 - (FixedMul(stereo_swing, finesine[angle]) >> FRACBITS);

    // volume calculation
    if (approx_dist < S_CLOSE_DIST)
    {
        *vol = snd_SfxVolume;
    }
    else if (doom1map8)
    {
        if (approx_dist > S_CLIPPING_DIST)
        {
            approx_dist = S_CLIPPING_DIST;
        }

        *vol = 15+ ((snd_SfxVolume-15)
                    *((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
            / S_ATTENUATOR;
    }
    else
    {
        // distance effect
        *vol = (snd_SfxVolume
                * ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
            / S_ATTENUATOR;
    }

    return (*vol > 0);
}

// clamp supplied integer to the range 0 <= x <= 255.

static int Clamp(int x)
{
    if (x < 0)
    {
        return 0;
    }
    else if (x > 255)
    {
        return 255;
    }
    return x;
}

void S_StartSound(void *origin_p, int sfx_id)
{
    sfxinfo_t *sfx;
    mobj_t *origin;
    int sep;
    int pitch;
    int cnum;
    int volume;

    // [JN] Do not play sound while demo-warp.
    if (nodrawers || demowarp || !snd_SfxVolume)
    {
        return;
    }

    origin = (mobj_t *) origin_p;
    volume = snd_SfxVolume;

    // check for bogus sound #
    if (sfx_id < 1 || sfx_id > NUMSFX)
    {
        I_Error("Bad sfx #: %d", sfx_id);
    }

    sfx = &S_sfx[sfx_id];

    // Initialize sound parameters
    pitch = NORM_PITCH;
    if (sfx->link)
    {
        volume += sfx->volume;
        pitch = sfx->pitch;

        if (volume < 1)
        {
            return;
        }

        if (volume > snd_SfxVolume)
        {
            volume = snd_SfxVolume;
        }
    }


    // Check to see if it is audible,
    //  and if not, modify the params
    // [PN] In spectating mode we DO NOT treat displayplayer sounds as local:
    // everything should be positioned in world at the real player's coords.
    if (origin)
    {
        const boolean force_local = (!crl_spectating)
            && (origin == players[displayplayer].mo || origin == players[displayplayer].so); // [crispy] weapon sound source

        if (!force_local)
        {
            const int rc = S_AdjustSoundParams(players[displayplayer].mo, origin, &volume, &sep);
            // Only center-separate when NOT spectating.
            if (!crl_spectating
            &&  origin->x == players[displayplayer].mo->x
            &&  origin->y == players[displayplayer].mo->y)
            {
                sep = NORM_SEP;
            }
            if (!rc)
            {
                return;
            }
        }
        else
        {
            sep = NORM_SEP;
        }
    }
    else
    {
        sep = NORM_SEP;
    }

    // hacks to vary the sfx pitches
    if (sfx_id >= sfx_sawup && sfx_id <= sfx_sawhit)
    {
        pitch += 8 - (M_Random()&15);
    }
    else if (sfx_id != sfx_itemup && sfx_id != sfx_tink)
    {
        pitch += 16 - (M_Random()&31);
    }
    pitch = Clamp(pitch);

    // kill old sound
    S_StopSound(origin);

    // try to find a channel
    cnum = S_GetChannel(origin, sfx);

    if (cnum < 0)
    {
        return;
    }

    // increase the usefulness
    if (sfx->usefulness++ < 0)
    {
        sfx->usefulness = 1;
    }

    if (sfx->lumpnum < 0)
    {
        sfx->lumpnum = I_GetSfxLumpNum(sfx);
    }

    channels[cnum].pitch = pitch;
    channels[cnum].handle = I_StartSound(sfx, cnum, volume, sep, channels[cnum].pitch);
}

void S_StartSoundOnce (void *origin_p, int sfx_id)
{
    int cnum;
    const sfxinfo_t *const sfx = &S_sfx[sfx_id];

    for (cnum = 0; cnum < snd_channels; cnum++)
    {
        if (channels[cnum].sfxinfo == sfx &&
            channels[cnum].origin == origin_p)
        {
            return;
        }
    }

    S_StartSound(origin_p, sfx_id);
}

//
// Stop and resume music, during game PAUSE.
//

void S_PauseSound(void)
{
    if (mus_playing && !mus_paused)
    {
        I_PauseSong();
        mus_paused = true;
    }
}

void S_ResumeSound(void)
{
    if (mus_playing && mus_paused)
    {
        I_ResumeSong();
        mus_paused = false;
    }
}

//
// Updates music & sounds
//

void S_UpdateSounds(mobj_t *listener)
{
    int                audible;
    int                cnum;
    int                volume;
    int                sep;
    const sfxinfo_t   *sfx;
    const channel_t   *c;

    I_UpdateSound();

    for (cnum=0; cnum<snd_channels; cnum++)
    {
        c = &channels[cnum];
        sfx = c->sfxinfo;

        if (c->sfxinfo)
        {
            if (I_SoundIsPlaying(c->handle))
            {
                // initialize parameters
                volume = snd_SfxVolume;
                sep = NORM_SEP;

                if (sfx->link)
                {
                    volume += sfx->volume;
                    if (volume < 1)
                    {
                        S_StopChannel(cnum);
                        continue;
                    }
                    else if (volume > snd_SfxVolume)
                    {
                        volume = snd_SfxVolume;
                    }
                }

                // check non-local sounds for distance clipping
                //  or modify their params
                // [PN] In spectating mode the displayplayer's own sounds are NOT local.
                if (c->origin)
                {
                    boolean treat_local = (!crl_spectating)
                        && ((listener == c->origin) || (c->origin == players[displayplayer].so)); // [crispy] weapon sound source

                    if (!treat_local)
                    {
                        audible = S_AdjustSoundParams(listener, c->origin, &volume, &sep);

                        if (!audible)
                        {
                            S_StopChannel(cnum);
                        }
                        else
                        {
                            I_UpdateSoundParams(c->handle, volume, sep);
                        }
                    }
                }
            }
            else
            {
                // if channel is allocated but sound has stopped,
                //  free it
                S_StopChannel(cnum);
            }
        }
    }
}

void S_SetMusicVolume(int volume)
{
    if (volume < 0 || volume > 127)
    {
        I_Error("Attempt to set music volume at %d",
                volume);
    }

    // [crispy] [JN] Fixed bug when music was hearable with zero volume
    if (!musicVolume)
    {
        S_PauseSound();
    }
    else
    if (!paused)
    {
        S_ResumeSound();
    }

    I_SetMusicVolume(volume);
}

void S_SetSfxVolume(int volume)
{
    if (volume < 0 || volume > 127)
    {
        I_Error("Attempt to set sfx volume at %d", volume);
    }

    snd_SfxVolume = volume;
}

//
// Starts some music with the music id found in sounds.h.
//

void S_StartMusic(int m_id)
{
    S_ChangeMusic(m_id, false);
}

void S_ChangeMusic(int musicnum, int looping)
{
    musicinfo_t *music = NULL;
    char namebuf[9];
    void *handle;

    // [JN] Do not play music while demo-warp,
    // but still change while fast forwarding to next level in demo playback.
    if ((nodrawers || demowarp) && !demo_gotonextlvl)
    {
        return;
    }

    // The Doom IWAD file has two versions of the intro music: d_intro
    // and d_introa.  The latter is used for OPL playback.

    if (musicnum == mus_intro && (snd_musicdevice == SNDDEVICE_ADLIB
                               || snd_musicdevice == SNDDEVICE_SB)
                               && !remaster_ost) // [JN] Remastered soundtrack doesn't have it.
    {
        musicnum = mus_introa;
    }

    if (musicnum <= mus_None || musicnum >= NUMMUSIC)
    {
        I_Error("Bad music number %d", musicnum);
    }
    else
    {
        music = &S_music[musicnum];
    }

    if (mus_playing == music)
    {
        return;
    }

    // [JN] After inner muscial number has been set, sync it with
    // external number, used in M_ID_MusicSystem.
    current_mus_num = musicnum;

    // shutdown old music
    S_StopMusic();

    if (remaster_ost && gameepisode < 5)
    {
        // [PN/JN] Generate the music lump name.
        S_ID_Generate_MusicName(namebuf, sizeof(namebuf), music);
        music->lumpnum = W_GetNumForName(namebuf);
    }
    else
    {
    // get lumpnum if neccessary
    if (!music->lumpnum)
    {
        M_snprintf(namebuf, sizeof(namebuf), "d_%s", DEH_String(music->name));
        music->lumpnum = W_GetNumForName(namebuf);
    }
    }

    music->data = W_CacheLumpNum(music->lumpnum, PU_STATIC);

    handle = I_RegisterSong(music->data, W_LumpLength(music->lumpnum));
    music->handle = handle;
    I_PlaySong(handle, looping);

    mus_playing = music;
}

void S_StopMusic(void)
{
    if (mus_playing)
    {
        if (mus_paused)
        {
            I_ResumeSong();
        }

        I_StopSong();
        I_UnRegisterSong(mus_playing->handle);
        W_ReleaseLumpNum(mus_playing->lumpnum);
        mus_playing->data = NULL;
        mus_playing = NULL;
    }
}

// -----------------------------------------------------------------------------
// S_MuteUnmuteSound
// [JN] Sets sfx and music volume to 0 when window loses 
//      it's focus and restores back when focus is regained.
// -----------------------------------------------------------------------------

void S_MuteUnmuteSound (boolean mute)
{
    if (mute)
    {
        // Stop all sounds and clear sfx channels.
        for (int i = 0 ; i < snd_channels ; i++)
        {
            S_StopChannel(i);
        }

        // Set volume variables to zero.
        S_SetMusicVolume(0);
        S_SetSfxVolume(0);
    }
    else
    {
        S_SetMusicVolume(musicVolume * 8);
        S_SetSfxVolume(sfxVolume * 8);
    }

    // All done, no need to invoke function until next 
    // minimizing/restoring of game window is happened.
    volume_needs_update = false;
}

// -----------------------------------------------------------------------------
// [JN] Remastered soundtrack (extras.wad) functions.
// -----------------------------------------------------------------------------

// [JN] Handles Doom 1 title music.
void S_ID_Change_D1_IntermissionMusic (void)
{
    const int id_mus_inter =
        remaster_ost ? mus_e2m3 : mus_inter;

    S_ChangeMusic(id_mus_inter, true); 
}

// [JN] Handles Doom 2 title music.
void S_ID_Start_D2_TitleMusic (void)
{
    const int id_mus_dm2ttl =
        (remaster_ost && snd_remaster_ost && logical_gamemission == pack_tnt) ? mus_tntttl : mus_dm2ttl;

    S_StartMusic(id_mus_dm2ttl);
}

// [JN] Handles Doom 2 tally screen music.
void S_ID_Change_D2_IntermissionMusic (void)
{
    const int id_mus_dm2int =
        (remaster_ost && snd_remaster_ost && logical_gamemission == pack_tnt) ? mus_tnt31 : mus_dm2int;

    S_ChangeMusic(id_mus_dm2int, true);
}

// [JN] Handles Doom 2 intermission text music.
void S_ID_Change_D2_ReadMusic (void)
{
    const int id_mus_read_m =
        (remaster_ost && snd_remaster_ost && logical_gamemission == pack_tnt) ? mus_tntred : mus_read_m;
    
    S_ChangeMusic(id_mus_read_m, true);
}

// [JN] Handles Doom 2 casting screen music.
void S_ID_Change_D2_CastMusic (void)
{
    const int id_mus_evil =
        (remaster_ost && snd_remaster_ost && logical_gamemission == pack_tnt) ? mus_tnt31 :
        (remaster_ost && logical_gamemission == pack_plut) ? mus_e1m8 : mus_evil;

    S_ChangeMusic(id_mus_evil, true);
}

// [JN/PN] Handles Doom 1 music selection, ensuring correct remastered
// or default track assignment.
int S_ID_Set_D1_RemasteredMusic (void)
{
    int mnum = 0;

    static const int remaster_d1[][11] =
    {
        { mus_intro, 0, mus_e1m1, mus_e1m2, mus_e1m3, mus_e1m4, mus_e1m5, mus_e1m6, mus_e1m7, mus_e1m8, mus_e1m9 },
        { mus_intro, 0, mus_e2m1, mus_e2m2, mus_e2m3, mus_e2m4, mus_e1m7, mus_e2m6, mus_e2m7, mus_e2m8, mus_e2m9 },
        { mus_intro, 0, mus_e2m9, mus_e3m2, mus_e3m3, mus_e1m8, mus_e1m7, mus_e1m6, mus_e2m7, mus_e3m8, mus_e1m9 },
        { mus_intro, 0, mus_e1m8, mus_e3m2, mus_e3m3, mus_e1m5, mus_e2m7, mus_e2m4, mus_e2m6, mus_e1m7, mus_e1m9 },
        { mus_intro, 0, mus_e5m1, mus_e5m2, mus_e5m3, mus_e5m5, mus_e5m5, mus_e5m6, mus_e5m7, mus_e5m8, mus_e5m9 },
        { mus_intro, 0, mus_e6m1, mus_e6m2, mus_e6m3, mus_e6m5, mus_e6m5, mus_e6m6, mus_e6m7, mus_e6m8, mus_e6m9 },
    };

    switch (gamestate)
    {
        case GS_DEMOSCREEN:    mnum = mus_intro;  break;
        case GS_INTERMISSION:  mnum = snd_remaster_ost ? mus_e2m3 : mus_inter; break;
        case GS_FINALE:        mnum = mus_victor; break;
        case GS_THEEND:        mnum = mus_bunny;  break;
        default:
            mnum = remaster_d1[gameepisode - 1][gamemap + 1];
            break;
    }

    return mnum;
}

// [JN/PN] Handles Doom 2 music selection for different game missions and states,
// ensuring correct remastered or default track assignment.
int S_ID_Set_D2_RemasteredMusic (void)
{
    int mnum = 0;

    if (gamemission == doom2)
    {
        static const int remaster_d2[32] =
        {
            mus_runnin, mus_stalks, mus_countd, mus_betwee, mus_doom, mus_the_da, mus_shawn, mus_ddtblu, mus_in_cit, mus_dead,
            mus_stalks, mus_the_da, mus_doom, mus_ddtblu, mus_runnin, mus_dead, mus_stalks, mus_romero, mus_shawn, mus_messag,
            mus_countd, mus_ddtblu, mus_ampie, mus_the_da, mus_adrian, mus_messag, mus_romero, mus_tense, mus_shawn, mus_openin,
            mus_evil, mus_ultima
        };
        static const int remaster_nerve[9] =
        {
            // Nerve.wad - use same track/lump names for remastered music.
            mus_messag, mus_ddtblu, mus_doom, mus_shawn, mus_in_cit,
            mus_the_da, mus_in_cit, mus_shawn, mus_ddtblu,
        };
        static const int original_nerve[9] =
        {
            // Nerve.wad - use unique track/lump names for original music,
            // in case of possible usage of music pack.
            mus_messag, mus_ddtblu, mus_doom, mus_shawn, mus_in_cit,
            mus_the_da, mus_in_cit, mus_shawn2, mus_ddtbl2,
        };

        switch (gamestate)
        {
            case GS_DEMOSCREEN:    mnum = mus_dm2ttl; break;
            case GS_INTERMISSION:  mnum = mus_dm2int; break;
            case GS_FINALE:        mnum = mus_read_m; break;
            case GS_THEEND:        mnum = mus_evil;   break;
            default:
                if (nerve && gamemap <= 9)
                {
                    mnum = snd_remaster_ost ? remaster_nerve[gamemap - 1] : original_nerve[gamemap - 1];
                }
                else
                {
                    mnum = snd_remaster_ost ? remaster_d2[gamemap - 1] : mus_runnin + gamemap - 1;
                }
                break;
        }
    }
    else if (gamemission == pack_tnt)
    {
        static const int remaster_tnt[32] =
        {
            mus_tnt01, mus_tnt02, mus_messag, mus_tnt04, mus_tnt05, mus_tnt06, mus_tnt07, mus_tnt08, mus_tnt01, mus_tnt10,
            mus_tnt11, mus_ddtblu, mus_tnt04, mus_tnt14, mus_tnt02, mus_tnt16, mus_tnt05, mus_tnt10, mus_countd, mus_tnt20,
            mus_in_cit, mus_tnt22, mus_ampie, mus_betwee, mus_doom, mus_tnt16, mus_tnt08, mus_tnt22, mus_tnt04, mus_tnt08,
            mus_tnt31, mus_in_cit
        };

        switch (gamestate)
        {
            case GS_DEMOSCREEN:    mnum = snd_remaster_ost ? mus_tntttl : mus_dm2ttl; break;
            case GS_INTERMISSION:  mnum = snd_remaster_ost ? mus_tnt31  : mus_evil;   break;
            case GS_FINALE:        mnum = snd_remaster_ost ? mus_tntred : mus_read_m; break;
            case GS_THEEND:        mnum = snd_remaster_ost ? mus_tnt31  : mus_evil;   break;
            default:
                mnum = snd_remaster_ost ? remaster_tnt[gamemap - 1] : mus_runnin + (gamemap - 1);
                break;
        }
    }
    else if (gamemission == pack_plut)
    {
        static const int remaster_plut[32] =
        {
            mus_e1m2, mus_e1m3, mus_e1m6, mus_e1m4, mus_e1m9, mus_e1m8, mus_e2m1, mus_e2m2, mus_e3m3, mus_e1m7,
            mus_bunny, mus_e3m8, mus_e3m2, mus_e2m8, mus_e2m7, mus_e2m9, mus_e1m1, mus_e1m7, mus_e1m5, mus_messag,
            mus_read_m, mus_ddtblu, mus_ampie, mus_the_da, mus_adrian, mus_messag, mus_e2m1, mus_e2m2, mus_e1m1, mus_victor,
            mus_e1m8, mus_e2m8
        };

        switch (gamestate)
        {
            case GS_DEMOSCREEN:    mnum = mus_dm2ttl; break;
            case GS_INTERMISSION:  mnum = mus_dm2int; break;
            case GS_FINALE:        mnum = mus_read_m; break;
            case GS_THEEND:        mnum = snd_remaster_ost ? mus_e1m8 : mus_evil; break;
            default:
                mnum = snd_remaster_ost ? remaster_plut[gamemap - 1] : mus_runnin + (gamemap - 1);
                break;
        }
    }
    
    return mnum;
}

// [PN/JN] Determines the correct music prefix based on game mission and map.
void S_ID_Generate_MusicName (char *namebuf, size_t bufsize, musicinfo_t *music)
{
    const char *prefix;

    if (gamemission == pack_tnt)
    {
        boolean special = (gamemap == 3 || gamemap == 12 || gamemap == 19 ||
                          gamemap == 21 || gamemap == 23 || gamemap == 24 ||
                          gamemap == 25 || gamemap == 32);

        if (special)
        {
            prefix = (snd_remaster_ost == 1) ? "h_%s" :
                     (snd_remaster_ost == 2) ? "o_%s" : "d_%s";
        }
        else
        {
            prefix = snd_remaster_ost ? "o_%s" : "d_%s";
        }
    }
    else
    {
        prefix = (snd_remaster_ost == 1) ? "h_%s" :
                 (snd_remaster_ost == 2) ? "o_%s" : "d_%s";
    }
    
    M_snprintf(namebuf, bufsize, prefix, DEH_String(music->name));
}
