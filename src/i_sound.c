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
// DESCRIPTION:  none
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "SDL_mixer.h"

#include "config.h"
#include "doomtype.h"

#include "gusconf.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"

// Sound sample rate to use for digital output (Hz)

int snd_samplerate = 44100;

// Maximum number of bytes to dedicate to allocated sound effects.
// (Default: 64MB)

int snd_cachesize = 64 * 1024 * 1024;

// Config variable that controls the sound buffer size.
// We default to 28ms (1000 / 35fps = 1 buffer per tic).

int snd_maxslicetime_ms = 28;

// External command to invoke to play back music.

char *snd_musiccmd = "";

// Whether to vary the pitch of sound effects
// Each game will set the default differently

int snd_pitchshift = -1;

int snd_musicdevice = SNDDEVICE_SB;
int snd_sfxdevice = SNDDEVICE_SB;

// [PN] Music volume auto gain.

int snd_auto_gain = 1;

// Low-level sound and music modules we are using
static const sound_module_t *sound_module;
static const music_module_t *music_module;

// If true, the music pack module was successfully initialized.
static boolean music_packs_active = false;

// This is either equal to music_module or &music_pack_module,
// depending on whether the current track is substituted.
static const music_module_t *active_music_module;

#ifndef DISABLE_SDL2MIXER
// -----------------------------------------------------------------------------
// [PN] Music auto gain (runtime normalization via SDL_mixer postmix analysis)
// -----------------------------------------------------------------------------

#define AUTO_GAIN_Q12_ONE           4096
#define AUTO_GAIN_TARGET_DB         -23.0f
#define AUTO_GAIN_FLOOR_DB          -70.0f
#define AUTO_GAIN_MIN_FACTOR        0.25f
#define AUTO_GAIN_MAX_FACTOR        3.00f
#define AUTO_GAIN_CLIP_LIMIT        0.98f
#define AUTO_GAIN_SILENCE_LEVEL     0.0025f
#define AUTO_GAIN_ATTACK            0.18f
#define AUTO_GAIN_RELEASE           0.06f
#define AUTO_GAIN_MOMENTARY_BLEND   0.45f
#define AUTO_GAIN_SHORTTERM_BLEND   0.10f
#define AUTO_GAIN_GLOBAL_BLEND      0.02f
#define AUTO_GAIN_WEIGHT_M          0.30f
#define AUTO_GAIN_WEIGHT_S          1.00f
#define AUTO_GAIN_WEIGHT_G          0.60f
#define AUTO_GAIN_FASTSTART_SECONDS 0.35f
#define AUTO_GAIN_FAST_ATTACK       0.70f
#define AUTO_GAIN_FAST_RELEASE      0.45f

#define DB_TO_GAIN(db)              powf(10.0f, (db) / 20.0f)

static SDL_atomic_t music_auto_gain_q12;
static SDL_atomic_t music_auto_gain_reset_pending;
static SDL_atomic_t music_auto_gain_fast_samples_left;
static boolean music_auto_gain_hook_registered = false;
static boolean music_auto_gain_s16_output = false;
static int music_auto_gain_channels = 2;
static int music_auto_gain_sample_rate = 44100;
static boolean music_stream_active = false;
static int music_base_volume = 127;
static int music_last_effective_volume = -1;

static boolean I_MusicAutoGainQuerySpec(int *freq, Uint16 *format, int *channels)
{
    int q_freq;
    Uint16 q_format;
    int q_channels;

    if (Mix_QuerySpec(&q_freq, &q_format, &q_channels) == 0)
    {
        return false;
    }

    if (freq != NULL)
    {
        *freq = q_freq;
    }
    if (format != NULL)
    {
        *format = q_format;
    }
    if (channels != NULL)
    {
        *channels = q_channels;
    }

    return true;
}

static float I_MusicAutoGainLinToDb(float level)
{
    if (level <= 0.0f)
    {
        return AUTO_GAIN_FLOOR_DB;
    }

    return 20.0f * log10f(level);
}

static void I_MusicAutoGainPostmix(void *udata, Uint8 *stream, int len)
{
    static float momentary_level = 0.0f;
    static float shortterm_level = 0.0f;
    static float global_level = 0.0f;
    static uint64_t total_frames = 0;
    static int level_initialized = 0;
    int16_t *samples;
    int sample_count;
    double sum_sq = 0.0;
    int peak_abs = 0;
    int fast_samples_left;
    float current;
    float desired;
    float peak;
    float rms_level;

    (void) udata;

    if (!snd_auto_gain || !music_stream_active || !music_auto_gain_s16_output
     || stream == NULL || len <= 0)
    {
        return;
    }

    // [PN] Ignore SFX activity while estimating music loudness.
    // Auto-gain should react to music content, not gameplay sound effects.
    if (Mix_Playing(-1) > 0)
    {
        return;
    }

    // [PN] Pseudo PC-speaker is mixed through a post effect, not regular
    // channels; guard it separately so it doesn't modulate music gain.
    if (snd_sfxdevice == SNDDEVICE_PCSPEAKER && I_PCS_HasPendingTone())
    {
        return;
    }

    samples = (int16_t *) stream;
    sample_count = len / (int) sizeof(int16_t);

    if (sample_count <= 0)
    {
        return;
    }

    if (SDL_AtomicGet(&music_auto_gain_reset_pending))
    {
        momentary_level = 0.0f;
        shortterm_level = 0.0f;
        global_level = 0.0f;
        total_frames = 0;
        level_initialized = 0;
        SDL_AtomicSet(&music_auto_gain_reset_pending, 0);
    }

    for (int i = 0; i < sample_count; ++i)
    {
        const int s = samples[i];
        const int a = s < 0 ? -s : s;
        const double fs = (double) s / 32768.0;

        if (a > peak_abs)
        {
            peak_abs = a;
        }

        sum_sq += fs * fs;
    }

    rms_level = (float) sqrt(sum_sq / (double) sample_count);
    peak = (float) peak_abs / 32768.0f;
    current = (float) SDL_AtomicGet(&music_auto_gain_q12) / AUTO_GAIN_Q12_ONE;
    total_frames += (uint64_t) sample_count / (uint64_t) music_auto_gain_channels;

    if (!level_initialized)
    {
        momentary_level = rms_level;
        shortterm_level = rms_level;
        global_level = rms_level;
        level_initialized = 1;
    }
    else
    {
        momentary_level += (rms_level - momentary_level) * AUTO_GAIN_MOMENTARY_BLEND;
        shortterm_level += (rms_level - shortterm_level) * AUTO_GAIN_SHORTTERM_BLEND;
        global_level += (rms_level - global_level) * AUTO_GAIN_GLOBAL_BLEND;
    }

    desired = current;

    if (shortterm_level < AUTO_GAIN_SILENCE_LEVEL
     && global_level < AUTO_GAIN_SILENCE_LEVEL)
    {
        desired = 1.0f;
    }
    else
    {
        const float momentary_db = I_MusicAutoGainLinToDb(momentary_level);
        const float shortterm_db = I_MusicAutoGainLinToDb(shortterm_level);
        const float global_db = I_MusicAutoGainLinToDb(global_level);
        const float relative_db = global_db - 10.0f;
        const float loudness_db =
            (AUTO_GAIN_WEIGHT_M * momentary_db
          +  AUTO_GAIN_WEIGHT_S * shortterm_db
          +  AUTO_GAIN_WEIGHT_G * global_db)
          / (AUTO_GAIN_WEIGHT_M + AUTO_GAIN_WEIGHT_S + AUTO_GAIN_WEIGHT_G);

        if (total_frames
            < (uint64_t) (music_auto_gain_sample_rate * AUTO_GAIN_FASTSTART_SECONDS)
         && momentary_db > AUTO_GAIN_FLOOR_DB)
        {
            desired = DB_TO_GAIN(AUTO_GAIN_TARGET_DB - momentary_db);
        }

        if (momentary_db > relative_db && momentary_db > AUTO_GAIN_FLOOR_DB)
        {
            const float gain = DB_TO_GAIN(AUTO_GAIN_TARGET_DB - loudness_db);
            if (peak >= 0.00001f && gain * peak < 1.0f)
            {
                desired = gain;
            }
        }
    }

    if (desired < AUTO_GAIN_MIN_FACTOR)
    {
        desired = AUTO_GAIN_MIN_FACTOR;
    }
    if (desired > AUTO_GAIN_MAX_FACTOR)
    {
        desired = AUTO_GAIN_MAX_FACTOR;
    }

    if (peak > 0.0001f && desired * peak > AUTO_GAIN_CLIP_LIMIT)
    {
        desired = AUTO_GAIN_CLIP_LIMIT / peak;
    }

    fast_samples_left = SDL_AtomicGet(&music_auto_gain_fast_samples_left);

    if (fast_samples_left > 0)
    {
        if (desired < current)
        {
            current += (desired - current) * AUTO_GAIN_FAST_ATTACK;
        }
        else
        {
            current += (desired - current) * AUTO_GAIN_FAST_RELEASE;
        }

        fast_samples_left -= sample_count;
        if (fast_samples_left < 0)
        {
            fast_samples_left = 0;
        }
        SDL_AtomicSet(&music_auto_gain_fast_samples_left, fast_samples_left);
    }
    else
    {
        if (desired < current)
        {
            current += (desired - current) * AUTO_GAIN_ATTACK;
        }
        else
        {
            current += (desired - current) * AUTO_GAIN_RELEASE;
        }
    }

    // Hard limiter guard to avoid clipping spikes.
    if (peak > 0.0001f && current * peak > AUTO_GAIN_CLIP_LIMIT)
    {
        current = AUTO_GAIN_CLIP_LIMIT / peak;
    }

    if (current < AUTO_GAIN_MIN_FACTOR)
    {
        current = AUTO_GAIN_MIN_FACTOR;
    }
    if (current > AUTO_GAIN_MAX_FACTOR)
    {
        current = AUTO_GAIN_MAX_FACTOR;
    }

    SDL_AtomicSet(&music_auto_gain_q12, (int) (current * AUTO_GAIN_Q12_ONE + 0.5f));
}

static void I_MusicAutoGainResetState(void)
{
    int freq = snd_samplerate;
    int channels = 2;
    Uint16 format = 0;
    int fast_samples = 0;
    boolean have_spec;

    have_spec = I_MusicAutoGainQuerySpec(&freq, &format, &channels);

    if (channels < 1)
    {
        channels = 2;
    }

    if (freq < 1)
    {
        freq = snd_samplerate;
    }

    music_auto_gain_channels = channels;
    music_auto_gain_sample_rate = freq;

    music_auto_gain_s16_output = have_spec
                              && (format == AUDIO_S16SYS
                               || format == AUDIO_S16LSB
                               || format == AUDIO_S16MSB);

    if (music_auto_gain_s16_output)
    {
        fast_samples = (int) (freq * channels * AUTO_GAIN_FASTSTART_SECONDS);
        if (fast_samples < 0)
        {
            fast_samples = 0;
        }
    }

    SDL_AtomicSet(&music_auto_gain_q12, AUTO_GAIN_Q12_ONE);
    SDL_AtomicSet(&music_auto_gain_reset_pending, 1);
    SDL_AtomicSet(&music_auto_gain_fast_samples_left, fast_samples);
}

static void I_MusicAutoGainEnsureHook(void)
{
    Uint16 format = 0;

    if (music_auto_gain_hook_registered
     || !I_MusicAutoGainQuerySpec(NULL, &format, NULL))
    {
        return;
    }

    music_auto_gain_s16_output = (format == AUDIO_S16SYS
                               || format == AUDIO_S16LSB
                               || format == AUDIO_S16MSB);

    if (!music_auto_gain_s16_output)
    {
        return;
    }

    Mix_SetPostMix(I_MusicAutoGainPostmix, NULL);
    music_auto_gain_hook_registered = true;
}

static void I_MusicAutoGainRemoveHook(void)
{
    if (music_auto_gain_hook_registered)
    {
        Mix_SetPostMix(NULL, NULL);
        music_auto_gain_hook_registered = false;
    }

    music_auto_gain_s16_output = false;
}

static int I_EffectiveMusicVolume(int base_volume)
{
    int scaled = base_volume;

    if (scaled < 0)
    {
        scaled = 0;
    }
    else if (scaled > 127)
    {
        scaled = 127;
    }

    if (snd_auto_gain && music_stream_active)
    {
        const int q12 = SDL_AtomicGet(&music_auto_gain_q12);
        scaled = (scaled * q12 + (AUTO_GAIN_Q12_ONE / 2)) / AUTO_GAIN_Q12_ONE;

        if (scaled < 0)
        {
            scaled = 0;
        }
        else if (scaled > 127)
        {
            scaled = 127;
        }
    }

    return scaled;
}
#else
static int music_base_volume = 127;
static int music_last_effective_volume = -1;
static boolean music_stream_active = false;

static int I_EffectiveMusicVolume(int base_volume)
{
    if (base_volume < 0)
    {
        return 0;
    }
    if (base_volume > 127)
    {
        return 127;
    }

    return base_volume;
}
#endif // DISABLE_SDL2MIXER

// Compiled-in sound modules:

static const sound_module_t *sound_modules[] =
{
#ifndef DISABLE_SDL2MIXER
    &sound_sdl_module,
#endif // DISABLE_SDL2MIXER
    &sound_pcsound_module,
    NULL,
};

// Compiled-in music modules:

static const music_module_t *music_modules[] =
{
#ifdef _WIN32
    &music_win_module,
#endif
#ifdef HAVE_FLUIDSYNTH
    &music_fl_module,
#endif // HAVE_FLUIDSYNTH
#ifndef DISABLE_SDL2MIXER
    &music_sdl_module,
#endif // DISABLE_SDL2MIXER
    &music_opl_module,
    NULL,
};

// Check if a sound device is in the given list of devices

static boolean SndDeviceInList(snddevice_t device, const snddevice_t *list,
                               int len)
{
    int i;

    for (i=0; i<len; ++i)
    {
        if (device == list[i])
        {
            return true;
        }
    }

    return false;
}

// Find and initialize a sound_module_t appropriate for the setting
// in snd_sfxdevice.

static void InitSfxModule(GameMission_t mission)
{
    int i;

    sound_module = NULL;

    for (i=0; sound_modules[i] != NULL; ++i)
    {
        // Is the sfx device in the list of devices supported by
        // this module?

        if (SndDeviceInList(snd_sfxdevice, 
                            sound_modules[i]->sound_devices,
                            sound_modules[i]->num_sound_devices))
        {
            // Initialize the module

            if (sound_modules[i]->Init(mission))
            {
                sound_module = sound_modules[i];
                return;
            }
        }
    }
}

// Initialize music according to snd_musicdevice.

static void InitMusicModule(void)
{
    int i;
    music_module = NULL;

    for (i=0; music_modules[i] != NULL; ++i)
    {
        // Is the music device in the list of devices supported
        // by this module?

        if (SndDeviceInList(snd_musicdevice, 
                            music_modules[i]->sound_devices,
                            music_modules[i]->num_sound_devices))
        {
        #ifdef _WIN32
            // Skip the native Windows MIDI module if using Timidity.

            if (strcmp(timidity_cfg_path, "") &&
                music_modules[i] == &music_win_module)
            {
                continue;
            }
        #endif

            // Initialize the module

            if (music_modules[i]->Init())
            {
                music_module = music_modules[i];

            #ifndef DISABLE_SDL2MIXER
                // [crispy] Always initialize SDL music module.
                if (music_module != &music_sdl_module)
                {
                    music_sdl_module.Init();
                }
            #endif

                return;
            }
        }
    }
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void I_InitSound(GameMission_t mission)
{
    boolean nosound, nosfx, nomusic, nomusicpacks;

    //!
    // @vanilla
    //
    // Disable all sound output.
    //

    nosound = M_CheckParm("-nosound") > 0;

    //!
    // @vanilla
    //
    // Disable sound effects. 
    //

    nosfx = M_CheckParm("-nosfx") > 0;

    //!
    // @vanilla
    //
    // Disable music.
    //

    nomusic = M_CheckParm("-nomusic") > 0;

    //!
    //
    // Disable substitution music packs.
    //

    nomusicpacks = M_ParmExists("-nomusicpacks");

    // Auto configure the music pack directory.
    M_SetMusicPackDir();

    // Initialize the sound and music subsystems.

    if (!nosound && !screensaver_mode)
    {
        // This is kind of a hack. If native MIDI is enabled, set up
        // the TIMIDITY_CFG environment variable here before SDL_mixer
        // is opened.

        if (!nomusic
         && (snd_musicdevice == SNDDEVICE_GENMIDI
          || snd_musicdevice == SNDDEVICE_GUS))
        {
            I_InitTimidityConfig();
        }

        if (!nosfx)
        {
            InitSfxModule(mission);
        }

        if (!nomusic)
        {
            InitMusicModule();
            active_music_module = music_module;
        }

        // We may also have substitute MIDIs we can load.
        if (!nomusicpacks && music_module != NULL)
        {
            music_packs_active = music_pack_module.Init();
        }
    }
    // [crispy] print the SDL audio backend
    {
	const char *driver_name = SDL_GetCurrentAudioDriver();

	fprintf(stderr, "I_InitSound: SDL audio driver is %s\n", driver_name ? driver_name : "none");
    }

    music_stream_active = false;
    music_last_effective_volume = -1;

#ifndef DISABLE_SDL2MIXER
    I_MusicAutoGainEnsureHook();
    I_MusicAutoGainResetState();
#endif
}

void I_ShutdownSound(void)
{
#ifndef DISABLE_SDL2MIXER
    I_MusicAutoGainRemoveHook();
    I_MusicAutoGainResetState();
#endif
    music_stream_active = false;
    music_last_effective_volume = -1;

    if (sound_module != NULL)
    {
        sound_module->Shutdown();
    }

    if (music_packs_active)
    {
        music_pack_module.Shutdown();
    }

#ifndef DISABLE_SDL2MIXER
    music_sdl_module.Shutdown();

    if (music_module == &music_sdl_module)
    {
        return;
    }
#endif

    if (music_module != NULL)
    {
        music_module->Shutdown();
    }
}

int I_GetSfxLumpNum(sfxinfo_t *sfxinfo)
{
    if (sound_module != NULL)
    {
        return sound_module->GetSfxLumpNum(sfxinfo);
    }
    else
    {
        return 0;
    }
}

void I_UpdateSound(void)
{
    if (sound_module != NULL)
    {
        sound_module->Update();
    }

    if (active_music_module != NULL && active_music_module->Poll != NULL)
    {
        active_music_module->Poll();
    }

    // [PN] Keep effective music volume in sync with runtime auto-gain factor.
    if (music_module != NULL)
    {
        int effective = I_EffectiveMusicVolume(music_base_volume);

        if (effective != music_last_effective_volume)
        {
            music_module->SetMusicVolume(effective);

#ifndef DISABLE_SDL2MIXER
            // [crispy] Always mirror volume to SDL mixer path.
            if (music_module != &music_sdl_module)
            {
                music_sdl_module.SetMusicVolume(effective);
            }
#endif

            music_last_effective_volume = effective;
        }
    }
}

static void CheckVolumeSeparation(int *vol, int *sep)
{
    if (*sep < 0)
    {
        *sep = 0;
    }
    else if (*sep > 254)
    {
        *sep = 254;
    }

    if (*vol < 0)
    {
        *vol = 0;
    }
    else if (*vol > 127)
    {
        *vol = 127;
    }
}

void I_UpdateSoundParams(int channel, int vol, int sep)
{
    if (sound_module != NULL)
    {
        CheckVolumeSeparation(&vol, &sep);
        sound_module->UpdateSoundParams(channel, vol, sep);
    }
}

int I_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch)
{
    if (sound_module != NULL)
    {
        CheckVolumeSeparation(&vol, &sep);
        return sound_module->StartSound(sfxinfo, channel, vol, sep, pitch);
    }
    else
    {
        return 0;
    }
}

void I_StopSound(int channel)
{
    if (sound_module != NULL)
    {
        sound_module->StopSound(channel);
    }
}

boolean I_SoundIsPlaying(int channel)
{
    if (sound_module != NULL)
    {
        return sound_module->SoundIsPlaying(channel);
    }
    else
    {
        return false;
    }
}

void I_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    if (sound_module != NULL && sound_module->CacheSounds != NULL)
    {
        sound_module->CacheSounds(sounds, num_sounds);
    }
}

void I_InitMusic(void)
{
}

void I_ShutdownMusic(void)
{

}

static void I_ApplyMusicVolume(void)
{
    int effective;

    if (music_module == NULL)
    {
        return;
    }

    effective = I_EffectiveMusicVolume(music_base_volume);

    if (effective == music_last_effective_volume)
    {
        return;
    }

    music_module->SetMusicVolume(effective);

#ifndef DISABLE_SDL2MIXER
    // [crispy] Always mirror volume to SDL mixer path.
    if (music_module != &music_sdl_module)
    {
        music_sdl_module.SetMusicVolume(effective);
    }
#endif

    music_last_effective_volume = effective;
}

void I_SetMusicVolume(int volume)
{
    music_base_volume = volume;

    if (music_base_volume < 0)
    {
        music_base_volume = 0;
    }
    else if (music_base_volume > 127)
    {
        music_base_volume = 127;
    }

    I_ApplyMusicVolume();
}

void I_PauseSong(void)
{
    if (active_music_module != NULL)
    {
        active_music_module->PauseMusic();
    }
}

void I_ResumeSong(void)
{
    if (active_music_module != NULL)
    {
        active_music_module->ResumeMusic();
    }
}

// Determine whether memory block is a .mid file

boolean IsMid(const byte *mem, int len)
{
    return len > 4 && !memcmp(mem, "MThd", 4);
}

// Determine whether memory block is a .mus file

boolean IsMus(const byte *mem, int len)
{
    return len > 4 && !memcmp(mem, "MUS\x1a", 4);
}

void *I_RegisterSong(void *data, int len)
{
    // If the music pack module is active, check to see if there is a
    // valid substitution for this track. If there is, we set the
    // active_music_module pointer to the music pack module for the
    // duration of this particular track.
    if (music_packs_active)
    {
        void *handle;

        handle = music_pack_module.RegisterSong(data, len);
        if (handle != NULL)
        {
            active_music_module = &music_pack_module;
            return handle;
        }
    }


    if (!IsMid(data, len) && !IsMus(data, len))
    {
#ifndef DISABLE_SDL2MIXER
        active_music_module = &music_sdl_module;
        return active_music_module->RegisterSong(data, len);
#else
        return NULL;
#endif
    }

    // No substitution for this track, so use the main module.
    active_music_module = music_module;
    if (active_music_module != NULL)
    {
        return active_music_module->RegisterSong(data, len);
    }
    else
    {
        return NULL;
    }
}

void I_UnRegisterSong(void *handle)
{
    if (active_music_module != NULL)
    {
        active_music_module->UnRegisterSong(handle);
    }

    music_stream_active = false;
#ifndef DISABLE_SDL2MIXER
    I_MusicAutoGainResetState();
#endif
    music_last_effective_volume = -1;
    I_ApplyMusicVolume();
}

void I_PlaySong(void *handle, boolean looping)
{
    if (active_music_module != NULL)
    {
        music_stream_active = (handle != NULL);
#ifndef DISABLE_SDL2MIXER
        I_MusicAutoGainEnsureHook();
        I_MusicAutoGainResetState();
#endif
        music_last_effective_volume = -1;
        I_ApplyMusicVolume();
        active_music_module->PlaySong(handle, looping);
    }
}

void I_StopSong(void)
{
    if (active_music_module != NULL)
    {
        active_music_module->StopSong();
    }

    music_stream_active = false;
#ifndef DISABLE_SDL2MIXER
    I_MusicAutoGainResetState();
#endif
    music_last_effective_volume = -1;
    I_ApplyMusicVolume();
}

boolean I_MusicIsPlaying(void)
{
    if (active_music_module != NULL)
    {
        return active_music_module->MusicIsPlaying();
    }
    else
    {
        return false;
    }
}

void I_BindSoundVariables(void)
{
    M_BindIntVariable("snd_sfxdevice",           &snd_sfxdevice);
    M_BindIntVariable("snd_musicdevice",         &snd_musicdevice);
    M_BindIntVariable("snd_auto_gain",           &snd_auto_gain);
    M_BindIntVariable("snd_maxslicetime_ms",     &snd_maxslicetime_ms);
    M_BindStringVariable("snd_musiccmd",         &snd_musiccmd);
    M_BindStringVariable("snd_dmxoption",        &snd_dmxoption);
    M_BindIntVariable("snd_samplerate",          &snd_samplerate);
    M_BindIntVariable("snd_cachesize",           &snd_cachesize);
    M_BindIntVariable("opl_io_port",             &opl_io_port);
    M_BindIntVariable("snd_pitchshift",          &snd_pitchshift);

    M_BindStringVariable("music_pack_path",      &music_pack_path);
    M_BindStringVariable("timidity_cfg_path",    &timidity_cfg_path);
    M_BindStringVariable("gus_patch_path",       &gus_patch_path);
    M_BindIntVariable("gus_ram_kb",              &gus_ram_kb);
#ifdef _WIN32
    M_BindStringVariable("winmm_midi_device",    &winmm_midi_device);
    M_BindIntVariable("winmm_complevel",         &winmm_complevel);
    M_BindIntVariable("winmm_reset_type",        &winmm_reset_type);
    M_BindIntVariable("winmm_reset_delay",       &winmm_reset_delay);
#endif

#ifdef HAVE_FLUIDSYNTH
    M_BindIntVariable("fsynth_chorus_active",       &fsynth_chorus_active);
    M_BindFloatVariable("fsynth_chorus_depth",      &fsynth_chorus_depth);
    M_BindFloatVariable("fsynth_chorus_level",      &fsynth_chorus_level);
    M_BindIntVariable("fsynth_chorus_nr",           &fsynth_chorus_nr);
    M_BindFloatVariable("fsynth_chorus_speed",      &fsynth_chorus_speed);
    M_BindStringVariable("fsynth_midibankselect",   &fsynth_midibankselect);
    M_BindIntVariable("fsynth_polyphony",           &fsynth_polyphony);
    M_BindIntVariable("fsynth_reverb_active",       &fsynth_reverb_active);
    M_BindFloatVariable("fsynth_reverb_damp",       &fsynth_reverb_damp);
    M_BindFloatVariable("fsynth_reverb_level",      &fsynth_reverb_level);
    M_BindFloatVariable("fsynth_reverb_roomsize",   &fsynth_reverb_roomsize);
    M_BindFloatVariable("fsynth_reverb_width",      &fsynth_reverb_width);
    M_BindFloatVariable("fsynth_gain",              &fsynth_gain);
    M_BindStringVariable("fsynth_sf_path",          &fsynth_sf_path);
#endif // HAVE_FLUIDSYNTH

    M_BindIntVariable("use_libsamplerate",       &use_libsamplerate);
    M_BindFloatVariable("libsamplerate_scale",   &libsamplerate_scale);
}

