//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2023 Julia Nechaevskaya
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
//	Refresh of things, i.e. objects represented by sprites.
//


#include <stdio.h>
#include <stdlib.h>

#include "deh_main.h"
#include "doomdef.h"
#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "p_local.h"
#include "r_local.h"
#include "doomstat.h"

#include "v_trans.h" // [crispy] colored blood sprites

#include "id_vars.h"
#include "id_func.h"


#define MINZ				(FRACUNIT*4)
#define BASEYCENTER			(ORIGHEIGHT/2)


static size_t num_vissprite, num_vissprite_alloc, num_vissprite_ptrs; // killough
static vissprite_t *vissprites, **vissprite_ptrs;                     // killough

typedef struct drawseg_xrange_item_s
{
    short x1, x2;
    drawseg_t *user;
} drawseg_xrange_item_t;

typedef struct drawsegs_xrange_s
{
    drawseg_xrange_item_t *items;
    int count;
} drawsegs_xrange_t;

#define DS_RANGES_COUNT 3
static drawsegs_xrange_t drawsegs_xranges[DS_RANGES_COUNT];
static drawseg_xrange_item_t *drawsegs_xrange;
static unsigned int drawsegs_xrange_size = 0;
static int drawsegs_xrange_count = 0;


//
// Sprite rotation 0 is facing the viewer,
//  rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//  which increases counter clockwise (protractor).
// There was a lot of stuff grabbed wrong, so I changed it...
//
fixed_t pspritescale;
fixed_t pspriteiscale;

static lighttable_t **spritelights;

// constant arrays used for psprite clipping and initializing clipping
int negonearray[MAXWIDTH];        // [JN] 32-bit integer math
int screenheightarray[MAXWIDTH];  // [JN] 32-bit integer math

// variables used to look up and range check thing_t sprites patches
spritedef_t *sprites;
int          numsprites;
static spriteframe_t  sprtemp[29];
static int            maxframe;
static const char    *spritename;


// -----------------------------------------------------------------------------
// R_InstallSpriteLump
// Local function for R_InitSprites.
// -----------------------------------------------------------------------------

static void R_InstallSpriteLump (int lump, unsigned frame, char rot, boolean flipped)
{
    int r;
    // [crispy] support 16 sprite rotations
    unsigned rotation = (rot >= 'A') ? rot - 'A' + 10 : (rot >= '0') ? rot - '0' : 17;

    if (frame >= 29 || rotation > 16) // [crispy] support 16 sprite rotations
    {
        I_Error("R_InstallSpriteLump: Bad frame characters in lump %i", lump);
    }

    if ((int)frame > maxframe)
    {
        maxframe = frame;
    }

    if (rotation == 0)
    {
        // the lump should be used for all rotations
        // [crispy] make non-fatal
        if (sprtemp[frame].rotate == false)
        {
            fprintf (stderr, "R_InitSprites: Sprite %s frame %c has "
                             "multip rot=0 lump\n", spritename, 'A'+frame);
        }

        // [crispy] make non-fatal
        if (sprtemp[frame].rotate == true)
        {
            fprintf (stderr, "R_InitSprites: Sprite %s frame %c has rotations "
		                      "and a rot=0 lump\n", spritename, 'A'+frame);
        }

        for (r = 0 ; r < 8 ; r++)
        {
            // [crispy] only if not yet substituted
            if (sprtemp[frame].lump[r] == -1)
            {
                sprtemp[frame].lump[r] = lump - firstspritelump;
                sprtemp[frame].flip[r] = (byte)flipped;
                sprtemp[frame].rotate = false;
            }
        }
        return;
    }

    // the lump is only used for one rotation
    if (sprtemp[frame].rotate == false)
    {
        I_Error ("R_InitSprites: Sprite %s frame %c has rotations "
                 "and a rot=0 lump", spritename, 'A'+frame);
    }

    sprtemp[frame].rotate = true;

    // make 0 based
    rotation--;

    if (sprtemp[frame].lump[rotation] != -1)
    {
        I_Error ("R_InitSprites: Sprite %s : %c : %c "
                 "has two lumps mapped to it",
                 spritename, 'A'+frame, '1'+rotation);
    }

    sprtemp[frame].lump[rotation] = lump - firstspritelump;
    sprtemp[frame].flip[rotation] = (byte)flipped;
}

// -----------------------------------------------------------------------------
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//  (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account
//  for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//  letter/number appended.
// The rotation character can be 0 to signify no rotations.
// -----------------------------------------------------------------------------

static void R_InitSpriteDefs(const char **namelist)
{ 
    const char **check;
    int   i;
    int   l;
    int   frame;
    int   rotation;
    int   start;
    int   end;
    int   patched;

    // count the number of sprite names
    check = namelist;

    while (*check != NULL)
    {
        check++;
    }

    numsprites = check-namelist;

    if (!numsprites)
    {
        return;
    }

    sprites = Z_Malloc(numsprites *sizeof(*sprites), PU_STATIC, NULL);

    start = firstspritelump - 1;
    end = lastspritelump + 1;

    // scan all the lump names for each of the names,
    //  noting the highest frame letter.
    // Just compare 4 characters as ints
    for (i = 0 ; i < numsprites ; i++)
    {
        spritename = DEH_String(namelist[i]);
        memset(sprtemp, -1, sizeof(sprtemp));

        maxframe = -1;

        // scan the lumps,
        //  filling in the frames for whatever is found
        for (l = start + 1 ; l < end ; l++)
        {
            if (!strncasecmp(lumpinfo[l]->name, spritename, 4))
            {
                frame = lumpinfo[l]->name[4] - 'A';
                rotation = lumpinfo[l]->name[5];

                if (modifiedgame)
                {
                    patched = W_GetNumForName (lumpinfo[l]->name);
                }
                else
                {
                    patched = l;
                }

                R_InstallSpriteLump (patched, frame, rotation, false);

                if (lumpinfo[l]->name[6])
                {
                    frame = lumpinfo[l]->name[6] - 'A';
                    rotation = lumpinfo[l]->name[7];
                    R_InstallSpriteLump (l, frame, rotation, true);
                }
            }
        }

        // check the frames that were found for completeness
        if (maxframe == -1)
        {
            sprites[i].numframes = 0;
            continue;
        }

        maxframe++;

        for (frame = 0 ; frame < maxframe ; frame++)
        {
            switch ((int)sprtemp[frame].rotate)
            {
                case -1:
                // no rotations were found for that frame at all
                // [crispy] make non-fatal
                fprintf (stderr, "R_InitSprites: No patches found "
                                 "for %s frame %c\n", spritename, frame+'A');
                break;

                case 0:
                // only the first rotation is needed
                break;

                case 1:
                // must have all 8 frames
                for (rotation=0 ; rotation<8 ; rotation++)
                {
                    if (sprtemp[frame].lump[rotation] == -1)
                    {
                        I_Error ("R_InitSprites: Sprite %s frame %c "
                            "is missing rotations",
                            spritename, frame+'A');
                    }
                }

                // [crispy] support 16 sprite rotations
                sprtemp[frame].rotate = 2;
                for ( ; rotation < 16 ; rotation++)
                {
                    if (sprtemp[frame].lump[rotation] == -1)
                    {
                        sprtemp[frame].rotate = 1;
                        break;
                    }
                }
                break;
            }
        }

        // allocate space for the frames present and copy sprtemp to it
        sprites[i].numframes = maxframe;
        sprites[i].spriteframes = Z_Malloc (maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
        memcpy (sprites[i].spriteframes, sprtemp, maxframe*sizeof(spriteframe_t));
    }
}

// -----------------------------------------------------------------------------
// R_InitSprites
// Called at program start.
// -----------------------------------------------------------------------------

void R_InitSprites (const char **namelist)
{
    for (int i = 0 ; i < SCREENWIDTH ; i++)
    {
        negonearray[i] = -1;
    }

    R_InitSpriteDefs (namelist);
}

// -----------------------------------------------------------------------------
// R_ClearSprites
// Called at frame start.
// -----------------------------------------------------------------------------

void R_ClearSprites (void)
{
    num_vissprite = 0;  // [JN] killough
}

// -----------------------------------------------------------------------------
// R_NewVisSprite
// -----------------------------------------------------------------------------

static vissprite_t *R_NewVisSprite (void)
{
    if (num_vissprite >= num_vissprite_alloc)   // [JN] killough
    {
        
        size_t num_vissprite_alloc_prev = num_vissprite_alloc;
        num_vissprite_alloc = num_vissprite_alloc ? num_vissprite_alloc*2 : 128;
        vissprites = I_Realloc(vissprites,num_vissprite_alloc*sizeof(*vissprites));

        // [JN] Andrey Budko: set all fields to zero
        memset(vissprites + num_vissprite_alloc_prev, 0,
        (num_vissprite_alloc - num_vissprite_alloc_prev)*sizeof(*vissprites));
    }

    return vissprites + num_vissprite++;
}

// -----------------------------------------------------------------------------
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored in posts/runs of opaque pixels.
// -----------------------------------------------------------------------------

int *mfloorclip;    // [JN] 32-bit integer math
int *mceilingclip;  // [JN] 32-bit integer math

fixed_t spryscale;
int64_t sprtopscreen; // [crispy] WiggleFix

void R_DrawMaskedColumn (column_t *column)
{
    int64_t topscreen;    // [crispy] WiggleFix
    int64_t bottomscreen; // [crispy] WiggleFix
    fixed_t basetexturemid;
    int     top = -1;

    basetexturemid = dc_texturemid;
    dc_texheight = 0;  // [crispy] Tutti-Frutti fix

    for ( ; column->topdelta != 0xff ; ) 
    {
        // [crispy] support for DeePsea tall patches
        if (column->topdelta <= top)
        {
            top += column->topdelta;
        }
        else
        {
            top = column->topdelta;
        }

        // calculate unclipped screen coordinates for post
        topscreen = sprtopscreen + spryscale*top;
        bottomscreen = topscreen + spryscale*column->length;

        dc_yl = (int)((topscreen+FRACUNIT-1)>>FRACBITS); // [crispy] WiggleFix
        dc_yh = (int)((bottomscreen-1)>>FRACBITS); // [crispy] WiggleFix

        if (dc_yh >= mfloorclip[dc_x])
        {
            dc_yh = mfloorclip[dc_x] - 1;
        }
        if (dc_yl <= mceilingclip[dc_x])
        {
            dc_yl = mceilingclip[dc_x] + 1;
        }

        if (dc_yl <= dc_yh)
        {
            dc_source = (byte *)column + 3;
            dc_texturemid = basetexturemid - (top<<FRACBITS);
    
            // Drawn by either R_DrawColumn or (SHADOW) R_DrawFuzzColumn.
            colfunc ();	
        }
        column = (column_t *)(  (byte *)column + column->length + 4);
    }

    dc_texturemid = basetexturemid;
}

// -----------------------------------------------------------------------------
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
// -----------------------------------------------------------------------------

static void R_DrawVisSprite (vissprite_t *vis)
{
    int       texturecolumn;
    fixed_t   frac;
    patch_t  *patch;
    column_t *column;

    patch = W_CacheLumpNum (vis->patch+firstspritelump, PU_CACHE);

    // [crispy] brightmaps for select sprites
    dc_colormap[0] = vis->colormap[0];
    dc_colormap[1] = vis->colormap[1];
    dc_brightmap = vis->brightmap;
    
    if (!dc_colormap[0])
    {
	// NULL colormap = shadow draw
	colfunc = fuzzcolfunc;
    }
    // [JN] Translucent fuzz.
    else if (vis->mobjflags & MF_SHADOW && vis_improved_fuzz == 2)
    {
	    if (vis->mobjflags & MF_TRANSLATION)
	    {
	        colfunc = transtlfuzzcolfunc;
	        dc_translation = translationtables - 256 +
	                         ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8));
	    }
	    else
	    {
	        colfunc = tlfuzzcolfunc;
	    }
    }
    else if (vis->mobjflags & MF_TRANSLATION)
    {
	colfunc = transcolfunc;
	dc_translation = translationtables - 256 +
	    ( (vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );
    }
    // [crispy] color-translated sprites (i.e. blood)
    else if (vis->translation)
    {
	colfunc = transcolfunc;
	dc_translation = vis->translation;
    }
    // [crispy] translucent sprites
    else if (vis_translucency && vis->mobjflags & MF_TRANSLUCENT)
    {
	    colfunc = tlcolfunc;
#ifdef CRISPY_TRUECOLOR
	blendfunc = vis->blendfunc;
#endif
    }
	
    dc_iscale = abs(vis->xiscale)>>detailshift;
    dc_texturemid = vis->texturemid;
    frac = vis->startfrac;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid,spryscale);
	
    for (dc_x=vis->x1 ; dc_x<=vis->x2 ; dc_x++, frac += vis->xiscale)
    {
	texturecolumn = frac>>FRACBITS;
#ifdef RANGECHECK
	if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
	    I_Error ("R_DrawSpriteRange: bad texturecolumn");
#endif
	column = (column_t *) ((byte *)patch +
			       LONG(patch->columnofs[texturecolumn]));
	R_DrawMaskedColumn (column);
    }

    colfunc = basecolfunc;
}



//
// R_ProjectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//
static void R_ProjectSprite (mobj_t* thing)
{
    fixed_t		tr_x;
    fixed_t		tr_y;
    
    fixed_t		gxt;
    fixed_t		gyt;
    fixed_t		gzt; // [JN] killough 3/27/98
    
    fixed_t		tx;
    fixed_t		tz;

    fixed_t		xscale;
    
    int			x1;
    int			x2;

    spritedef_t*	sprdef;
    spriteframe_t*	sprframe;
    int			lump;
    
    unsigned		rot;
    boolean		flip;
    
    int			index;

    vissprite_t*	vis;
    
    angle_t		ang;
    fixed_t		iscale;
    
    fixed_t             interpx;
    fixed_t             interpy;
    fixed_t             interpz;
    fixed_t             interpangle;

    // [AM] Interpolate between current and last position,
    //      if prudent.
    if (vid_uncapped_fps &&
        // Don't interpolate if the mobj did something
        // that would necessitate turning it off for a tic.
        thing->interp == true &&
        // Don't interpolate during a paused state.
        realleveltime > oldleveltime &&
        // [JN] Don't interpolate things while freeze mode.
        (!crl_freeze ||
        // [JN] ... Hovewer, interpolate player while freeze mode,
        // so their sprite won't get desynced with moving camera.
        (crl_freeze && thing->type == MT_PLAYER)))
    {
        interpx = thing->oldx + FixedMul(thing->x - thing->oldx, fractionaltic);
        interpy = thing->oldy + FixedMul(thing->y - thing->oldy, fractionaltic);
        interpz = thing->oldz + FixedMul(thing->z - thing->oldz, fractionaltic);
        interpangle = R_InterpolateAngle(thing->oldangle, thing->angle, fractionaltic);
    }
    else
    {
        interpx = thing->x;
        interpy = thing->y;
        interpz = thing->z;
        interpangle = thing->angle;
    }

    // [JN] Apply amplitude to floating powerups:
    if (phys_floating_powerups
    && (thing->type == MT_MEGA       // Megasphere
    ||  thing->type == MT_MISC12     // Supercharge
    ||  thing->type == MT_INV        // Invulnerability
    ||  thing->type == MT_INS))      // Partial invisibility
    {
        if (realleveltime > oldleveltime)
        {
            interpz = thing->old_float_z + FixedMul(thing->float_z - thing->old_float_z, fractionaltic);
        }
        else
        {
            interpz = thing->float_z;
        }
    }

    // transform the origin point
    tr_x = interpx - viewx;
    tr_y = interpy - viewy;
	
    gxt = FixedMul(tr_x,viewcos); 
    gyt = -FixedMul(tr_y,viewsin);
    
    tz = gxt-gyt; 

    // thing is behind view plane?
    if (tz < MINZ)
	return;
    
    xscale = FixedDiv(projection, tz);
	
    gxt = -FixedMul(tr_x,viewsin); 
    gyt = FixedMul(tr_y,viewcos); 
    tx = -(gyt+gxt); 

    // too far off the side?
    if (abs(tx)>(tz<<2))
	return;
    
    // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
    if ((unsigned int) thing->sprite >= (unsigned int) numsprites)
	I_Error ("R_ProjectSprite: invalid sprite number %i ",
		 thing->sprite);
#endif
    sprdef = &sprites[thing->sprite];
    // [crispy] the TNT1 sprite is not supposed to be rendered anyway
    if (!sprdef->numframes && thing->sprite == SPR_TNT1)
    {
	return;
    }
#ifdef RANGECHECK
    if ( (thing->frame&FF_FRAMEMASK) >= sprdef->numframes )
	I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
		 thing->sprite, thing->frame);
#endif
    sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK];

    if (sprframe->rotate)
    {
	// choose a different rotation based on player view
	ang = R_PointToAngle (interpx, interpy);
	// [crispy] now made non-fatal
	if (sprframe->rotate == -1)
	{
	    return;
	}
	else
	// [crispy] support 16 sprite rotations
	if (sprframe->rotate == 2)
	{
	    const unsigned rot2 = (ang-interpangle+(unsigned)(ANG45/4)*17);
	    rot = (rot2>>29) + ((rot2>>25)&8);
	}
	else
	{
	rot = (ang-interpangle+(unsigned)(ANG45/2)*9)>>29;
	}
	lump = sprframe->lump[rot];
	flip = (boolean)sprframe->flip[rot];
    }
    else
    {
	// use single rotation for all views
	lump = sprframe->lump[0];
	flip = (boolean)sprframe->flip[0];
    }

    // [crispy] randomly flip corpse, blood and death animation sprites
    if (vis_flip_corpses &&
        (thing->flags & MF_FLIPPABLE) &&
        !(thing->flags & MF_SHOOTABLE) &&
        (thing->health & 1))
    {
	flip = !flip;
    }
    
    // calculate edges of the shape
    // [crispy] fix sprite offsets for mirrored sprites
    tx -= flip ? spritewidth[lump] - spriteoffset[lump] : spriteoffset[lump];
    x1 = (centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS;

    // off the right side?
    if (x1 > viewwidth)
	return;
    
    tx +=  spritewidth[lump];
    x2 = ((centerxfrac + FixedMul (tx,xscale) ) >>FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
	return;
    
    // [JN] killough 4/9/98: clip things which are out of view due to height
    gzt = interpz + spritetopoffset[lump];
    if (interpz > viewz + FixedDiv(viewheight << FRACBITS, xscale) ||
        gzt < (int64_t)viewz - FixedDiv((viewheight << FRACBITS)-viewheight, xscale))
    {
	return;
    }

    // [JN] quickly reject sprites with bad x ranges
    if (x1 >= x2)
    {
	return;
    }

    // store information in a vissprite
    vis = R_NewVisSprite ();
    vis->translation = NULL; // [crispy] no color translation
    vis->mobjflags = thing->flags;
    vis->scale = xscale<<detailshift;
    vis->gx = interpx;
    vis->gy = interpy;
    vis->gz = interpz;
    vis->gzt = gzt; // [JN] killough 3/27/98
    vis->texturemid = gzt - viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;	
    iscale = FixedDiv (FRACUNIT, xscale);

    if (flip)
    {
	vis->startfrac = spritewidth[lump]-1;
	vis->xiscale = -iscale;
    }
    else
    {
	vis->startfrac = 0;
	vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
	vis->startfrac += vis->xiscale*(vis->x1-x1);
    vis->patch = lump;
    
    // get light level
    // [JN] Do not zero-out colormap for translucent fuzz.
    if (thing->flags & MF_SHADOW && vis_improved_fuzz < 2)
    {
	// shadow draw
	vis->colormap[0] = vis->colormap[1] = NULL;
    }
    else if (fixedcolormap)
    {
	// fixed map
	vis->colormap[0] = vis->colormap[1] = fixedcolormap;
    }
    else if (thing->frame & FF_FULLBRIGHT)
    {
	// full bright
	vis->colormap[0] = vis->colormap[1] = colormaps;
    }
    
    else
    {
	// diminished light
	index = (xscale / vid_hires) >> (LIGHTSCALESHIFT - detailshift);

	if (index >= MAXLIGHTSCALE) 
	    index = MAXLIGHTSCALE-1;

	// [crispy] brightmaps for select sprites
	vis->colormap[0] = spritelights[index];

    // [JN] Apply different types half-brights for certain objects.
    //  Not to be confused:
    //   * Semi-bright. Lits up brightmapped pixels with non-full power.
    //     If sector brightness is below ~48, no brighting will be applied.
    //   * Demi-bright. Lits up brightmapped pixels with full power,
    //     and non-brightmapped pixels with distance index miltipled by 2.
    //   * Hemi-bright. Lits up brightmapped pixels with full power,
    //     and non-brightmapped pixels with distance index miltipled by 4.
    //   * Just animated. Lits up all sprite pixels and applies animation.
        // Semi-brigths:
        if (thing->sprite == SPR_BON2   // Armor Bonus
        ||  thing->sprite == SPR_BAR1)  // Explosive Barrel
        {
            vis->colormap[1] = spritelights[MAXDIMINDEX];
        }
        // Demi-brigths:
        else
        if (thing->sprite == SPR_CAND   // Candestick
        ||  thing->sprite == SPR_CBRA   // Candelabra
        ||  thing->sprite == SPR_COLU   // Floor Lamp
        ||  thing->sprite == SPR_FCAN   // Flaming Barrel
        ||  thing->sprite == SPR_TLMP   // Tall Tech Lamp
        ||  thing->sprite == SPR_TLP2)  // Short Tech Lamp
        {
            const int demi_bright = MIN(index*2, MAXDIMINDEX);

            vis->colormap[0] = spritelights[demi_bright];

            // Animated brightmaps:
            if (thing->sprite == SPR_CAND   // Candestick
            ||  thing->sprite == SPR_CBRA)  // Candelabra
            {
                vis->colormap[1] = &colormaps[thing->bmap_flick*256];
            }
            else
            if (thing->sprite == SPR_FCAN)  // Flaming Barrel
            {
                vis->colormap[1] = &colormaps[thing->bmap_glow*256];
            }
            else
            {
                vis->colormap[1] = colormaps;
            }
        }
        // Hemi-brigths:
        else
        if (thing->sprite == SPR_TBLU   // Tall Blue Torch
        ||  thing->sprite == SPR_TGRN   // Tall Green Torch
        ||  thing->sprite == SPR_TRED)  // Tall Red Torch
        {
            const int hemi_bright = MIN(index*4, MAXDIMINDEX);

            vis->colormap[0] = spritelights[hemi_bright];
            vis->colormap[1] = &colormaps[thing->bmap_flick/3*256];
        }
        // Just animated:
        else
        if (thing->sprite == SPR_SMBT   // Short Blue Torch
        ||  thing->sprite == SPR_SMGT   // Short Green Torch
        ||  thing->sprite == SPR_SMRT)  // Short Red Torch
        {
            vis->colormap[0] = vis->colormap[1]
                             = &colormaps[thing->bmap_flick/4*256];
        }
        else
        if (thing->sprite == SPR_CEYE   // Evil Eye
        ||  thing->sprite == SPR_FSKU)  // Floating Skull Rock
        {
            vis->colormap[0] = vis->colormap[1]
                             = &colormaps[thing->bmap_glow*256];
        }
        else
        if (thing->sprite == SPR_POL3)  // Pile of Skulls and Candles
        {
            vis->colormap[0] = vis->colormap[1]
                             = &colormaps[thing->bmap_flick/3*256];
        }
        // Normal brightmap:
        else
        {
            vis->colormap[1] = colormaps;
        }
    }	
    vis->brightmap = R_BrightmapForSprite(thing->sprite);

    // [crispy] colored blood
    if (vis_colored_blood &&
        (thing->type == MT_BLOOD || thing->state - states == S_GIBS) &&
        thing->target)
    {
	// [crispy] Thorn Things in Hacx bleed green blood
	if (gamemission == pack_hacx)
	{
	    if (thing->target->type == MT_BABY)
	    {
		vis->translation = cr[CR_RED2GREEN];
	    }
	}
	else
	{
	    // [crispy] Barons of Hell and Hell Knights bleed green blood
	    if (thing->target->type == MT_BRUISER || thing->target->type == MT_KNIGHT)
	    {
		vis->translation = cr[CR_RED2GREEN];
	    }
	    else
	    // [crispy] Cacodemons bleed blue blood
	    if (thing->target->type == MT_HEAD)
	    {
		vis->translation = cr[CR_RED2BLUE];
	    }
	}
    }

#ifdef CRISPY_TRUECOLOR
    // [crispy] translucent sprites
    // [JN] Draw full bright sprites with different functions, depending on user's choice.
    if (thing->flags & MF_TRANSLUCENT)
    {
	vis->blendfunc = (thing->frame & FF_FULLBRIGHT) ? I_BlendAddFunc : I_BlendOver;
    }
#endif
}




//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
void R_AddSprites (sector_t* sec)
{
    mobj_t*		thing;
    int			lightnum;

    lightnum = (sec->lightlevel >> LIGHTSEGSHIFT)+(extralight * LIGHTBRIGHT);

    if (lightnum < 0)		
	spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
	spritelights = scalelight[LIGHTLEVELS-1];
    else
	spritelights = scalelight[lightnum];

    // Handle all things in sector.
    for (thing = sec->thinglist ; thing ; thing = thing->snext)
	R_ProjectSprite (thing);
}

// -----------------------------------------------------------------------------
// R_ApplyWeaponBob
// [crispy] apply bobbing (or centering) to the player's weapon sprite
// -----------------------------------------------------------------------------

static inline void R_ApplyWeaponBob (fixed_t *sx, boolean bobx, fixed_t *sy, boolean boby)
{
    const angle_t angle = (128 * realleveltime) & FINEMASK;

    if (sx)
    {
        *sx = FRACUNIT;

        if (bobx)
        {
            *sx += FixedMul(viewplayer->bob, finecosine[angle]);
        }
    }

    if (sy)
    {
        *sy = 32 * FRACUNIT; // [crispy] WEAPONTOP

        if (boby)
        {
            *sy += FixedMul(viewplayer->bob, finesine[angle & (FINEANGLES / 2 - 1)]);
        }
    }
}

//
// R_DrawPSprite
//

boolean pspr_interp = true; // interpolate weapon bobbing

static void R_DrawPSprite (pspdef_t* psp)
{
    fixed_t		tx;
    int			x1;
    int			x2;
    spritedef_t*	sprdef;
    spriteframe_t*	sprframe;
    int			lump;
    boolean		flip;
    vissprite_t*	vis;
    vissprite_t		avis;

    fixed_t         psp_sx = psp->sx, psp_sy = psp->sy;    
    const int state = viewplayer->psprites[ps_weapon].state - states;       // [crispy]
    const weaponinfo_t *const winfo = &weaponinfo[viewplayer->readyweapon]; // [crispy]

    // decide which patch to use
#ifdef RANGECHECK
    if ( (unsigned)psp->state->sprite >= (unsigned int) numsprites)
	I_Error ("R_ProjectSprite: invalid sprite number %i ",
		 psp->state->sprite);
#endif
    sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
    if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
	I_Error ("R_ProjectSprite: invalid sprite frame %i : %i ",
		 psp->state->sprite, psp->state->frame);
#endif
    sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

    lump = sprframe->lump[0];
    flip = (boolean)sprframe->flip[0] ^ gp_flip_levels;
    
    // [JN] Weapon attack alignment. Common bobbing:
    if (phys_weapon_alignment)
    {
        // Apply full bobbing to all states, except raising and lowering.
        if (state != winfo->downstate && state != winfo->upstate)
        {
            if (phys_weapon_alignment == 2 && viewplayer->attackdown)
            {
                // Center weapon while firing.
                psp_sx = FRACUNIT;
                psp_sy = WEAPONTOP;
            }
            else
            {
                R_ApplyWeaponBob(&psp_sx, true, &psp_sy, true);
            }
        }
        else
        {
            // Apply x-only bobbing to raising and lowering states.
            R_ApplyWeaponBob(&psp_sx, true, 0, false);
        }

        // [crispy] squat down weapon sprite a bit after hitting the ground
        psp_sy += abs(viewplayer->psp_dy);
    }

    // calculate edges of the shape
    tx = psp_sx-(ORIGWIDTH/2)*FRACUNIT;
	
    // [crispy] fix sprite offsets for mirrored sprites
    tx -= flip ? 2 * tx - spriteoffset[lump] + spritewidth[lump] : spriteoffset[lump];
    x1 = (centerxfrac + FixedMul (tx,pspritescale) ) >>FRACBITS;

    // off the right side
    if (x1 > viewwidth)
	return;		

    tx +=  spritewidth[lump];
    x2 = ((centerxfrac + FixedMul (tx, pspritescale) ) >>FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
	return;
    
    // store information in a vissprite
    vis = &avis;
    vis->translation = NULL; // [crispy] no color translation
    vis->mobjflags = 0;
    // [crispy] weapons drawn 1 pixel too high when player is idle
    vis->texturemid = (BASEYCENTER<<FRACBITS)+FRACUNIT/5-(psp_sy-spritetopoffset[lump]);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;	
    vis->scale = pspritescale<<detailshift;
    
    if (flip)
    {
	vis->xiscale = -pspriteiscale;
	vis->startfrac = spritewidth[lump]-1;
    }
    else
    {
	vis->xiscale = pspriteiscale;
	vis->startfrac = 0;
    }
    
    if (vis->x1 > x1)
	vis->startfrac += vis->xiscale*(vis->x1-x1);

    vis->patch = lump;

    if (viewplayer->powers[pw_invisibility] > 4*32
	|| viewplayer->powers[pw_invisibility] & 8)
    {
	// shadow draw
	if (vis_improved_fuzz < 2)
	{
	    vis->colormap[0] = vis->colormap[1] = NULL;
	}
	else
	{
	    // [JN] Account invulnerability effect for translucent fuzz.
	    vis->colormap[0] = vis->colormap[1] = fixedcolormap ? 
                                              fixedcolormap : spritelights[MAXLIGHTSCALE-1];
	    vis->mobjflags |= MF_SHADOW;
	}
    }
    else if (fixedcolormap)
    {
	// fixed color
	vis->colormap[0] = vis->colormap[1] = fixedcolormap;
    }
    else if (psp->state->frame & FF_FULLBRIGHT)
    {
	// full bright
	vis->colormap[0] = vis->colormap[1] = colormaps;
    }
    else
    {
	// local light
	vis->colormap[0] = spritelights[MAXLIGHTSCALE-1];
	vis->colormap[1] = colormaps;
    }
    vis->brightmap = R_BrightmapForState(psp->state - states);
	
    // [crispy] interpolate weapon bobbing
    if (vid_uncapped_fps)
    {
        static int     oldx1, x1_saved;
        static fixed_t oldtexturemid, texturemid_saved;
        static int     oldlump = -1;
        static int     oldgametic = -1;

        if (oldgametic < gametic)
        {
            oldx1 = x1_saved;
            oldtexturemid = texturemid_saved;
            oldgametic = gametic;
        }

        x1_saved = vis->x1;
        texturemid_saved = vis->texturemid;

        if (lump == oldlump && pspr_interp)
        {
            int deltax = x2 - vis->x1;
            vis->x1 = oldx1 + FixedMul(vis->x1 - oldx1, fractionaltic);
            vis->x2 = vis->x1 + deltax;
            if (vis->x2 >= viewwidth)
            {
                vis->x2 = viewwidth - 1;
            }
            vis->texturemid = oldtexturemid + FixedMul(vis->texturemid - oldtexturemid, fractionaltic);
        }
        else
        {
            oldx1 = vis->x1;
            oldtexturemid = vis->texturemid;
            oldlump = lump;
            pspr_interp = true;
        }
    }

    // [crispy] free look
    vis->texturemid += FixedMul(((centery - viewheight / 2) << FRACBITS), pspriteiscale) >> detailshift;

    R_DrawVisSprite (vis);
}



//
// R_DrawPlayerSprites
//
static void R_DrawPlayerSprites (void)
{
    int		i;
    int		lightnum;
    pspdef_t*	psp;
    
    // Do not draw player gun sprite if spectating
    if (crl_spectating)
    	return;
    
    // get light level
    lightnum =
	(viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT) 
	+(extralight * LIGHTBRIGHT);

    if (lightnum < 0)		
	spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
	spritelights = scalelight[LIGHTLEVELS-1];
    else
	spritelights = scalelight[lightnum];
    
    // clip to screen bounds
    mfloorclip = screenheightarray;
    mceilingclip = negonearray;
    
    // add all active psprites
    for (i=0, psp=viewplayer->psprites;
	 i<NUMPSPRITES;
	 i++,psp++)
    {
	if (psp->state)
	    R_DrawPSprite (psp);
    }
}

// -----------------------------------------------------------------------------
// R_SortVisSprites
//
// Rewritten by Lee Killough to avoid using unnecessary
// linked lists, and to use faster sorting algorithm.
// -----------------------------------------------------------------------------

#define bcopyp(d, s, n) memcpy(d, s, (n) * sizeof(void *))

// killough 9/2/98: merge sort

static void msort(vissprite_t **s, vissprite_t **t, const int n)
{
    if (n >= 16)
    {
        int n1 = n/2, n2 = n - n1;
        vissprite_t **s1 = s, **s2 = s + n1, **d = t;

        msort(s1, t, n1);
        msort(s2, t, n2);

        while ((*s1)->scale >= (*s2)->scale ?
              (*d++ = *s1++, --n1) : (*d++ = *s2++, --n2));

        if (n2)
        bcopyp(d, s2, n2);
        else
        bcopyp(d, s1, n1);

        bcopyp(s, t, n);
    }
    else
    {
        int i;

        for (i = 1; i < n; i++)
        {
            vissprite_t *temp = s[i];

            if (s[i-1]->scale < temp->scale)
            {
                int j = i;

                while ((s[j] = s[j-1])->scale < temp->scale && --j);
                s[j] = temp;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// R_SortVisSprites
// -----------------------------------------------------------------------------

static void R_SortVisSprites (void)
{
    if (num_vissprite)
    {
        int i = num_vissprite;

        // If we need to allocate more pointers for the vissprites,
        // allocate as many as were allocated for sprites -- killough
        // killough 9/22/98: allocate twice as many

        if (num_vissprite_ptrs < num_vissprite*2)
        {
            free(vissprite_ptrs);  // better than realloc -- no preserving needed
            vissprite_ptrs = malloc((num_vissprite_ptrs = num_vissprite_alloc*2)
                                    * sizeof *vissprite_ptrs);
        }

        // Sprites of equal distance need to be sorted in inverse order.
        // This is most easily achieved by filling the sort array
        // backwards before the sort.

        while (--i >= 0)
        {
            vissprite_ptrs[num_vissprite-i-1] = vissprites+i;
        }

        // killough 9/22/98: replace qsort with merge sort, since the keys
        // are roughly in order to begin with, due to BSP rendering.

        msort(vissprite_ptrs, vissprite_ptrs + num_vissprite, num_vissprite);
    }
}



//
// R_DrawSprite
//
static void R_DrawSprite (vissprite_t* spr)
{
    drawseg_t*		ds;
    int			clipbot[MAXWIDTH];  // [JN] 32-bit integer math
    int			cliptop[MAXWIDTH];  // [JN] 32-bit integer math
    int			x;
    int			r1;
    int			r2;
    fixed_t		scale;
    fixed_t		lowscale;
    int			silhouette;
		
    for (x = spr->x1 ; x<=spr->x2 ; x++)
	clipbot[x] = cliptop[x] = -2;
    
    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds=ds_p-1 ; ds >= drawsegs ; ds--)
    {
	// determine if the drawseg obscures the sprite
	if (ds->x1 > spr->x2
	    || ds->x2 < spr->x1
	    || (!ds->silhouette
		&& !ds->maskedtexturecol) )
	{
	    // does not cover sprite
	    continue;
	}
			
	r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
	r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

	if (ds->scale1 > ds->scale2)
	{
	    lowscale = ds->scale2;
	    scale = ds->scale1;
	}
	else
	{
	    lowscale = ds->scale1;
	    scale = ds->scale2;
	}
		
	if (scale < spr->scale
	    || ( lowscale < spr->scale
		 && !R_PointOnSegSide (spr->gx, spr->gy, ds->curline) ) )
	{
	    // masked mid texture?
	    if (ds->maskedtexturecol)	
		R_RenderMaskedSegRange (ds, r1, r2);
	    // seg is behind sprite
	    continue;			
	}

	
	// clip this piece of the sprite
	silhouette = ds->silhouette;
	
	if (spr->gz >= ds->bsilheight)
	    silhouette &= ~SIL_BOTTOM;

	if (spr->gzt <= ds->tsilheight)
	    silhouette &= ~SIL_TOP;
			
	if (silhouette == 1)
	{
	    // bottom sil
	    for (x=r1 ; x<=r2 ; x++)
		if (clipbot[x] == -2)
		    clipbot[x] = ds->sprbottomclip[x];
	}
	else if (silhouette == 2)
	{
	    // top sil
	    for (x=r1 ; x<=r2 ; x++)
		if (cliptop[x] == -2)
		    cliptop[x] = ds->sprtopclip[x];
	}
	else if (silhouette == 3)
	{
	    // both
	    for (x=r1 ; x<=r2 ; x++)
	    {
		if (clipbot[x] == -2)
		    clipbot[x] = ds->sprbottomclip[x];
		if (cliptop[x] == -2)
		    cliptop[x] = ds->sprtopclip[x];
	    }
	}
		
    }
    
    // all clipping has been performed, so draw the sprite

    // check for unclipped columns
    for (x = spr->x1 ; x<=spr->x2 ; x++)
    {
	if (clipbot[x] == -2)		
	    clipbot[x] = viewheight;

	if (cliptop[x] == -2)
	    cliptop[x] = -1;
    }
		
    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite (spr);
}

// -------------------------------------------------------------------------
// R_DrawMasked
// -------------------------------------------------------------------------

void R_DrawMasked (void)
{
    int        i;
    drawseg_t *ds;

    R_SortVisSprites();

    // [JN] Andrey Budko
    // Makes sense for scenes with huge amount of drawsegs.
    // ~12% of speed improvement on epic.wad map05
    for (i = 0 ; i < DS_RANGES_COUNT ; i++)
    {
        drawsegs_xranges[i].count = 0;
    }

    if (num_vissprite > 0)
    {
        if (drawsegs_xrange_size < maxdrawsegs)
        {
            drawsegs_xrange_size = 2 * maxdrawsegs;

            for(i = 0; i < DS_RANGES_COUNT; i++)
            {
                drawsegs_xranges[i].items = I_Realloc(
                drawsegs_xranges[i].items,
                drawsegs_xrange_size * sizeof(drawsegs_xranges[i].items[0]));
            }
        }

        for (ds = ds_p; ds-- > drawsegs;)
        {
            if (ds->silhouette || ds->maskedtexturecol)
            {
                drawsegs_xranges[0].items[drawsegs_xranges[0].count].x1 = ds->x1;
                drawsegs_xranges[0].items[drawsegs_xranges[0].count].x2 = ds->x2;
                drawsegs_xranges[0].items[drawsegs_xranges[0].count].user = ds;

                // [JN] Andrey Budko: ~13% of speed improvement on sunder.wad map10
                if (ds->x1 < centerx)
                {
                    drawsegs_xranges[1].items[drawsegs_xranges[1].count] = 
                    drawsegs_xranges[0].items[drawsegs_xranges[0].count];
                    drawsegs_xranges[1].count++;
                }
                if (ds->x2 >= centerx)
                {
                    drawsegs_xranges[2].items[drawsegs_xranges[2].count] = 
                    drawsegs_xranges[0].items[drawsegs_xranges[0].count];
                    drawsegs_xranges[2].count++;
                }

                drawsegs_xranges[0].count++;
            }
        }
    }

    // draw all vissprites back to front

    IDRender.numsprites = num_vissprite;
    for (i = num_vissprite ; --i>=0 ; )
    {
        vissprite_t* spr = vissprite_ptrs[i];

        if (spr->x2 < centerx)
        {
            drawsegs_xrange = drawsegs_xranges[1].items;
            drawsegs_xrange_count = drawsegs_xranges[1].count;
        }
        else if (spr->x1 >= centerx)
        {
            drawsegs_xrange = drawsegs_xranges[2].items;
            drawsegs_xrange_count = drawsegs_xranges[2].count;
        }
        else
        {
            drawsegs_xrange = drawsegs_xranges[0].items;
            drawsegs_xrange_count = drawsegs_xranges[0].count;
        }

        R_DrawSprite(vissprite_ptrs[i]);    // [JN] killough
    }

    // render any remaining masked mid textures

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):
    for (ds = ds_p ; ds-- > drawsegs ; )
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange (ds, ds->x1, ds->x2);

    // draw the psprites on top of everything
    //  but does not draw on side views
    if (!viewangleoffset)
    {
        R_DrawPlayerSprites ();
    }
}
