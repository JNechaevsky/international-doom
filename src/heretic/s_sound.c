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

#include <stdlib.h>

#include "doomdef.h"
#include "i_system.h"
#include "m_random.h"
#include "sounds.h"
#include "s_sound.h"
#include "i_sound.h"
#include "r_local.h"
#include "p_local.h"

#include "sounds.h"

#include "w_wad.h"
#include "z_zone.h"

#include "id_vars.h"

/*
===============================================================================

		MUSIC & SFX API

===============================================================================
*/

void S_ShutDown(void);
boolean S_StopSoundID(int sound_id, int priority);

static channel_t channel[MAX_CHANNELS];

static void *rs;          // Handle for the registered song
int mus_song = -1;
int mus_lumpnum;
void *mus_sndptr;
byte *soundCurve;

// [JN] jff 3/17/98 to keep track of last IDMUS specified music num
int idmusnum;

int snd_MaxVolume = 10;
int snd_MusicVolume = 10;

int AmbChan;

void S_Start(void)
{
    int i;

    // [JN] If music was chosen by cheat code, play it.
    if (idmusnum != -1)
    {
        S_StartSong(idmusnum, true);
    }
    else
    {
        S_StartSong((gameepisode - 1) * 9 + gamemap - 1, true);
    }

    //stop all sounds
    for (i = 0; i < snd_channels; i++)
    {
        if (channel[i].handle)
        {
            S_StopSound(channel[i].mo);
        }
    }
    memset(channel, 0, snd_channels * sizeof(channel_t));
}

void S_StartSong(int song, boolean loop)
{
    int mus_len;

    // [JN] Do not play music while demo-warp,
    // but still change while fast forwarding to next level in demo playback.
    if ((nodrawers || demowarp) && !demo_gotonextlvl)
    {
        return;
    }

    if (song == mus_song)
    {                           // don't replay an old song
        return;
    }

    if (rs != NULL)
    {
        I_StopSong();
        I_UnRegisterSong(rs);
        rs = NULL;
        mus_song = -1;
    }

    if (song < mus_e1m1 || song > NUMMUSIC)
    {
        return;
    }
    // [crispy] support dedicated music tracks for each map
    if (S_music[song][1].name && W_CheckNumForName(S_music[song][1].name) > 0)
    {
        mus_lumpnum = (W_GetNumForName(S_music[song][1].name));
    }
    else
    {
        mus_lumpnum = (W_GetNumForName(S_music[song][0].name));
    }
    mus_sndptr = W_CacheLumpNum(mus_lumpnum, PU_MUSIC);
    mus_len = W_LumpLength(mus_lumpnum);
    rs = I_RegisterSong(mus_sndptr, mus_len);
    I_PlaySong(rs, loop);       //'true' denotes endless looping.
    mus_song = song;
}

static mobj_t *GetSoundListener(void)
{
    static degenmobj_t dummy_listener;

    // If we are at the title screen, the console player doesn't have an
    // object yet, so return a pointer to a static dummy listener instead.

    if (players[consoleplayer].mo != NULL)
    {
        return players[displayplayer].mo;
    }
    else
    {
        dummy_listener.x = 0;
        dummy_listener.y = 0;
        dummy_listener.z = 0;

        return (mobj_t *) &dummy_listener;
    }
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
    const int64_t dxy = (dy > dx) ? dy + (dx >> 1) : dx + (dy >> 1);

    return (dz > dxy) ? dz + (dxy >> 1) : dxy + (dz >> 1);
}

void S_StartSound(void *_origin, int sound_id)
{
    mobj_t *origin = _origin;
    mobj_t *listener;
    int dist, vol;
    int i;
    int priority;
    int sep;
    int angle;
    int64_t absx;
    int64_t absy;
    int64_t absz;  // [JN] Z-axis sfx distance

    static int sndcount = 0;
    int chan;

    // [JN] Do not play sound while demo-warp.
    if (nodrawers /*|| demowarp*/)
    {
        return;
    }

    listener = GetSoundListener();

    if (sound_id == 0 || snd_MaxVolume == 0 || (nodrawers && singletics))
        return;
    if (origin == NULL)
    {
        origin = listener;
    }

// calculate the distance before other stuff so that we can throw out
// sounds that are beyond the hearing range.
    absx = llabs(origin->x - listener->x);
    absy = llabs(origin->y - listener->y);
    absz = aud_z_axis_sfx ?
           llabs(origin->z - listener->z) : 0;
    dist = S_ApproxDistanceZ(absx, absy, absz);
    dist >>= FRACBITS;
//  dist = P_AproxDistance(origin->x-viewx, origin->y-viewy)>>FRACBITS;

    if (dist >= MAX_SND_DIST)
    {
//      dist = MAX_SND_DIST - 1;
        return;                 //sound is beyond the hearing range...
    }
    if (dist < 0)
    {
        dist = 0;
    }
    priority = S_sfx[sound_id].priority;
    priority *= (10 - (dist / 160));
    if (!S_StopSoundID(sound_id, priority))
    {
        return;                 // other sounds have greater priority
    }
    for (i = 0; i < snd_channels; i++)
    {
        if (origin->player)
        {
            i = snd_channels;
            break;              // let the player have more than one sound.
        }
        if (origin == channel[i].mo)
        {                       // only allow other mobjs one sound
            S_StopSound(channel[i].mo);
            break;
        }
    }
    if (i >= snd_channels)
    {
        if (sound_id >= sfx_wind)
        {
            if (AmbChan != -1 && S_sfx[sound_id].priority <=
                S_sfx[channel[AmbChan].sound_id].priority)
            {
                return;         //ambient channel already in use
            }
            else
            {
                AmbChan = -1;
            }
        }
        for (i = 0; i < snd_channels; i++)
        {
            if (channel[i].mo == NULL)
            {
                break;
            }
        }
        if (i >= snd_channels)
        {
            //look for a lower priority sound to replace.
            sndcount++;
            if (sndcount >= snd_channels)
            {
                sndcount = 0;
            }
            for (chan = 0; chan < snd_channels; chan++)
            {
                i = (sndcount + chan) % snd_channels;
                if (priority >= channel[i].priority)
                {
                    chan = -1;  //denote that sound should be replaced.
                    break;
                }
            }
            if (chan != -1)
            {
                return;         //no free channels.
            }
            else                //replace the lower priority sound.
            {
                if (channel[i].handle)
                {
                    if (I_SoundIsPlaying(channel[i].handle))
                    {
                        I_StopSound(channel[i].handle);
                    }
                    if (S_sfx[channel[i].sound_id].usefulness > 0)
                    {
                        S_sfx[channel[i].sound_id].usefulness--;
                    }

                    if (AmbChan == i)
                    {
                        AmbChan = -1;
                    }
                }
            }
        }
    }
    if (S_sfx[sound_id].lumpnum == 0)
    {
        S_sfx[sound_id].lumpnum = I_GetSfxLumpNum(&S_sfx[sound_id]);
    }

    // calculate the volume based upon the distance from the sound origin.
//      vol = (snd_MaxVolume*16 + dist*(-snd_MaxVolume*16)/MAX_SND_DIST)>>9;
    vol = soundCurve[dist];

    if (origin == listener || snd_monosfx)
    {
        sep = 128;
    }
    else
    {
        angle = R_PointToAngle2(listener->x, listener->y,
                                origin->x, origin->y);
        angle = (angle - viewangle) >> 24;
        if (gp_flip_levels)
            angle = 255 - angle;
        sep = angle * 2 - 128;
        if (sep < 64)
            sep = -sep;
        if (sep > 192)
            sep = 512 - sep;
    }

    channel[i].pitch = (byte) (NORM_PITCH + (M_Random() & 7) - (M_Random() & 7));
    channel[i].handle = I_StartSound(&S_sfx[sound_id], i, vol, sep, channel[i].pitch);
    channel[i].mo = origin;
    channel[i].sound_id = sound_id;
    channel[i].priority = priority;
    if (sound_id >= sfx_wind)
    {
        AmbChan = i;
    }
    if (S_sfx[sound_id].usefulness == -1)
    {
        S_sfx[sound_id].usefulness = 1;
    }
    else
    {
        S_sfx[sound_id].usefulness++;
    }
}

// -----------------------------------------------------------------------------
// S_StartSoundAmbient
// [JN] Plays ambient sound with updated-on-fly stereo separation.
// -----------------------------------------------------------------------------

void S_StartSoundAmbient (void *_origin, int sound_id)
{
    mobj_t *origin = _origin;
    mobj_t *listener;
    int dist, vol;
    int i;
    int sep;
    int angle;
    int64_t absx;
    int64_t absy;
    int64_t absz;  // [JN] Z-axis sfx distance

    if (sound_id == 0 || snd_MaxVolume == 0 || (nodrawers && singletics))
    {
        return;
    }

    listener = GetSoundListener();

    if (origin == NULL)
    {
        origin = listener;
    }

    if (origin == listener || snd_monosfx)
    {
        sep = 128;
    }
    else
    {
        angle = R_PointToAngle2(listener->x, listener->y, origin->x, origin->y);
        angle = (angle - viewangle) >> 24;
        if (gp_flip_levels)
            angle = 255 - angle;
        sep = angle * 2 - 128;
        if (sep < 64)
            sep = -sep;
        if (sep > 192)
            sep = 512 - sep;
    }

    // [JN] Calculate the distance.
    absx = llabs(origin->x - listener->x);
    absy = llabs(origin->y - listener->y);
    absz = aud_z_axis_sfx ?
           llabs(origin->z - listener->z) : 0;
    dist = S_ApproxDistanceZ(absx, absy, absz);
    dist >>= FRACBITS;

    if (dist >= MAX_SND_DIST)
    {
        dist = MAX_SND_DIST - 1;
    }
    if (dist < 0)
    {
        dist = 0;
    }

    // [JN] Calculate the volume based upon the distance from the sound origin.
    vol = soundCurve[dist];
    
    // [JN] No priority checking.
    for (i = 0; i < snd_channels; i++)
    {
        if (channel[i].mo == NULL)
        {
            break;
        }
    }

    if (i >= snd_channels)
    {
        return;
    }

    channel[i].pitch = (byte) (NORM_PITCH + (M_Random() & 7) - (M_Random() & 7));
    channel[i].handle = I_StartSound(&S_sfx[sound_id], i, vol, sep, channel[i].pitch);
    channel[i].mo = origin;
    channel[i].sound_id = sound_id;

    if (S_sfx[sound_id].usefulness == -1)
    {
        S_sfx[sound_id].usefulness = 1;
    }
    else
    {
        S_sfx[sound_id].usefulness++;
    }
}

void S_StartSoundAtVolume(void *_origin, int sound_id, int volume)
{
    mobj_t *origin = _origin;
    mobj_t *listener;
    int i;

    // [JN] CRL - do not play music while demo-warp.
    if (nodrawers /*|| demowarp*/)
    {
        return;
    }

    listener = GetSoundListener();

    if (sound_id == 0 || snd_MaxVolume == 0 || soundCurve[0] == 0)
        return;
    if (origin == NULL)
    {
        origin = listener;
    }

    if (volume == 0)
    {
        return;
    }
    volume = (volume * (snd_MaxVolume + 1) * 8) >> 7;

// no priority checking, as ambient sounds would be the LOWEST.
    for (i = 0; i < snd_channels; i++)
    {
        if (channel[i].mo == NULL)
        {
            break;
        }
    }
    if (i >= snd_channels)
    {
        return;
    }
    if (S_sfx[sound_id].lumpnum == 0)
    {
        S_sfx[sound_id].lumpnum = I_GetSfxLumpNum(&S_sfx[sound_id]);
    }

    channel[i].pitch = (byte) (NORM_PITCH - (M_Random() & 3) + (M_Random() & 3));
    channel[i].handle = I_StartSound(&S_sfx[sound_id], i, volume, 128, channel[i].pitch);
    channel[i].mo = origin;
    channel[i].sound_id = sound_id;
    channel[i].priority = 1;    //super low priority.
    if (S_sfx[sound_id].usefulness == -1)
    {
        S_sfx[sound_id].usefulness = 1;
    }
    else
    {
        S_sfx[sound_id].usefulness++;
    }
}

boolean S_StopSoundID(int sound_id, int priority)
{
    int i;
    int lp;                     //least priority
    int found;

    if (S_sfx[sound_id].numchannels == -1)
    {
        return (true);
    }
    lp = -1;                    //denote the argument sound_id
    found = 0;
    for (i = 0; i < snd_channels; i++)
    {
        if (channel[i].sound_id == sound_id && channel[i].mo)
        {
            found++;            //found one.  Now, should we replace it??
            if (priority >= channel[i].priority)
            {                   // if we're gonna kill one, then this'll be it
                lp = i;
                priority = channel[i].priority;
            }
        }
    }
    if (found < S_sfx[sound_id].numchannels)
    {
        return (true);
    }
    else if (lp == -1)
    {
        return (false);         // don't replace any sounds
    }
    if (channel[lp].handle)
    {
        if (I_SoundIsPlaying(channel[lp].handle))
        {
            I_StopSound(channel[lp].handle);
        }
        if (S_sfx[channel[i].sound_id].usefulness > 0)
        {
            S_sfx[channel[i].sound_id].usefulness--;
        }
        channel[lp].mo = NULL;
    }
    return (true);
}

void S_StopSound(void *_origin)
{
    mobj_t *origin = _origin;
    int i;

    for (i = 0; i < snd_channels; i++)
    {
        if (channel[i].mo == origin)
        {
            I_StopSound(channel[i].handle);
            if (S_sfx[channel[i].sound_id].usefulness > 0)
            {
                S_sfx[channel[i].sound_id].usefulness--;
            }
            channel[i].handle = 0;
            channel[i].mo = NULL;
            if (AmbChan == i)
            {
                AmbChan = -1;
            }
        }
    }
}

void S_SoundLink(mobj_t * oldactor, mobj_t * newactor)
{
    int i;

    for (i = 0; i < snd_channels; i++)
    {
        if (channel[i].mo == oldactor)
            channel[i].mo = newactor;
    }
}

void S_PauseSound(void)
{
    I_PauseSong();
}

void S_ResumeSound(void)
{
    I_ResumeSong();
}

void S_UpdateSounds(mobj_t * listener)
{
    int i, dist, vol;
    int angle;
    int sep;
    int priority;
    int64_t absx;
    int64_t absy;
    int64_t absz;  // [JN] Z-axis sfx distance

    I_UpdateSound();

    listener = GetSoundListener();
    if (snd_MaxVolume == 0)
    {
        return;
    }

    for (i = 0; i < snd_channels; i++)
    {
        if (!channel[i].handle || S_sfx[channel[i].sound_id].usefulness == -1)
        {
            continue;
        }
        if (!I_SoundIsPlaying(channel[i].handle))
        {
            if (S_sfx[channel[i].sound_id].usefulness > 0)
            {
                S_sfx[channel[i].sound_id].usefulness--;
            }
            channel[i].handle = 0;
            channel[i].mo = NULL;
            channel[i].sound_id = 0;
            if (AmbChan == i)
            {
                AmbChan = -1;
            }
        }
        if (channel[i].mo == NULL || channel[i].sound_id == 0
         || channel[i].mo == listener || listener == NULL)
        {
            continue;
        }
        else
        {
            absx = llabs(channel[i].mo->x - listener->x);
            absy = llabs(channel[i].mo->y - listener->y);
            // [JN] Z-axis sfx distance.
            absz = aud_z_axis_sfx ? 
                   llabs(channel[i].mo->z - listener->z) : 0;
            dist = S_ApproxDistanceZ(absx, absy, absz);
            dist >>= FRACBITS;
//          dist = P_AproxDistance(channel[i].mo->x-listener->x, channel[i].mo->y-listener->y)>>FRACBITS;

            if (dist >= MAX_SND_DIST)
            {
                S_StopSound(channel[i].mo);
                continue;
            }
            if (dist < 0)
                dist = 0;

// calculate the volume based upon the distance from the sound origin.
//          vol = (*((byte *)W_CacheLumpName("SNDCURVE", PU_CACHE)+dist)*(snd_MaxVolume*8))>>7;
            vol = soundCurve[dist];

            // [JN] Support for mono sfx mode
            if (snd_monosfx)
            {
                sep = 128;
            }
            else
            {
                angle = R_PointToAngle2(listener->x, listener->y,
                                        channel[i].mo->x, channel[i].mo->y);
                angle = (angle - viewangle) >> 24;
                if (gp_flip_levels)
                    angle = 255 - angle;
                sep = angle * 2 - 128;
                if (sep < 64)
                    sep = -sep;
                if (sep > 192)
                    sep = 512 - sep;
            }
            // TODO: Pitch shifting.
            I_UpdateSoundParams(channel[i].handle, vol, sep);
            priority = S_sfx[channel[i].sound_id].priority;
            priority *= (10 - (dist >> 8));
            channel[i].priority = priority;
        }
    }
}

void S_Init(void)
{
    idmusnum = -1; // [JN] jff 3/17/98 insure idmus number is blank

    I_SetOPLDriverVer(opl_doom2_1_666);
    soundCurve = Z_Malloc(MAX_SND_DIST, PU_STATIC, NULL);
    if (snd_channels > 16)
    {
        snd_channels = 16;
    }
    I_SetMusicVolume(snd_MusicVolume * 8);
    S_SetMaxVolume();

    I_AtExit(S_ShutDown, true);

    // Heretic defaults to pitch-shifting on
    if (snd_pitchshift == -1)
    {
        snd_pitchshift = 1;
    }

    I_PrecacheSounds(S_sfx, NUMSFX);
}

void S_GetChannelInfo(SoundInfo_t * s)
{
    int i;
    ChanInfo_t *c;

    s->channelCount = snd_channels;
    s->musicVolume = snd_MusicVolume;
    s->soundVolume = snd_MaxVolume;
    for (i = 0; i < snd_channels; i++)
    {
        c = &s->chan[i];
        c->id = channel[i].sound_id;
        c->priority = channel[i].priority;
        c->name = S_sfx[c->id].name;
        c->mo = channel[i].mo;

        if (c->mo != NULL)
        {
            c->distance = P_AproxDistance(c->mo->x - viewx, c->mo->y - viewy)
                >> FRACBITS;
        }
        else
        {
            c->distance = 0;
        }
    }
}

void S_SetMaxVolume(void)
{
    int i;

    for (i = 0; i < MAX_SND_DIST; i++)
    {
        soundCurve[i] =
            (*((byte *) W_CacheLumpName("SNDCURVE", PU_CACHE) + i) *
             (snd_MaxVolume * 8)) >> 7;
    }
}

static boolean musicPaused;
void S_SetMusicVolume(void)
{
    I_SetMusicVolume(snd_MusicVolume * 8);
    if (snd_MusicVolume == 0)
    {
        I_PauseSong();
        musicPaused = true;
    }
    else if (musicPaused)
    {
        musicPaused = false;
        I_ResumeSong();
    }
}

void S_ShutDown(void)
{
    I_StopSong();
    if (rs != NULL)
    {
        I_UnRegisterSong(rs);
        rs = NULL;
        mus_song = -1;
    }
    I_ShutdownSound();
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
            if (channel[i].handle)
            {
                S_StopSound(channel[i].mo);
            }
        }
        memset(channel, 0, snd_channels * sizeof(channel_t));

        // Set volume to zero.
        I_SetMusicVolume(0);
        for (int i = 0; i < MAX_SND_DIST; i++)
        {
            soundCurve[i] = 0;
        }

    }
    else
    {
        // Restore volume to actual values.
        I_SetMusicVolume(snd_MusicVolume * 8);
        S_SetMaxVolume();
    }

    // All done, no need to invoke function until next 
    // minimizing/restoring of game window is happened.
    volume_needs_update = false;
}
