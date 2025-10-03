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

#include "h2def.h"
#include "m_random.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_array.h"
#include "m_argv.h"
#include "m_misc.h"
#include "r_local.h"
#include "p_local.h"            // for P_AproxDistance
#include "sounds.h"
#include "s_sound.h"

#include "id_vars.h"


#define PRIORITY_MAX_ADJUST 10
#define DIST_ADJUST (MAX_SND_DIST/PRIORITY_MAX_ADJUST)

#define DEFAULT_ARCHIVEPATH     "o:\\sound\\archive\\"



/*
===============================================================================

		MUSIC & SFX API

===============================================================================
*/

//static channel_t channel[MAX_CHANNELS];

//static int rs; //the current registered song.
//int mus_song = -1;
//int mus_lumpnum;
//void *mus_sndptr;
//byte *soundCurve;


static channel_t Channel[MAX_CHANNELS];
static void *RegisteredSong;      //the current registered song.
static boolean MusicPaused;
int Mus_Song = -1;
static byte *Mus_SndPtr;
static byte *SoundCurve;

int snd_MaxVolume = 10;                // maximum volume for sound
int snd_MusicVolume = 10;              // maximum volume for music
// [JN] Internal sound variables, friendly with muting.
static int sfxVolume;
static int musVolume;

static degenmobj_t dummy_listener;

// [PN] Which lump is currently "held" for music.
static int Mus_LumpNum = -1;
// [JN] Enforce music replay while changing music system.
boolean mus_force_replay = false;

// int AmbChan;

//==========================================================================
//
// S_Start
//
//==========================================================================

void S_Start(void)
{
    S_StopAllSound();
    S_StartSong(gamemap, true);
}

// -----------------------------------------------------------------------------
// S_RemasterSong 
//  [PN] Returns the remastered version of a music lump if available,
//  otherwise falls back to the vanilla music lump.
// -----------------------------------------------------------------------------

typedef struct {
    const char *src;   // Initial lump (vanilla)
    const char *r;     // Remastered R_*
    const char *o;     // Original O_*
} ost_remap_t;

static const ost_remap_t remaster_ost_remap[] = {
	{ "Winnowr", "R_WINNOW", "O_WINNOW" },
	{ "Jachr",   "R_JACH",   "O_JACH"   },
	{ "Simonr",  "R_SIMON",  "O_SIMON"  },
	{ "Wutzitr", "R_WUTZIT", "O_WUTZIT" },
	{ "Falconr", "R_FALCON", "O_FALCON" },
	{ "Levelr",  "R_LEVEL",  "O_LEVEL"  },
	{ "Chartr",  "O_CHART",  "O_CHART"  },
	{ "Swampr",  "R_SWAMP",  "O_SWAMP"  },
	{ "Deepr",   "R_DEEP",   "O_DEEP"   },
	{ "Fubasr",  "R_FUBAS",  "O_FUBAS"  },
	{ "Grover",  "R_GROVE",  "O_GROVE"  },
	{ "Fortr",   "R_FORT",   "O_FORT"   },
	{ "Foojar",  "R_FOOJA",  "O_FOOJA"  },
	{ "Sixater", "R_SIXATE", "O_SIXATE" },
	{ "Wobabyr", "R_WOBABY", "O_WOBABY" },
	{ "Cryptr",  "R_CRYPT",  "O_CRYPT"  },
	{ "Fantar",  "R_FANTA",  "O_FANTA"  },
	{ "Blechr",  "R_BLECH",  "O_BLECH"  },
	{ "Voidr",   "R_VOID",   "O_VOID"   },
	{ "Chap_1r", "R_CHAP_1", "O_CHAP_1" },
	{ "Chap_2r", "O_CHAP_2", "O_CHAP_2" },
	{ "Chap_3r", "R_CHAP_3", "O_CHAP_3" },
	{ "Chap_4r", "R_CHAP_4", "O_CHAP_4" },
	{ "Chippyr", "R_CHIPPY", "O_CHIPPY" },
	{ "Percr",   "R_PERC",   "O_PERC"   },
	{ "Secretr", "R_SECRET", "O_SECRET" },
	{ "Bonesr",  "R_BONES",  "O_BONES"  },
	{ "Octor",   "R_OCTO",   "O_OCTO"   },
	{ "Rithmr",  "R_RITHM",  "O_RITHM"  },
	{ "Stalkr",  "R_STALK",  "O_STALK"  },
	{ "Borkr",   "R_BORK",   "O_BORK"   },
	{ "Crucibr", "R_CRUCIB", "O_CRUCIB" },
	{ "hexen",   "R_HEXEN",  "O_HEXEN"  },
	{ "hub",     "R_HUB",    "O_HUB"    },
	{ "hall",    "R_HALL",   "O_HALL"   },
	{ "orb",     "R_ORB",    "O_ORB"    },
	{ "chess",   "R_CHESS",  "O_CHESS"  },
};

static const char *S_RemasterSong (const char *songLump)
{
    // Find a mapping entry by original name (case-insensitive).
    const ost_remap_t *music = NULL;
    for (size_t i = 0; i < ARRAY_LEN(remaster_ost_remap); ++i)
    {
        if (strcasecmp(songLump, remaster_ost_remap[i].src) == 0)
        {
            music = &remaster_ost_remap[i];
            break;
        }
    }

    if (!music)
    {
        return songLump; // No mapping found — keep the original lump
    }

    // Select the variant according to settings and availability,
    // also making sure that the target lump actually exists.
    if (snd_remaster_ost == 1 && remaster_ost_r)
    {
        if (W_CheckNumForName(music->r) >= 0)
            return music->r;
    }
    else if (snd_remaster_ost == 2 && remaster_ost_o)
    {
        if (W_CheckNumForName(music->o) >= 0)
            return music->o;
    }

    // Fallback to the original if no remaster is available
    return songLump;
}

//==========================================================================
//
// S_StartSong
//
//==========================================================================

void S_StartSong(int song, boolean loop)
{
    const char *songLump;
    int lumpnum;
    int length;

    if (nodrawers)
    {
        return;
    }

    // If we're in CD music mode, play a CD track, instead:
    /*
    if (cdmusic)
    {
        // Default to the player-chosen track
        if (cd_custom_track != 0)
        {
            track = cd_custom_track;
        }
        else
        {
            track = P_GetMapCDTrack(gamemap);
        }

        StartCDTrack(track, loop);
    }
    else
    */
    {
        if (song == Mus_Song && !mus_force_replay)
        {                       // don't replay an old song
            return;             // [JN] Unless music system change.
        }
        if (RegisteredSong)
        {
            I_StopSong();
            I_UnRegisterSong(RegisteredSong);
            RegisteredSong = 0;
            Mus_Song = -1;

            // [PN] Release the previous music lump, if any
            if (Mus_LumpNum != -1)
            {
                W_ReleaseLumpNum(Mus_LumpNum);
                Mus_LumpNum = -1;
            }
        }
        songLump = P_GetMapSongLump(song);
        if (!songLump)
        {
            return;
        }

        // [PN] Resolve to R_/O_ remaster, or fallback to original.
        songLump = S_RemasterSong(songLump);

        lumpnum = W_GetNumForName(songLump);
        Mus_SndPtr = W_CacheLumpNum(lumpnum, PU_MUSIC);
        length = W_LumpLength(lumpnum);

        RegisteredSong = I_RegisterSong(Mus_SndPtr, length);
        I_PlaySong(RegisteredSong, loop);
        Mus_Song = song;

        // [PN] Do not release now
        // W_ReleaseLumpNum(lumpnum);
        Mus_LumpNum = lumpnum;
    }
}

//==========================================================================
//
// S_StartSongName
//
//==========================================================================

void S_StartSongName(const char *songLump, boolean loop)
{
    int lumpnum;
    int length;

    if (!songLump || nodrawers)
    {
        return;
    }
    /*
    if (cdmusic)
    {
        cdTrack = 0;

        if (!strcmp(songLump, "hexen"))
        {
            cdTrack = P_GetCDTitleTrack();
        }
        else if (!strcmp(songLump, "hub"))
        {
            cdTrack = P_GetCDIntermissionTrack();
        }
        else if (!strcmp(songLump, "hall"))
        {
            cdTrack = P_GetCDEnd1Track();
        }
        else if (!strcmp(songLump, "orb"))
        {
            cdTrack = P_GetCDEnd2Track();
        }
        else if (!strcmp(songLump, "chess") && cd_custom_track == 0)
        {
            cdTrack = P_GetCDEnd3Track();
        }
//	Uncomment this, if Kevin writes a specific song for startup
//	else if(!strcmp(songLump, "start"))
//	{
//		cdTrack = P_GetCDStartTrack();
//	}
        if (cdTrack != 0)
        {
            cd_custom_track = 0;
            StartCDTrack(cdTrack, loop);
        }
    }
    else
    */
    {
        if (RegisteredSong)
        {
            I_StopSong();
            I_UnRegisterSong(RegisteredSong);
            RegisteredSong = NULL;
            Mus_Song = -1;

            // [PN] Release the previously held music lump.
            if (Mus_LumpNum != -1)
            {
                W_ReleaseLumpNum(Mus_LumpNum);
                Mus_LumpNum = -1;
            }
        }

        // [PN] Resolve to R_/O_ remaster, or fallback to original.
        songLump = S_RemasterSong(songLump);

        lumpnum = W_GetNumForName(songLump);
        Mus_SndPtr = W_CacheLumpNum(lumpnum, PU_MUSIC);
        length = W_LumpLength(lumpnum);

        RegisteredSong = I_RegisterSong(Mus_SndPtr, length);
        I_PlaySong(RegisteredSong, loop);

        // [PN] Do not release now
        // W_ReleaseLumpNum(lumpnum);
        // Mus_Song = -1;
        Mus_LumpNum = lumpnum;
    }
}

//==========================================================================
//
// S_GetSoundID
//
//==========================================================================

int S_GetSoundID (const char *name)
{
    for (int i = 0; i < NUMSFX; i++)
    {
        if (!strcmp(S_sfx[i].tagname, name))
        {
            return i;
        }
    }
    return 0;
}

//==========================================================================
//
// S_StartSound
//
//==========================================================================

void S_StartSound(mobj_t * origin, int sound_id)
{
    S_StartSoundAtVolume(origin, sound_id, 127);
}

static mobj_t *GetSoundListener(void)
{
    // If we are at the title screen, the console player doesn't have an
    // object yet, so return a pointer to a static dummy listener instead.

    if (players[displayplayer].mo != NULL)
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


//==========================================================================
//
// S_StartSoundAtVolume
//
//==========================================================================

void S_StartSoundAtVolume(mobj_t * origin, int sound_id, int volume)
{
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

    if (sound_id == 0 || sfxVolume == 0 || nodrawers)
        return;

    listener = GetSoundListener();

    if (origin == NULL)
    {
        origin = listener;
    }
    if (volume == 0)
    {
        return;
    }

    // calculate the distance before other stuff so that we can throw out
    // sounds that are beyond the hearing range.
    absx = abs(origin->x - listener->x);
    absy = abs(origin->y - listener->y);
    absz = aud_z_axis_sfx ?
           llabs(origin->z - listener->z) : 0;
    dist = S_ApproxDistanceZ(absx, absy, absz);
    dist >>= FRACBITS;
    if (dist >= MAX_SND_DIST)
    {
        return;                 // sound is beyond the hearing range...
    }
    if (dist < 0)
    {
        dist = 0;
    }
    priority = S_sfx[sound_id].priority;
    priority *= (PRIORITY_MAX_ADJUST - (dist / DIST_ADJUST));
    #if 0
    // TODO
    if (!S_StopSoundID(sound_id, priority))
    {
        return;                 // other sounds have greater priority
    }
    #endif
    for (i = 0; i < snd_channels; i++)
    {
        if (origin != (mobj_t *)&dummy_listener && origin->player)
        {
            i = snd_channels;
            break;              // let the player have more than one sound.
        }
        if (origin == Channel[i].mo)
        {                       // only allow other mobjs one sound
            S_StopSound(Channel[i].mo);
            break;
        }
    }
    if (i >= snd_channels)
    {
        for (i = 0; i < snd_channels; i++)
        {
            if (Channel[i].mo == NULL)
            {
                break;
            }
        }
        if (i >= snd_channels)
        {
            // look for a lower priority sound to replace.
            sndcount++;
            if (sndcount >= snd_channels)
            {
                sndcount = 0;
            }
            for (chan = 0; chan < snd_channels; chan++)
            {
                i = (sndcount + chan) % snd_channels;
                if (priority >= Channel[i].priority)
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
                if (Channel[i].handle)
                {
                    if (I_SoundIsPlaying(Channel[i].handle))
                    {
                        I_StopSound(Channel[i].handle);
                    }
                    if (S_sfx[Channel[i].sound_id].usefulness > 0)
                    {
                        S_sfx[Channel[i].sound_id].usefulness--;
                    }
                }
            }
        }
    }

    Channel[i].mo = origin;

    vol = (SoundCurve[dist] * (sfxVolume * 8) * volume) >> 14;
    if (origin == listener || snd_monosfx)
    {
        sep = 128;
//              vol = (volume*(sfxVolume+1)*8)>>7;
    }
    else
    {
        angle = R_PointToAngle2(listener->x,
                                listener->y,
                                Channel[i].mo->x, Channel[i].mo->y);
        angle = (angle - viewangle) >> 24;
        if (gp_flip_levels)
            angle = 255 - angle;
        sep = angle * 2 - 128;
        if (sep < 64)
            sep = -sep;
        if (sep > 192)
            sep = 512 - sep;
//              vol = SoundCurve[dist];
    }

    // if the sfxinfo_t is marked as 'can be pitch shifted'
    if (S_sfx[sound_id].pitch)
    {
        Channel[i].pitch = (byte) (NORM_PITCH + (M_Random() & 7) - (M_Random() & 7));
    }
    else
    {
        Channel[i].pitch = NORM_PITCH;
    }

    if (S_sfx[sound_id].lumpnum == 0)
    {
        S_sfx[sound_id].lumpnum = I_GetSfxLumpNum(&S_sfx[sound_id]);
    }

    Channel[i].handle = I_StartSound(&S_sfx[sound_id],
                                     i,
                                     vol,
                                     sep,
                                     Channel[i].pitch);
    Channel[i].sound_id = sound_id;
    Channel[i].priority = priority;
    Channel[i].volume = volume;
    if (S_sfx[sound_id].usefulness < 0)
    {
        S_sfx[sound_id].usefulness = 1;
    }
    else
    {
        S_sfx[sound_id].usefulness++;
    }
}

//==========================================================================
//
// S_StopSoundID
//
//==========================================================================

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
        if (Channel[i].sound_id == sound_id && Channel[i].mo)
        {
            found++;            //found one.  Now, should we replace it??
            if (priority >= Channel[i].priority)
            {                   // if we're gonna kill one, then this'll be it
                lp = i;
                priority = Channel[i].priority;
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
    if (Channel[lp].handle)
    {
        if (I_SoundIsPlaying(Channel[lp].handle))
        {
            I_StopSound(Channel[lp].handle);
        }
        if (S_sfx[Channel[lp].sound_id].usefulness > 0)
        {
            S_sfx[Channel[lp].sound_id].usefulness--;
        }
        Channel[lp].mo = NULL;
    }
    return (true);
}

//==========================================================================
//
// S_StopSound
//
//==========================================================================

void S_StopSound (const mobj_t *origin)
{
    for (int i = 0; i < snd_channels; i++)
    {
        if (Channel[i].mo == origin)
        {
            I_StopSound(Channel[i].handle);
            if (S_sfx[Channel[i].sound_id].usefulness > 0)
            {
                S_sfx[Channel[i].sound_id].usefulness--;
            }
            Channel[i].handle = 0;
            Channel[i].mo = NULL;
        }
    }
}

//==========================================================================
//
// S_StopAllSound
//
//==========================================================================

void S_StopAllSound(void)
{
    int i;

    //stop all sounds
    for (i = 0; i < snd_channels; i++)
    {
        if (Channel[i].handle)
        {
            S_StopSound(Channel[i].mo);
        }
    }
    memset(Channel, 0, snd_channels * sizeof(channel_t));
}

//==========================================================================
//
// S_PauseSound
//
//==========================================================================

void S_PauseSound(void)
{
    /*
    if (cdmusic)
    {
        I_CDMusStop();
    }
    else
    */
    {
        I_PauseSong();
    }
}

//==========================================================================
//
// S_ResumeSound
//
//==========================================================================

void S_ResumeSound(void)
{
    /*
    if (cdmusic)
    {
        I_CDMusResume();
    }
    else
    */
    {
        I_ResumeSong();
    }
}

//==========================================================================
//
// S_UpdateSounds
//
//==========================================================================

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

    if (sfxVolume == 0)
    {
        return;
    }

    // Update any Sequences
    SN_UpdateActiveSequences();

    for (i = 0; i < snd_channels; i++)
    {
        if (!Channel[i].handle || S_sfx[Channel[i].sound_id].usefulness == -1)
        {
            continue;
        }
        if (!I_SoundIsPlaying(Channel[i].handle))
        {
            if (S_sfx[Channel[i].sound_id].usefulness > 0)
            {
                S_sfx[Channel[i].sound_id].usefulness--;
            }
            Channel[i].handle = 0;
            Channel[i].mo = NULL;
            Channel[i].sound_id = 0;
        }
        if (Channel[i].mo == NULL || Channel[i].sound_id == 0
         || Channel[i].mo == listener || listener == NULL)
        {
            continue;
        }
        else
        {
            absx = abs(Channel[i].mo->x - listener->x);
            absy = abs(Channel[i].mo->y - listener->y);
            // [JN] Z-axis sfx distance.
            absz = aud_z_axis_sfx ? 
                   llabs(Channel[i].mo->z - listener->z) : 0;
            dist = S_ApproxDistanceZ(absx, absy, absz);
            dist >>= FRACBITS;

            if (dist >= MAX_SND_DIST)
            {
                S_StopSound(Channel[i].mo);
                continue;
            }
            if (dist < 0)
            {
                dist = 0;
            }
            //vol = SoundCurve[dist];
            vol =
                (SoundCurve[dist] * (sfxVolume * 8) *
                 Channel[i].volume) >> 14;
            if (Channel[i].mo == listener || snd_monosfx)
            {
                sep = 128;
            }
            else
            {
                angle = R_PointToAngle2(listener->x, listener->y,
                                        Channel[i].mo->x, Channel[i].mo->y);
                angle = (angle - viewangle) >> 24;
                if (gp_flip_levels)
                    angle = 255 - angle;
                sep = angle * 2 - 128;
                if (sep < 64)
                    sep = -sep;
                if (sep > 192)
                    sep = 512 - sep;
            }
            I_UpdateSoundParams(i, vol, sep);
            priority = S_sfx[Channel[i].sound_id].priority;
            priority *= PRIORITY_MAX_ADJUST - (dist / DIST_ADJUST);
            Channel[i].priority = priority;
        }
    }
}

//==========================================================================
//
// S_Init
//
//==========================================================================

void S_Init(void)
{
    I_SetOPLDriverVer(opl_doom2_1_666);
    SoundCurve = W_CacheLumpName("SNDCURVE", PU_STATIC);
//      SoundCurve = Z_Malloc(MAX_SND_DIST, PU_STATIC, NULL);

    if (snd_channels > 16)
    {
        snd_channels = 16;
    }
    // [JN] Initialize internal volume variables.
    sfxVolume = snd_MaxVolume;
    musVolume = snd_MusicVolume;
    I_SetMusicVolume(musVolume * 8);

    I_AtExit(S_ShutDown, true);

    // Hexen defaults to pitch-shifting on
    if (snd_pitchshift == -1)
    {
        snd_pitchshift = 1;
    }

    I_PrecacheSounds(S_sfx, NUMSFX);
}

//==========================================================================
//
// S_GetChannelInfo
//
//==========================================================================

void S_GetChannelInfo(SoundInfo_t * s)
{
    int i;
    ChanInfo_t *c;

    s->channelCount = snd_channels;
    s->musicVolume = musVolume;
    s->soundVolume = sfxVolume;
    for (i = 0; i < snd_channels; i++)
    {
        c = &s->chan[i];
        c->id = Channel[i].sound_id;
        c->priority = Channel[i].priority;
        c->name = S_sfx[c->id].name;
        c->mo = Channel[i].mo;

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

//==========================================================================
//
// S_GetSoundPlayingInfo
//
//==========================================================================

boolean S_GetSoundPlayingInfo (const mobj_t *const mobj, int sound_id)
{
    for (int i = 0; i < snd_channels; i++)
    {
        if (Channel[i].sound_id == sound_id && Channel[i].mo == mobj)
        {
            if (I_SoundIsPlaying(Channel[i].handle))
            {
                return true;
            }
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// S_SetSfxVolume
// -----------------------------------------------------------------------------

void S_SetSfxVolume (int volume)
{
    sfxVolume = volume;
}

//==========================================================================
//
// S_SetMusicVolume
//
//==========================================================================

void S_SetMusicVolume (int volume)
{
    musVolume = volume;
    
    I_SetMusicVolume(musVolume * 8);

    if (musVolume == 0)
    {
        I_PauseSong();
        MusicPaused = true;
    }
    else if (MusicPaused)
    {
        I_ResumeSong();
        MusicPaused = false;
    }
}

//==========================================================================
//
// S_ShutDown
//
//==========================================================================

void S_ShutDown(void)
{
    I_StopSong();
    if (RegisteredSong)
    {
        I_UnRegisterSong(RegisteredSong);
        RegisteredSong = NULL;
        Mus_Song = -1;
    }
    // [PN] Release the previously held music lump.
    if (Mus_LumpNum != -1)
    {
        W_ReleaseLumpNum(Mus_LumpNum);
        Mus_LumpNum = -1;
    }
    I_ShutdownSound();
}

//==========================================================================
//
// S_InitScript
//
//==========================================================================

void S_InitScript(void)
{
    int i;

    SC_OpenLump("sndinfo");

    while (SC_GetString())
    {
        if (*sc_String == '$')
        {
            if (!strcasecmp(sc_String, "$ARCHIVEPATH"))
            {
                SC_MustGetString();
            }
            else if (!strcasecmp(sc_String, "$MAP"))
            {
                SC_MustGetNumber();
                SC_MustGetString();
                if (sc_Number)
                {
                    P_PutMapSongLump(sc_Number, sc_String);
                }
            }
            continue;
        }
        else
        {
            for (i = 0; i < NUMSFX; i++)
            {
                if (!strcmp(S_sfx[i].tagname, sc_String))
                {
                    SC_MustGetString();
                    if (*sc_String != '?')
                    {
                        M_StringCopy(S_sfx[i].name, sc_String,
                                     sizeof(S_sfx[i].name));
                    }
                    else
                    {
                        M_StringCopy(S_sfx[i].name, "default",
                                     sizeof(S_sfx[i].name));
                    }
                    break;
                }
            }
            if (i == NUMSFX)
            {
                SC_MustGetString();
            }
        }
    }
    SC_Close();

    for (i = 0; i < NUMSFX; i++)
    {
        if (!strcmp(S_sfx[i].name, ""))
        {
            M_StringCopy(S_sfx[i].name, "default", sizeof(S_sfx[i].name));
        }
    }
}

// -----------------------------------------------------------------------------
// S_StopMusic
//  [JN] Stop current music without shutting down sound system.
// -----------------------------------------------------------------------------

void S_StopMusic (void)
{
    I_StopSong();
    if (RegisteredSong != NULL)
    {
        I_UnRegisterSong(RegisteredSong);
        RegisteredSong = NULL;
        Mus_Song = -1;
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
        S_StopAllSound();

        // Set volume to zero.
        S_SetSfxVolume(0);
        S_SetMusicVolume(0);

    }
    else
    {
        // Restore volume to actual values.
        S_SetSfxVolume(snd_MaxVolume);
        S_SetMusicVolume(snd_MusicVolume);
    }

    // All done, no need to invoke function until next 
    // minimizing/restoring of game window is happened.
    volume_needs_update = false;
}
