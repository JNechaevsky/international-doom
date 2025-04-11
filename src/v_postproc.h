//
// Copyright(C) 2025 Polina "Aura" N.
// Copyright(C) 2025 Julia Nechaevskaya
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
//  Video post processing effect for software renderer.
//


#pragma once

#include "i_video.h"
#include "id_vars.h"

extern void V_PProc_SupersampledSmoothing (boolean st_background_on, int st_height);

extern boolean pproc_display_effects;
extern boolean pproc_plyrview_effects;
extern boolean V_PProc_EffectsActive (void);

extern void V_PProc_Display (void);
extern void V_PProc_PlayerView (void);
