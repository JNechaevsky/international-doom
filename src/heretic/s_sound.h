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

// soundst.h

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

extern int snd_MaxVolume;
extern int snd_MusicVolume;

extern int mus_song;
extern int idmusnum;
extern boolean mus_force_replay;

void S_Start(void);
void S_StartSound(void *_origin, int sound_id);
void S_StartSoundAmbient (void *_origin, int sound_id);
void S_StartSoundAtVolume(void *_origin, int sound_id, int volume);
void S_StopSound(void *_origin);
void S_PauseSound(void);
void S_ResumeSound(void);
void S_UpdateSounds(mobj_t * listener);
void S_StartSong(int song, boolean loop);
void S_Init(void);
void S_GetChannelInfo(SoundInfo_t * s);
void S_SetMaxVolume(void);
void S_SetMusicVolume(void);
void S_ShutDown(void);

extern void S_MuteUnmuteSound (boolean mute);

#endif
