//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2026 Polina "Aura" N.
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
//
// DESCRIPTION:  Asteroids-inspired mini-game inside Doom automap.
//


#include <stdlib.h>

#include "doomdef.h"
#include "am_map.h"
#include "m_controls.h"
#include "m_random.h"
#include "i_video.h"
#include "v_video.h"
#include "d_loop.h"
#include "am_oids.h"

#define AMO_MAX_BULLETS  24
#define AMO_MAX_ROCKS    24
#define AMO_POINTS       8

#define AMO_SHIP_RADIUS      (5 * FRACUNIT)
#define AMO_SHIP_THRUST      (FRACUNIT / 4)
#define AMO_SHIP_FRICTION    ((98 * FRACUNIT) / 100)
#define AMO_SHIP_MAX_SPEED   (4 * FRACUNIT)
#define AMO_SHIP_TURN_STEP   (ANG1 * 6)

#define AMO_BULLET_RADIUS    (FRACUNIT)
#define AMO_BULLET_SPEED     (6 * FRACUNIT)
#define AMO_BULLET_LIFETIME  (TICRATE)
#define AMO_FIRE_COOLDOWN    6

#define AMO_ROCK_RADIUS_BIG   (18 * FRACUNIT)
#define AMO_ROCK_RADIUS_MED   (12 * FRACUNIT)
#define AMO_ROCK_RADIUS_SMALL (8 * FRACUNIT)

typedef struct
{
    fixed_t oldx, oldy;
    fixed_t x, y;
    fixed_t vx, vy;
    angle_t oldangle;
    angle_t angle;
    int invuln_tics;
    boolean alive;
} amo_ship_t;

typedef struct
{
    fixed_t oldx, oldy;
    fixed_t x, y;
    fixed_t vx, vy;
    int ttl;
    boolean active;
} amo_bullet_t;

typedef struct
{
    fixed_t oldx, oldy;
    fixed_t x, y;
    fixed_t vx, vy;
    fixed_t radius;
    fixed_t shape[AMO_POINTS];
    angle_t oldangle;
    angle_t angle;
    int spin;
    int size;
    boolean active;
} amo_rock_t;

static fixed_t ff_x, ff_y, ff_w, ff_h;
static int ff_x_i, ff_y_i, ff_w_i, ff_h_i;

static boolean amo_active;
static boolean input_turn_left;
static boolean input_turn_right;
static boolean input_thrust;
static boolean input_reverse;
static boolean input_fire;
static boolean request_restart;

static int fire_cooldown;
static int wave;
static int lives;
static int score;
static boolean game_over;

static amo_ship_t ship;
static amo_bullet_t bullets[AMO_MAX_BULLETS];
static amo_rock_t rocks[AMO_MAX_ROCKS];

// -----------------------------------------------------------------------------
// AMO_RandRange
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static int AMO_RandRange(int minv, int maxv)
{
    return minv + (M_Random() % (maxv - minv + 1));
}

// -----------------------------------------------------------------------------
// AMO_RandSign
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static int AMO_RandSign(void)
{
    return (M_Random() & 1) ? 1 : -1;
}

// -----------------------------------------------------------------------------
// AMO_AngleFromDegrees
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static angle_t AMO_AngleFromDegrees(unsigned int degrees)
{
    // [PN] Use 64-bit math to avoid signed overflow in BAM conversion.
    return (angle_t) ((uint64_t) degrees * (uint64_t) ANG1);
}

// -----------------------------------------------------------------------------
// AMO_Cos
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static fixed_t AMO_Cos(angle_t a)
{
    return finecosine[a >> ANGLETOFINESHIFT];
}

// -----------------------------------------------------------------------------
// AMO_Sin
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static fixed_t AMO_Sin(angle_t a)
{
    return finesine[a >> ANGLETOFINESHIFT];
}

// -----------------------------------------------------------------------------
// AMO_WrapAxis
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static fixed_t AMO_WrapAxis(fixed_t value, fixed_t minv, fixed_t size)
{
    const fixed_t maxv = minv + size;

    if (size <= 0)
    {
        return minv;
    }

    // [PN] Normalize with modulo to avoid long/infinite wrapping loops.
    if (value < minv || value >= maxv)
    {
        int64_t delta = (int64_t) value - minv;
        const int64_t span = size;

        delta %= span;

        if (delta < 0)
        {
            delta += span;
        }

        value = minv + (fixed_t) delta;
    }

    return value;
}

// -----------------------------------------------------------------------------
// AMO_WrapPoint
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_WrapPoint(fixed_t *x, fixed_t *y)
{
    *x = AMO_WrapAxis(*x, ff_x, ff_w);
    *y = AMO_WrapAxis(*y, ff_y, ff_h);
}

// -----------------------------------------------------------------------------
// AMO_GetDrawFrac
// [PN] Returns render interpolation fraction for uncapped automap drawing.
// -----------------------------------------------------------------------------
static fixed_t AMO_GetDrawFrac(void)
{
    if (!vid_uncapped_fps || realleveltime <= oldleveltime)
    {
        return FRACUNIT;
    }

    return fractionaltic;
}

// -----------------------------------------------------------------------------
// AMO_LerpWrapAxis
// [PN] Interpolates axis on a torus, avoiding long cross-screen wrap lerps.
// -----------------------------------------------------------------------------
static fixed_t AMO_LerpWrapAxis(fixed_t oldv, fixed_t newv,
                                fixed_t minv, fixed_t size, fixed_t frac)
{
    fixed_t delta = newv - oldv;

    if (size > 0)
    {
        const fixed_t half = size >> 1;

        if (delta > half)
        {
            delta -= size;
        }
        else if (delta < -half)
        {
            delta += size;
        }
    }

    return AMO_WrapAxis(oldv + FixedMul(delta, frac), minv, size);
}

// -----------------------------------------------------------------------------
// AMO_LerpAngle
// [PN] Interpolates angle with wrap-safe signed delta.
// -----------------------------------------------------------------------------
static angle_t AMO_LerpAngle(angle_t olda, angle_t newa, fixed_t frac)
{
    const int32_t delta = (int32_t) (newa - olda);
    const int32_t step = (int32_t) (((int64_t) delta * frac) / FRACUNIT);
    return olda + (angle_t) step;
}

// -----------------------------------------------------------------------------
// AMO_AbsFixed
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static fixed_t AMO_AbsFixed(fixed_t v)
{
    return v < 0 ? -v : v;
}

// -----------------------------------------------------------------------------
// AMO_CircleOverlap
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static boolean AMO_CircleOverlap(fixed_t ax, fixed_t ay, fixed_t ar,
                                 fixed_t bx, fixed_t by, fixed_t br)
{
    const int64_t dx = (int64_t) ax - bx;
    const int64_t dy = (int64_t) ay - by;
    const int64_t rr = (int64_t) ar + br;

    return (dx * dx + dy * dy) <= (rr * rr);
}

// -----------------------------------------------------------------------------
// AMO_Plot
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_Plot(int x, int y, pixel_t color)
{
    if (x < ff_x_i || x >= ff_x_i + ff_w_i || y < ff_y_i || y >= ff_y_i + ff_h_i)
    {
        return;
    }

    I_VideoBuffer[y * SCREENWIDTH + x] = color;
}

// -----------------------------------------------------------------------------
// AMO_DrawLineFixedRaw
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_DrawLineFixedRaw(fixed_t x0, fixed_t y0,
                                 fixed_t x1, fixed_t y1,
                                 int color)
{
    const int ix0 = x0 >> FRACBITS;
    const int iy0 = y0 >> FRACBITS;
    const int ix1 = x1 >> FRACBITS;
    const int iy1 = y1 >> FRACBITS;
    fline_t fl;
    const int clip_left = ff_x_i - 1;
    const int clip_right = ff_x_i + ff_w_i;
    const int clip_top = ff_y_i - 1;
    const int clip_bottom = ff_y_i + ff_h_i;
    const int max_seg = (ff_w_i > ff_h_i ? ff_w_i : ff_h_i) / 2;

    // [PN] Fast reject when segment bbox is fully outside the playfield.
    if ((ix0 < clip_left && ix1 < clip_left)
     || (ix0 > clip_right && ix1 > clip_right)
     || (iy0 < clip_top && iy1 < clip_top)
     || (iy0 > clip_bottom && iy1 > clip_bottom))
    {
        return;
    }

    // [PN] Asteroids geometry should never generate map-wide segments.
    // Rejecting those prevents runaway diagonal artifacts from bad vertices.
    if (abs(ix1 - ix0) > max_seg || abs(iy1 - iy0) > max_seg)
    {
        return;
    }

    // [PN] AM_drawFline uses local automap coordinates.
    fl.a.x = ix0 - ff_x_i;
    fl.a.y = iy0 - ff_y_i;
    fl.b.x = ix1 - ff_x_i;
    fl.b.y = iy1 - ff_y_i;

    // [PN] Keep endpoints inside framebuffer bounds expected by AM_drawFline.
    if ((unsigned int) fl.a.x >= (unsigned int) ff_w_i
    ||  (unsigned int) fl.a.y >= (unsigned int) ff_h_i
    ||  (unsigned int) fl.b.x >= (unsigned int) ff_w_i
    ||  (unsigned int) fl.b.y >= (unsigned int) ff_h_i)
    {
        return;
    }

    AM_drawFline(&fl, color);
}

// -----------------------------------------------------------------------------
// AMO_ClearBullets
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_ClearBullets(void)
{
    for (int i = 0; i < AMO_MAX_BULLETS; ++i)
    {
        bullets[i].active = false;
    }
}

// -----------------------------------------------------------------------------
// AMO_ClearRocks
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_ClearRocks(void)
{
    for (int i = 0; i < AMO_MAX_ROCKS; ++i)
    {
        rocks[i].active = false;
    }
}

// -----------------------------------------------------------------------------
// AMO_RockRadiusBySize
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static fixed_t AMO_RockRadiusBySize(int size)
{
    if (size >= 3)
    {
        return AMO_ROCK_RADIUS_BIG;
    }

    if (size == 2)
    {
        return AMO_ROCK_RADIUS_MED;
    }

    return AMO_ROCK_RADIUS_SMALL;
}

static amo_rock_t *AMO_AllocRock(void)
{
    for (int i = 0; i < AMO_MAX_ROCKS; ++i)
    {
        if (!rocks[i].active)
        {
            return &rocks[i];
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// AMO_InitRock
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_InitRock(amo_rock_t *rock, int size,
                         fixed_t x, fixed_t y,
                         fixed_t vx, fixed_t vy)
{
    rock->active = true;
    rock->size = size;
    rock->oldx = x;
    rock->oldy = y;
    rock->x = x;
    rock->y = y;
    rock->vx = vx;
    rock->vy = vy;
    rock->radius = AMO_RockRadiusBySize(size);
    rock->oldangle = (angle_t) (M_Random() << 24);
    rock->angle = rock->oldangle;
    rock->spin = AMO_RandSign() * (int) (ANG1 * AMO_RandRange(1, 3));

    for (int i = 0; i < AMO_POINTS; ++i)
    {
        // [PN] Slightly irregular contour per rock so shapes are not identical.
        rock->shape[i] = (fixed_t) ((AMO_RandRange(80, 120) * FRACUNIT) / 100);
    }
}

// -----------------------------------------------------------------------------
// AMO_SpawnRockAt
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_SpawnRockAt(int size, fixed_t x, fixed_t y, fixed_t vx, fixed_t vy)
{
    amo_rock_t *rock = AMO_AllocRock();

    if (rock == NULL)
    {
        return;
    }

    AMO_InitRock(rock, size, x, y, vx, vy);
}

// -----------------------------------------------------------------------------
// AMO_SpawnRockAtEdge
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_SpawnRockAtEdge(int size)
{
    const int edge = AMO_RandRange(0, 3);
    const int min_x = ff_x_i;
    const int max_x = ff_x_i + ff_w_i - 1;
    const int min_y = ff_y_i;
    const int max_y = ff_y_i + ff_h_i - 1;
    fixed_t x;
    fixed_t y;

    if (edge == 0)
    {
        x = min_x << FRACBITS;
        y = AMO_RandRange(min_y, max_y) << FRACBITS;
    }
    else if (edge == 1)
    {
        x = max_x << FRACBITS;
        y = AMO_RandRange(min_y, max_y) << FRACBITS;
    }
    else if (edge == 2)
    {
        x = AMO_RandRange(min_x, max_x) << FRACBITS;
        y = min_y << FRACBITS;
    }
    else
    {
        x = AMO_RandRange(min_x, max_x) << FRACBITS;
        y = max_y << FRACBITS;
    }

    const fixed_t speed = (fixed_t) ((AMO_RandRange(30, 120) * FRACUNIT) / 100);
    const fixed_t vx = (fixed_t) (AMO_RandSign() * speed);
    const fixed_t vy = (fixed_t) (AMO_RandSign() * speed);

    if (AMO_CircleOverlap(x, y, 24 * FRACUNIT, ship.x, ship.y, 28 * FRACUNIT))
    {
        x = AMO_WrapAxis(x + (ff_w >> 1), ff_x, ff_w);
        y = AMO_WrapAxis(y + (ff_h >> 1), ff_y, ff_h);
    }

    AMO_SpawnRockAt(size, x, y, vx, vy);
}

// -----------------------------------------------------------------------------
// AMO_SpawnWave
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_SpawnWave(void)
{
    int count = 4 + wave;

    if (count > AMO_MAX_ROCKS)
    {
        count = AMO_MAX_ROCKS;
    }

    for (int i = 0; i < count; ++i)
    {
        AMO_SpawnRockAtEdge(3);
    }
}

// -----------------------------------------------------------------------------
// AMO_ResetShip
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_ResetShip(void)
{
    ship.oldx = ff_x + (ff_w >> 1);
    ship.oldy = ff_y + (ff_h >> 1);
    ship.x = ff_x + (ff_w >> 1);
    ship.y = ff_y + (ff_h >> 1);
    ship.vx = 0;
    ship.vy = 0;
    ship.oldangle = ANG90;
    ship.angle = ANG90;
    ship.invuln_tics = TICRATE * 2;
    ship.alive = true;
}

// -----------------------------------------------------------------------------
// AMO_FireBullet
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_FireBullet(void)
{
    for (int i = 0; i < AMO_MAX_BULLETS; ++i)
    {
        if (!bullets[i].active)
        {
            const fixed_t dirx = AMO_Cos(ship.angle);
            const fixed_t diry = AMO_Sin(ship.angle);
            const fixed_t spawn_x = ship.x + FixedMul(dirx, AMO_SHIP_RADIUS);
            const fixed_t spawn_y = ship.y - FixedMul(diry, AMO_SHIP_RADIUS);

            bullets[i].active = true;
            bullets[i].ttl = AMO_BULLET_LIFETIME;
            bullets[i].oldx = spawn_x;
            bullets[i].oldy = spawn_y;
            bullets[i].x = spawn_x;
            bullets[i].y = spawn_y;
            bullets[i].vx = ship.vx + FixedMul(dirx, AMO_BULLET_SPEED);
            bullets[i].vy = ship.vy - FixedMul(diry, AMO_BULLET_SPEED);
            return;
        }
    }
}

// -----------------------------------------------------------------------------
// AMO_SplitRock
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_SplitRock(const amo_rock_t *rock)
{
    const int next_size = rock->size - 1;
    const fixed_t split_speed = (next_size == 2) ? (FRACUNIT) : ((3 * FRACUNIT) / 2);

    if (next_size <= 0)
    {
        return;
    }

    for (int i = 0; i < 2; ++i)
    {
        const angle_t dir = rock->angle
                          + (i ? ANG90 : ANG270)
                          + AMO_AngleFromDegrees((unsigned int) AMO_RandRange(0, 45));
        const fixed_t vx = rock->vx + FixedMul(AMO_Cos(dir), split_speed);
        const fixed_t vy = rock->vy - FixedMul(AMO_Sin(dir), split_speed);

        AMO_SpawnRockAt(next_size, rock->x, rock->y, vx, vy);
    }
}

// -----------------------------------------------------------------------------
// AMO_UpdateShip
// [PN] Updates ship steering, thrust, velocity, wrapping, and firing.
// -----------------------------------------------------------------------------
static void AMO_UpdateShip(void)
{
    if (!ship.alive)
    {
        return;
    }

    ship.oldx = ship.x;
    ship.oldy = ship.y;
    ship.oldangle = ship.angle;

    if (input_turn_left)
    {
        ship.angle += AMO_SHIP_TURN_STEP;
    }

    if (input_turn_right)
    {
        ship.angle -= AMO_SHIP_TURN_STEP;
    }

    if (input_thrust)
    {
        ship.vx += FixedMul(AMO_Cos(ship.angle), AMO_SHIP_THRUST);
        ship.vy -= FixedMul(AMO_Sin(ship.angle), AMO_SHIP_THRUST);
    }

    if (input_reverse)
    {
        ship.vx -= FixedMul(AMO_Cos(ship.angle), AMO_SHIP_THRUST);
        ship.vy += FixedMul(AMO_Sin(ship.angle), AMO_SHIP_THRUST);
    }

    ship.vx = FixedMul(ship.vx, AMO_SHIP_FRICTION);
    ship.vy = FixedMul(ship.vy, AMO_SHIP_FRICTION);

    if (AMO_AbsFixed(ship.vx) > AMO_SHIP_MAX_SPEED)
    {
        ship.vx = ship.vx < 0 ? -AMO_SHIP_MAX_SPEED : AMO_SHIP_MAX_SPEED;
    }

    if (AMO_AbsFixed(ship.vy) > AMO_SHIP_MAX_SPEED)
    {
        ship.vy = ship.vy < 0 ? -AMO_SHIP_MAX_SPEED : AMO_SHIP_MAX_SPEED;
    }

    ship.x += ship.vx;
    ship.y += ship.vy;
    AMO_WrapPoint(&ship.x, &ship.y);

    if (ship.invuln_tics > 0)
    {
        ship.invuln_tics--;
    }

    if (input_fire && fire_cooldown <= 0)
    {
        AMO_FireBullet();
        fire_cooldown = AMO_FIRE_COOLDOWN;
    }
}

// -----------------------------------------------------------------------------
// AMO_UpdateBullets
// [PN] Moves bullets, wraps them, and expires timed-out shots.
// -----------------------------------------------------------------------------
static void AMO_UpdateBullets(void)
{
    for (int i = 0; i < AMO_MAX_BULLETS; ++i)
    {
        amo_bullet_t *bullet = &bullets[i];

        if (!bullet->active)
        {
            continue;
        }

        bullet->oldx = bullet->x;
        bullet->oldy = bullet->y;
        bullet->x += bullet->vx;
        bullet->y += bullet->vy;
        AMO_WrapPoint(&bullet->x, &bullet->y);

        if (--bullet->ttl <= 0)
        {
            bullet->active = false;
        }
    }
}

// -----------------------------------------------------------------------------
// AMO_UpdateRocks
// [PN] Moves and rotates asteroid bodies with screen wrapping.
// -----------------------------------------------------------------------------
static void AMO_UpdateRocks(void)
{
    for (int i = 0; i < AMO_MAX_ROCKS; ++i)
    {
        amo_rock_t *rock = &rocks[i];

        if (!rock->active)
        {
            continue;
        }

        rock->oldx = rock->x;
        rock->oldy = rock->y;
        rock->oldangle = rock->angle;
        rock->x += rock->vx;
        rock->y += rock->vy;
        rock->angle += rock->spin;

        AMO_WrapPoint(&rock->x, &rock->y);
    }
}

// -----------------------------------------------------------------------------
// AMO_HandleBulletHits
// [PN] Resolves bullet-versus-rock collisions and rock splitting.
// -----------------------------------------------------------------------------
static void AMO_HandleBulletHits(void)
{
    for (int i = 0; i < AMO_MAX_BULLETS; ++i)
    {
        amo_bullet_t *bullet = &bullets[i];

        if (!bullet->active)
        {
            continue;
        }

        for (int j = 0; j < AMO_MAX_ROCKS; ++j)
        {
            amo_rock_t *rock = &rocks[j];

            if (!rock->active)
            {
                continue;
            }

            if (AMO_CircleOverlap(bullet->x, bullet->y, AMO_BULLET_RADIUS,
                                  rock->x, rock->y, rock->radius))
            {
                bullet->active = false;
                rock->active = false;
                score += rock->size * 10;
                AMO_SplitRock(rock);
                break;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// AMO_HandleShipHit
// [PN] Checks ship collisions, applying lives and game-over state.
// -----------------------------------------------------------------------------
static void AMO_HandleShipHit(void)
{
    if (!ship.alive || ship.invuln_tics > 0)
    {
        return;
    }

    for (int i = 0; i < AMO_MAX_ROCKS; ++i)
    {
        amo_rock_t *rock = &rocks[i];

        if (!rock->active)
        {
            continue;
        }

        if (AMO_CircleOverlap(ship.x, ship.y, AMO_SHIP_RADIUS,
                              rock->x, rock->y, rock->radius))
        {
            lives--;

            if (lives > 0)
            {
                AMO_ResetShip();
            }
            else
            {
                ship.alive = false;
                game_over = true;
            }

            return;
        }
    }
}

// -----------------------------------------------------------------------------
// AMO_CountActiveRocks
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static int AMO_CountActiveRocks(void)
{
    int count = 0;

    for (int i = 0; i < AMO_MAX_ROCKS; ++i)
    {
        count += rocks[i].active ? 1 : 0;
    }

    return count;
}

void AM_OidsSetBounds(int x, int y, int w, int h)
{
    ff_x_i = x;
    ff_y_i = y;
    ff_w_i = w;
    ff_h_i = h;

    ff_x = x << FRACBITS;
    ff_y = y << FRACBITS;
    ff_w = w << FRACBITS;
    ff_h = h << FRACBITS;
}

void AM_OidsStart(void)
{
    if (ff_w_i <= 0 || ff_h_i <= 0)
    {
        return;
    }

    amo_active = true;
    input_turn_left = false;
    input_turn_right = false;
    input_thrust = false;
    input_reverse = false;
    input_fire = false;
    request_restart = false;
    fire_cooldown = 0;

    wave = 0;
    score = 0;
    lives = 3;
    game_over = false;

    AMO_ClearBullets();
    AMO_ClearRocks();
    AMO_ResetShip();
    AMO_SpawnWave();
}

void AM_OidsStop(void)
{
    amo_active = false;
    input_turn_left = false;
    input_turn_right = false;
    input_thrust = false;
    input_reverse = false;
    input_fire = false;
    request_restart = false;
}

boolean AM_OidsActive(void)
{
    return amo_active;
}

boolean AM_OidsResponder(const event_t *ev)
{
    if (!amo_active)
    {
        return false;
    }

    if (ev->type == ev_mouse)
    {
        input_fire = (ev->data1 & 1) != 0;
        return true;
    }

    if (ev->type != ev_keydown && ev->type != ev_keyup)
    {
        return false;
    }

    const int key = ev->data1;

    // [PN] Let automap toggle key close the map even in Asteroids mode.
    if (key == key_map_toggle || key == key_map_toggle2)
    {
        return false;
    }

    if (ev->type == ev_keydown)
    {
        if (key == key_map_west)
        {
            input_turn_left = true;
            return true;
        }
        if (key == key_map_east)
        {
            input_turn_right = true;
            return true;
        }
        if (key == key_map_north)
        {
            input_thrust = true;
            return true;
        }
        if (key == key_map_south)
        {
            input_reverse = true;
            return true;
        }
        if (key == key_fire || key == key_fire2
         || key == key_map_mark || key == key_map_mark2)
        {
            input_fire = true;
            return true;
        }
        if (key == key_map_grid || key == key_map_grid2)
        {
            request_restart = true;
            return true;
        }

        // [PN] Consume the rest of automap editing controls while game mode is active.
        if (key == key_map_zoomout || key == key_map_zoomout2
         || key == key_map_zoomin || key == key_map_zoomin2
         || key == key_map_maxzoom || key == key_map_maxzoom2
         || key == key_map_follow || key == key_map_follow2
         || key == key_map_rotate || key == key_map_rotate2
         || key == key_map_overlay || key == key_map_overlay2
         || key == key_map_mousepan || key == key_map_mousepan2
         || key == key_map_clearmark || key == key_map_clearmark2)
        {
            return true;
        }
    }
    else
    {
        if (key == key_map_west)
        {
            input_turn_left = false;
            return true;
        }
        if (key == key_map_east)
        {
            input_turn_right = false;
            return true;
        }
        if (key == key_map_north)
        {
            input_thrust = false;
            return true;
        }
        if (key == key_map_south)
        {
            input_reverse = false;
            return true;
        }
        if (key == key_fire || key == key_fire2
         || key == key_map_mark || key == key_map_mark2)
        {
            input_fire = false;
            return true;
        }

        if (key == key_map_zoomout || key == key_map_zoomout2
         || key == key_map_zoomin || key == key_map_zoomin2
         || key == key_map_maxzoom || key == key_map_maxzoom2
         || key == key_map_follow || key == key_map_follow2
         || key == key_map_rotate || key == key_map_rotate2
         || key == key_map_overlay || key == key_map_overlay2
         || key == key_map_mousepan || key == key_map_mousepan2
         || key == key_map_grid || key == key_map_grid2
         || key == key_map_clearmark || key == key_map_clearmark2)
        {
            return true;
        }
    }

    return false;
}

void AM_OidsTicker(void)
{
    if (!amo_active)
    {
        return;
    }

    if (request_restart)
    {
        AM_OidsStart();
        return;
    }

    if (fire_cooldown > 0)
    {
        fire_cooldown--;
    }

    if (game_over)
    {
        return;
    }

    AMO_UpdateShip();
    AMO_UpdateBullets();
    AMO_UpdateRocks();

    AMO_HandleBulletHits();
    AMO_HandleShipHit();

    if (!game_over && AMO_CountActiveRocks() == 0)
    {
        wave++;
        AMO_SpawnWave();
    }
}

// -----------------------------------------------------------------------------
// AMO_DrawShipInstance
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_DrawShipInstance(fixed_t sx, fixed_t sy, angle_t angle, int color)
{
    const fixed_t wing = (4 * FRACUNIT);

    if (!ship.alive)
    {
        return;
    }

    const fixed_t nose_x = sx + FixedMul(AMO_Cos(angle), AMO_SHIP_RADIUS);
    const fixed_t nose_y = sy - FixedMul(AMO_Sin(angle), AMO_SHIP_RADIUS);

    const fixed_t left_x = sx + FixedMul(AMO_Cos(angle + AMO_AngleFromDegrees(140)), wing);
    const fixed_t left_y = sy - FixedMul(AMO_Sin(angle + AMO_AngleFromDegrees(140)), wing);

    const fixed_t right_x = sx + FixedMul(AMO_Cos(angle + AMO_AngleFromDegrees(220)), wing);
    const fixed_t right_y = sy - FixedMul(AMO_Sin(angle + AMO_AngleFromDegrees(220)), wing);

    AMO_DrawLineFixedRaw(nose_x, nose_y, left_x, left_y, color);
    AMO_DrawLineFixedRaw(left_x, left_y, right_x, right_y, color);
    AMO_DrawLineFixedRaw(right_x, right_y, nose_x, nose_y, color);

    if (input_thrust)
    {
        fixed_t flame_x;
        fixed_t flame_y;

        flame_x = sx + FixedMul(AMO_Cos(angle + ANG180), (6 * FRACUNIT));
        flame_y = sy - FixedMul(AMO_Sin(angle + ANG180), (6 * FRACUNIT));

        AMO_DrawLineFixedRaw(sx, sy, flame_x, flame_y, color);
    }
}

// -----------------------------------------------------------------------------
// AMO_DrawBullets
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_DrawBullets(pixel_t color, fixed_t frac)
{
    for (int i = 0; i < AMO_MAX_BULLETS; ++i)
    {
        if (bullets[i].active)
        {
            const fixed_t fx = AMO_LerpWrapAxis(bullets[i].oldx, bullets[i].x, ff_x, ff_w, frac);
            const fixed_t fy = AMO_LerpWrapAxis(bullets[i].oldy, bullets[i].y, ff_y, ff_h, frac);
            const int x = fx >> FRACBITS;
            const int y = fy >> FRACBITS;

            AMO_Plot(x, y, color);
            AMO_Plot(x - 1, y, color);
            AMO_Plot(x + 1, y, color);
            AMO_Plot(x, y - 1, color);
            AMO_Plot(x, y + 1, color);
        }
    }
}

// -----------------------------------------------------------------------------
// AMO_DrawRockInstance
// [PN] Internal helper for Asteroids mode simulation and rendering.
// -----------------------------------------------------------------------------
static void AMO_DrawRockInstance(const amo_rock_t *rock,
                                 fixed_t rock_x, fixed_t rock_y, angle_t rock_angle,
                                 fixed_t off_x, fixed_t off_y,
                                 int color)
{
    fixed_t first_x = 0;
    fixed_t first_y = 0;
    fixed_t prev_x = 0;
    fixed_t prev_y = 0;

    for (int i = 0; i < AMO_POINTS; ++i)
    {
        const angle_t a = rock_angle + AMO_AngleFromDegrees((unsigned int) i * 45u);
        const fixed_t r = FixedMul(rock->radius, rock->shape[i]);
        const fixed_t x = rock_x + off_x + FixedMul(AMO_Cos(a), r);
        const fixed_t y = rock_y + off_y - FixedMul(AMO_Sin(a), r);

        if (i == 0)
        {
            first_x = x;
            first_y = y;
        }
        else
        {
            AMO_DrawLineFixedRaw(prev_x, prev_y, x, y, color);
        }

        prev_x = x;
        prev_y = y;
    }

    AMO_DrawLineFixedRaw(prev_x, prev_y, first_x, first_y, color);
}

// -----------------------------------------------------------------------------
// AMO_DrawShip
// [PN] Draws ship copies across wrapped tiles for seamless edges.
// -----------------------------------------------------------------------------
static void AMO_DrawShip(int color, fixed_t frac)
{
    const fixed_t ship_x = AMO_LerpWrapAxis(ship.oldx, ship.x, ff_x, ff_w, frac);
    const fixed_t ship_y = AMO_LerpWrapAxis(ship.oldy, ship.y, ff_y, ff_h, frac);
    const angle_t ship_angle = AMO_LerpAngle(ship.oldangle, ship.angle, frac);

    for (int oy = -1; oy <= 1; ++oy)
    {
        for (int ox = -1; ox <= 1; ++ox)
        {
            AMO_DrawShipInstance(ship_x + (fixed_t) (ox * ff_w),
                                 ship_y + (fixed_t) (oy * ff_h),
                                 ship_angle,
                                 color);
        }
    }
}

// -----------------------------------------------------------------------------
// AMO_DrawRock
// [PN] Draws rock copies across wrapped tiles for seamless edges.
// -----------------------------------------------------------------------------
static void AMO_DrawRock(const amo_rock_t *rock, int color, fixed_t frac)
{
    const fixed_t rock_x = AMO_LerpWrapAxis(rock->oldx, rock->x, ff_x, ff_w, frac);
    const fixed_t rock_y = AMO_LerpWrapAxis(rock->oldy, rock->y, ff_y, ff_h, frac);
    const angle_t rock_angle = AMO_LerpAngle(rock->oldangle, rock->angle, frac);

    for (int oy = -1; oy <= 1; ++oy)
    {
        for (int ox = -1; ox <= 1; ++ox)
        {
            AMO_DrawRockInstance(rock,
                                 rock_x, rock_y, rock_angle,
                                 (fixed_t) (ox * ff_w),
                                 (fixed_t) (oy * ff_h),
                                 color);
        }
    }
}

void AM_OidsDrawer(void)
{
    const int rock_color = 176;
    const int ship_color = 231;
    const pixel_t bullet_color = pal_color[252];
    // [PN] Freeze interpolation on game-over frame to keep the X marker stable.
    const fixed_t draw_frac = game_over ? FRACUNIT : AMO_GetDrawFrac();

    if (!amo_active)
    {
        return;
    }

    for (int i = 0; i < AMO_MAX_ROCKS; ++i)
    {
        if (rocks[i].active)
        {
            AMO_DrawRock(&rocks[i], rock_color, draw_frac);
        }
    }

    AMO_DrawBullets(bullet_color, draw_frac);
    AMO_DrawShip(ship_color, draw_frac);

    if (game_over)
    {
        const int cx = ff_x_i + (ff_w_i >> 1);
        const int cy = ff_y_i + (ff_h_i >> 1);

        AMO_DrawLineFixedRaw((cx - 10) << FRACBITS, (cy - 10) << FRACBITS,
                             (cx + 10) << FRACBITS, (cy + 10) << FRACBITS, 160);
        AMO_DrawLineFixedRaw((cx - 10) << FRACBITS, (cy + 10) << FRACBITS,
                             (cx + 10) << FRACBITS, (cy - 10) << FRACBITS, 160);
    }

    (void) score;
}
