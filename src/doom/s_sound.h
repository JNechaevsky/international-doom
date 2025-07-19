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
//	The not so system specific sound interface.
//


#ifndef __S_SOUND__
#define __S_SOUND__

#include "sounds.h"

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void S_Init(int sfxVolume, int musicVolume);


// Shut down sound 

void S_Shutdown(void);



//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//

void S_Start(void);

//
// Start sound for thing at <origin>
//  using <sound_id> from sounds.h
//

void S_StartSound(void *origin_p, int sfx_id);
void S_StartSoundOnce(void *origin_p, int sfx_id);

// Stop sound for thing at <origin>
void S_StopSound(const mobj_t *origin);


// Start music using <music_id> from sounds.h
void S_StartMusic(int m_id);

// Start music using <music_id> from sounds.h,
//  and set whether looping
void S_ChangeMusic(int musicnum, int looping);

// query if music is playing
boolean S_MusicPlaying(void);

// Stops the music fer sure.
void S_StopMusic(void);

// Stop and resume music, during game PAUSE.
void S_PauseSound(void);
void S_ResumeSound(void);


//
// Updates music & sounds
//
void S_UpdateSounds(mobj_t *listener);

void S_SetMusicVolume(int volume);
void S_SetSfxVolume(int volume);

extern void S_ChangeSFXSystem (void);
extern void S_UpdateStereoSeparation (void);
extern void S_MuteUnmuteSound (boolean mute);

extern int current_mus_num;

// [JN] jff 3/17/98 holds last IDMUS number, or -1
extern int idmusnum;

// [JN] Remastered soundtrack (extras.wad).
extern void S_ID_Change_D1_IntermissionMusic (void);
extern void S_ID_Start_D2_TitleMusic (void);
extern void S_ID_Change_D2_IntermissionMusic (void);
extern void S_ID_Change_D2_ReadMusic (void);
extern void S_ID_Change_D2_CastMusic (void);
extern int  S_ID_Set_D1_RemasteredMusic (void);
extern int  S_ID_Set_D2_RemasteredMusic (void);
extern void S_ID_Generate_MusicName (char *namebuf, size_t bufsize, musicinfo_t *music);

#endif

