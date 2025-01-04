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
//	Fixed point arithemtics, implementation.
//


#ifndef __M_FIXED__
#define __M_FIXED__

#include <stdint.h>



//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS		16
#define FRACUNIT		(1<<FRACBITS)
#define FIXED2DOUBLE(x)		(x / (double)FRACUNIT)

typedef int fixed_t;

fixed_t FixedMul	(fixed_t a, fixed_t b);
fixed_t FixedDiv	(fixed_t a, fixed_t b);

inline static int64_t FixedMul64 (int64_t a, int64_t b)
{
    return (a * b >> FRACBITS);
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define BETWEEN(l, u, x) ((l) > (x) ? (l) : (x) > (u) ? (u) : (x))

inline static int DivRoundClosest(const int n, const int d)
{
    return ((n < 0) == (d < 0)) ? ((n + d / 2) / d) : ((n - d / 2) / d);
}


#endif
