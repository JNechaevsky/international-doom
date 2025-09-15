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
// DoomData.h

// all external data is defined here
// most of the data is loaded into different structures at run time

#ifndef __DOOMDATA__
#define __DOOMDATA__

#include "doomtype.h"

/*
===============================================================================

						map level types

===============================================================================
*/

// lump order in a map wad
enum
{
    ML_LABEL,
    ML_THINGS,
    ML_LINEDEFS,
    ML_SIDEDEFS,
    ML_VERTEXES,
    ML_SEGS,
    ML_SSECTORS,
    ML_NODES,
    ML_SECTORS,
    ML_REJECT,
    ML_BLOCKMAP
};


typedef PACKED_STRUCT (
{
    short x, y;
}) mapvertex_t;

typedef PACKED_STRUCT (
{
    short textureoffset;
    short rowoffset;
    char toptexture[8], bottomtexture[8], midtexture[8];
    short sector;               // on viewer's side
}) mapsidedef_t;

typedef PACKED_STRUCT (
{
    unsigned short v1, v2; // [crispy] extended nodes
    unsigned short flags; // [crispy] extended nodes
    short special, tag;
    // sidenum[1] will be NO_INDEX if one sided
    unsigned short sidenum[2]; // [crispy] extended nodes
}) maplinedef_t;

// [crispy] allow loading of Hexen-format maps
// taken from chocolate-doom/src/hexen/xddefs.h:63-75
typedef PACKED_STRUCT (
{
    short v1;
    short v2;
    short flags;
    byte special;
    byte arg1;
    byte arg2;
    byte arg3;
    byte arg4;
    byte arg5;
    short sidenum[2];
}) maplinedef_hexen_t;

#define	ML_BLOCKING			1
#define	ML_BLOCKMONSTERS	2
#define	ML_TWOSIDED			4       // backside will not be present at all
                                                                        // if not two sided

// if a texture is pegged, the texture will have the end exposed to air held
// constant at the top or bottom of the texture (stairs or pulled down things)
// and will move with a height change of one of the neighbor sectors
// Unpegged textures allways have the first row of the texture at the top
// pixel of the line for both top and bottom textures (windows)
#define	ML_DONTPEGTOP		8
#define	ML_DONTPEGBOTTOM	16

#define ML_SECRET			32      // don't map as two sided: IT'S A SECRET!
#define ML_SOUNDBLOCK		64      // don't let sound cross two of these
#define	ML_DONTDRAW			128     // don't draw on the automap
#define	ML_MAPPED			256     // set if allready drawn in automap


typedef PACKED_STRUCT (
{
    short floorheight, ceilingheight;
    char floorpic[8], ceilingpic[8];
    short lightlevel;
    short special, tag;
}) mapsector_t;

typedef PACKED_STRUCT (
{
    unsigned short numsegs; // [crispy] extended nodes
    // segs are stored sequentially
    unsigned short firstseg; // [crispy] extended nodes
}) mapsubsector_t;

// [crispy] allow loading of maps with DeePBSP nodes
// taken from prboom-plus/src/doomdata.h:163-166
typedef PACKED_STRUCT (
{
    unsigned short numsegs;
    int firstseg;
}) mapsubsector_deepbsp_t;

// [crispy] allow loading of maps with ZDBSP nodes
// taken from prboom-plus/src/doomdata.h:168-170
typedef PACKED_STRUCT (
{
    unsigned int numsegs;
}) mapsubsector_zdbsp_t;

typedef PACKED_STRUCT (
{
    unsigned short v1, v2; // [crispy] extended nodes
    short angle;
    unsigned short linedef; // [crispy] extended nodes
    short side;
    short offset;
}) mapseg_t;

// [crispy] allow loading of maps with DeePBSP nodes
// taken from prboom-plus/src/doomdata.h:183-190
typedef PACKED_STRUCT (
{
    int v1;
    int v2;
    unsigned short angle;
    unsigned short linedef;
    short side;
    unsigned short offset;
}) mapseg_deepbsp_t;

// [crispy] allow loading of maps with ZDBSP nodes
// taken from prboom-plus/src/doomdata.h:192-196
typedef PACKED_STRUCT (
{
    unsigned int v1, v2;
    unsigned short linedef;
    unsigned char side;
}) mapseg_zdbsp_t;

#define	NF_SUBSECTOR_VANILLA	0x8000
#define	NF_SUBSECTOR	0x80000000 // [crispy] extended nodes
#define	NO_INDEX	((unsigned short)-1) // [crispy] extended nodes
typedef PACKED_STRUCT (
{
    short x, y, dx, dy;         // partition line
    short bbox[2][4];           // bounding box for each child
    unsigned short children[2]; // if NF_SUBSECTOR its a subsector
}) mapnode_t;

// [crispy] allow loading of maps with DeePBSP nodes
// taken from prboom-plus/src/doomdata.h:216-225
typedef PACKED_STRUCT (
{
    short x;
    short y;
    short dx;
    short dy;
    short bbox[2][4];
    int children[2];
}) mapnode_deepbsp_t;

// [crispy] allow loading of maps with ZDBSP nodes
// taken from prboom-plus/src/doomdata.h:227-136
typedef PACKED_STRUCT (
{
    short x;
    short y;
    short dx;
    short dy;
    short bbox[2][4];
    int children[2];
}) mapnode_zdbsp_t;

typedef PACKED_STRUCT (
{
    short x, y;
    short angle;
    short type;
    short options;
}) mapthing_t;

// [crispy] allow loading of Hexen-format maps
// taken from chocolate-doom/src/hexen/xddefs.h:134-149
typedef PACKED_STRUCT (
{
    short tid;
    short x;
    short y;
    short height;
    short angle;
    short type;
    short options;
    byte special;
    byte arg1;
    byte arg2;
    byte arg3;
    byte arg4;
    byte arg5;
}) mapthing_hexen_t;

#define	MTF_EASY		1
#define	MTF_NORMAL		2
#define	MTF_HARD		4
#define	MTF_AMBUSH		8





#endif // __DOOMDATA__
