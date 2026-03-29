//
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
// DESCRIPTION:
//       SDL Joystick code.
//


#include "SDL.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "doomtype.h"
#include "d_event.h"
#include "i_joystick.h"
#include "i_system.h"

#include "m_config.h"
#include "m_fixed.h"
#include "m_misc.h"

static SDL_GameController *gamepad = NULL;
static SDL_Joystick *joystick = NULL;
static char controller_name[128] = "NOT CONNECTED";

// Configuration variables:

// SDL_GameControllerType of gamepad
static int gamepad_type = 0;

// SDL GUID and index of the joystick to use.
static char *joystick_guid = "";
static int joystick_index = -1;

// Which joystick axis to use for horizontal movement, and whether to
// invert the direction:

int joystick_x_axis = 0;
int joystick_x_invert = 0;

// Which joystick axis to use for vertical movement, and whether to
// invert the direction:

int joystick_y_axis = 1;
int joystick_y_invert = 0;

// Which joystick axis to use for strafing?

int joystick_strafe_axis = -1;
int joystick_strafe_invert = 0;

// Which joystick axis to use for looking?

int joystick_look_axis = -1;
int joystick_look_invert = 0;

// Configurable dead zone for each axis, specified as a percentage of the axis
// max value.
int joystick_x_dead_zone = 33;
int joystick_y_dead_zone = 33;
int joystick_strafe_dead_zone = 33;
int joystick_look_dead_zone = 33;

int use_analog = 0;

int joystick_turn_sensitivity = 10;
int joystick_move_sensitivity = 10;
int joystick_look_sensitivity = 10;

// Virtual to physical button joystick button mapping. By default this
// is a straight mapping.
static int joystick_physical_buttons[NUM_VIRTUAL_BUTTONS] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
};

static void I_UpdateControllerName(void)
{
    const char *name = NULL;

    if (gamepad != NULL)
    {
        name = SDL_GameControllerName(gamepad);
    }
    else if (joystick != NULL)
    {
        name = SDL_JoystickName(joystick);
    }

    if (name == NULL || name[0] == '\0')
    {
        M_StringCopy(controller_name, "NOT CONNECTED", sizeof(controller_name));
    }
    else
    {
        M_StringCopy(controller_name, name, sizeof(controller_name));
    }
}

static void I_ShutdownGamepad(void)
{
    if (gamepad != NULL)
    {
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
        I_UpdateControllerName();
    }
}

static int FindFirstGamepad(void)
{
    int i;
    int gamepadindex = -1;

    for (i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            gamepadindex = i;
            break;
        }
    }

    return gamepadindex;
}

static int FindFirstJoystick(void)
{
    if (SDL_NumJoysticks() > 0)
    {
        return 0;
    }

    return -1;
}

static int FindSpecificGamepad(SDL_JoystickGUID guid)
{
    SDL_JoystickGUID dev_guid;
    int i;
    int gamepadindex = -1;

    for (i = 0; i < SDL_NumJoysticks(); ++i)
    {
        dev_guid = SDL_JoystickGetDeviceGUID(i);
        if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
        {
            gamepadindex = i;
            break;
        }
    }

    return gamepadindex;
}

static int DeviceIndexGamepad(void)
{
    SDL_JoystickGUID guid, dev_guid;
    int index = -1;

    if (strcmp(joystick_guid, ""))
    {
        guid = SDL_JoystickGetGUIDFromString(joystick_guid);

        // First, look for the gamepad at the previously-used index.
        if (joystick_index >= 0 && joystick_index < SDL_NumJoysticks())
        {
            dev_guid = SDL_JoystickGetDeviceGUID(joystick_index);
            if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
            {
                return joystick_index;
            }
        }

        // Maybe the index has moved?
        index = FindSpecificGamepad(guid);
    }

    // If the previous gamepad isn't present, see if a different one is
    // available.
    if (index < 0)
    {
        index = FindFirstGamepad();
    }

    return index;
}

static void I_InitGamepad(void)
{
    SDL_JoystickGUID guid;
    char guid_string[GUID_STRING_BUF_SIZE];
    int index;

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
    {
        return;
    }

    // [PN] Keep controller events enabled even when no device is currently open.
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);

    index = DeviceIndexGamepad();

    if (index < 0)
    {
        return;
    }

    gamepad = SDL_GameControllerOpen(index);

    if (gamepad == NULL)
    {
        printf("I_InitGamepad: Failed to open gamepad: %s\n", SDL_GetError());
        return;
    }

    joystick_index = index;
    gamepad_type = SDL_GameControllerTypeForIndex(index);

    guid = SDL_JoystickGetDeviceGUID(joystick_index);
    SDL_JoystickGetGUIDString(guid, guid_string, sizeof(guid_string));
    joystick_guid = M_StringDuplicate(guid_string);

    // GameController events do not fire if Joystick events are disabled.
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_GameControllerEventState(SDL_ENABLE);
    I_UpdateControllerName();

    printf("I_InitGamepad: %s\n", SDL_GameControllerName(gamepad));
    I_AtExit(I_ShutdownGamepad, true);
}

static int GetTriggerStateGamepad(SDL_GameControllerAxis trigger)
{
    return (SDL_GameControllerGetAxis(gamepad, trigger) > TRIGGER_THRESHOLD);
}

// Get the state of the given virtual button.

static int ReadButtonStateGamepad(int vbutton)
{
    int physbutton, state;

    // Map from virtual button to physical (SDL) button.
    if (vbutton < NUM_VIRTUAL_BUTTONS)
    {
        physbutton = joystick_physical_buttons[vbutton];
    }
    else
    {
        physbutton = vbutton;
    }

    switch (physbutton)
    {
        case GAMEPAD_BUTTON_TRIGGERLEFT:
            state = GetTriggerStateGamepad(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            break;

        case GAMEPAD_BUTTON_TRIGGERRIGHT:
            state = GetTriggerStateGamepad(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            break;

        default:
            state = SDL_GameControllerGetButton(gamepad, physbutton);
            break;
    }

    return state;
}

// Get a bitmask of all currently-pressed buttons

static int GetButtonsStateGamepad(void)
{
    int i;
    int result = 0;

    for (i = 0; i < MAX_VIRTUAL_BUTTONS; ++i)
    {
        if (ReadButtonStateGamepad(i))
        {
            result |= 1u << i;
        }
    }

    return result;
}

// Read the state of an axis, inverting if necessary.

static int GetAxisStateGamepad(int axis, int invert, int dead_zone)
{
    int result;

    // Axis -1 means disabled.

    if (axis < 0)
    {
        return 0;
    }

    // Dead zone is expressed as percentage of axis max value
    dead_zone = 32768 * dead_zone / 100;

    result = SDL_GameControllerGetAxis(gamepad, axis);

    if (result < dead_zone && result > -dead_zone)
    {
        return 0;
    }

    if (invert)
    {
        result = -result;
    }

    result *= FRACUNIT / 32768; // Want FXP number between -1 and 1

    return result;
}

static void I_UpdateGamepad(void)
{
    if (gamepad != NULL)
    {
        event_t ev;

        ev.type = ev_joystick;
        ev.data1 = GetButtonsStateGamepad();
        ev.data2 = GetAxisStateGamepad(joystick_x_axis, joystick_x_invert,
                                       joystick_x_dead_zone);
        ev.data3 = GetAxisStateGamepad(joystick_y_axis, joystick_y_invert,
                                       joystick_y_dead_zone);
        ev.data4 =
            GetAxisStateGamepad(joystick_strafe_axis, joystick_strafe_invert,
                                joystick_strafe_dead_zone);
        ev.data5 = GetAxisStateGamepad(joystick_look_axis, joystick_look_invert,
                                       joystick_look_dead_zone);

        D_PostEvent(&ev);
    }
}

void I_ShutdownJoystick(void)
{
    if (joystick != NULL)
    {
        SDL_JoystickClose(joystick);
        joystick = NULL;
        I_UpdateControllerName();
    }
}

static boolean IsValidAxis(int axis)
{
    int num_axes;

    if (axis < 0)
    {
        return true;
    }

    if (IS_BUTTON_AXIS(axis))
    {
        return true;
    }

    if (IS_HAT_AXIS(axis))
    {
        return HAT_AXIS_HAT(axis) < SDL_JoystickNumHats(joystick);
    }

    num_axes = SDL_JoystickNumAxes(joystick);

    return axis < num_axes;
}

static int DeviceIndex(void)
{
    SDL_JoystickGUID guid, dev_guid;
    int i;

    // [PN] No stored GUID yet: pick the previous index if still valid,
    // otherwise auto-detect the first available joystick.
    if (!strcmp(joystick_guid, ""))
    {
        if (joystick_index >= 0 && joystick_index < SDL_NumJoysticks())
        {
            return joystick_index;
        }

        return FindFirstJoystick();
    }

    guid = SDL_JoystickGetGUIDFromString(joystick_guid);

    // GUID identifies a class of device rather than a specific device.
    // Check if joystick_index has the expected GUID, as this can act
    // as a tie-breaker in case there are multiple identical devices.
    if (joystick_index >= 0 && joystick_index < SDL_NumJoysticks())
    {
        dev_guid = SDL_JoystickGetDeviceGUID(joystick_index);
        if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
        {
            return joystick_index;
        }
    }

    // Check all devices to look for one with the expected GUID.
    for (i = 0; i < SDL_NumJoysticks(); ++i)
    {
        dev_guid = SDL_JoystickGetDeviceGUID(i);
        if (!memcmp(&guid, &dev_guid, sizeof(SDL_JoystickGUID)))
        {
            printf("I_InitJoystick: Joystick moved to index %d.\n", i);
            return i;
        }
    }

    // No joystick found with the expected GUID: auto-detect fallback.
    return FindFirstJoystick();
}

void I_InitJoystick(void)
{
    SDL_JoystickGUID guid;
    char guid_string[GUID_STRING_BUF_SIZE];
    int index;

    // [PN] Prefer SDL GameController API when possible.
    I_InitGamepad();
    if (gamepad != NULL)
    {
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
    {
        return;
    }

    // [PN] Keep joystick events enabled for hotplug detection.
    SDL_JoystickEventState(SDL_ENABLE);

    index = DeviceIndex();

    if (index < 0)
    {
        return;
    }

    // Open the joystick

    joystick = SDL_JoystickOpen(index);

    if (joystick == NULL)
    {
        printf("I_InitJoystick: Failed to open joystick #%i: %s\n", index,
               SDL_GetError());
        return;
    }

    if (!IsValidAxis(joystick_x_axis)
     || !IsValidAxis(joystick_y_axis)
     || !IsValidAxis(joystick_strafe_axis)
     || !IsValidAxis(joystick_look_axis))
    {
        printf("I_InitJoystick: Invalid joystick axis for configured joystick "
               "(run joystick setup again)\n");

        SDL_JoystickClose(joystick);
        joystick = NULL;
        I_UpdateControllerName();
        return;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    // Initialized okay!

    joystick_index = index;
    gamepad_type = 0;
    guid = SDL_JoystickGetDeviceGUID(joystick_index);
    SDL_JoystickGetGUIDString(guid, guid_string, sizeof(guid_string));
    joystick_guid = M_StringDuplicate(guid_string);
    I_UpdateControllerName();

    printf("I_InitJoystick: %s\n", SDL_JoystickName(joystick));

    I_AtExit(I_ShutdownJoystick, true);
}

static boolean IsAxisButton(int physbutton)
{
    if (IS_BUTTON_AXIS(joystick_x_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_x_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_x_axis))
        {
            return true;
        }
    }
    if (IS_BUTTON_AXIS(joystick_y_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_y_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_y_axis))
        {
            return true;
        }
    }
    if (IS_BUTTON_AXIS(joystick_strafe_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_strafe_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_strafe_axis))
        {
            return true;
        }
    }
    if (IS_BUTTON_AXIS(joystick_look_axis))
    {
        if (physbutton == BUTTON_AXIS_NEG(joystick_look_axis)
         || physbutton == BUTTON_AXIS_POS(joystick_look_axis))
        {
            return true;
        }
    }

    return false;
}

// Get the state of the given virtual button.

static int ReadButtonState(int vbutton)
{
    int physbutton;

    // Map from virtual button to physical (SDL) button.
    if (vbutton < NUM_VIRTUAL_BUTTONS)
    {
        physbutton = joystick_physical_buttons[vbutton];
    }
    else
    {
        physbutton = vbutton;
    }

    // Never read axis buttons as buttons.
    if (IsAxisButton(physbutton))
    {
        return 0;
    }

    return SDL_JoystickGetButton(joystick, physbutton);
}

// Get a bitmask of all currently-pressed buttons

static int GetButtonsState(void)
{
    int i;
    int result = 0;

    for (i = 0; i < MAX_VIRTUAL_BUTTONS; ++i)
    {
        if (ReadButtonState(i))
        {
            result |= 1 << i;
        }
    }

    return result;
}

// Read the state of an axis, inverting if necessary.

static int GetAxisState(int axis, int invert, int dead_zone)
{
    int result;

    // Axis -1 means disabled.

    if (axis < 0)
    {
        return 0;
    }

    // Is this a button axis, or a hat axis?
    // If so, we need to handle it specially.

    result = 0;

    if (IS_BUTTON_AXIS(axis))
    {
        if (SDL_JoystickGetButton(joystick, BUTTON_AXIS_NEG(axis)))
        {
            result -= 32767;
        }
        if (SDL_JoystickGetButton(joystick, BUTTON_AXIS_POS(axis)))
        {
            result += 32767;
        }
    }
    else if (IS_HAT_AXIS(axis))
    {
        int direction = HAT_AXIS_DIRECTION(axis);
        int hatval = SDL_JoystickGetHat(joystick, HAT_AXIS_HAT(axis));

        if (direction == HAT_AXIS_HORIZONTAL)
        {
            if ((hatval & SDL_HAT_LEFT) != 0)
            {
                result -= 32767;
            }
            else if ((hatval & SDL_HAT_RIGHT) != 0)
            {
                result += 32767;
            }
        }
        else if (direction == HAT_AXIS_VERTICAL)
        {
            if ((hatval & SDL_HAT_UP) != 0)
            {
                result -= 32767;
            }
            else if ((hatval & SDL_HAT_DOWN) != 0)
            {
                result += 32767;
            }
        }
    }
    else
    {
        result = SDL_JoystickGetAxis(joystick, axis);

        // Dead zone is expressed as percentage of axis max value
        dead_zone = 32768 * dead_zone / 100;

        if (result < dead_zone && result > -dead_zone)
        {
            result = 0;
        }
    }

    if (invert)
    {
        result = -result;
    }

    result *= FRACUNIT / 32768; // Want FXP number between -1 and 1

    return result;
}

void I_UpdateJoystick(void)
{
    if (gamepad != NULL)
    {
        I_UpdateGamepad();
        return;
    }

    if (joystick != NULL)
    {
        event_t ev;

        ev.type = ev_joystick;
        ev.data1 = GetButtonsState();
        ev.data2 = GetAxisState(joystick_x_axis, joystick_x_invert,
                                joystick_x_dead_zone);
        ev.data3 = GetAxisState(joystick_y_axis, joystick_y_invert,
                                joystick_y_dead_zone);
        ev.data4 = GetAxisState(joystick_strafe_axis, joystick_strafe_invert,
                                joystick_strafe_dead_zone);
        ev.data5 = GetAxisState(joystick_look_axis, joystick_look_invert,
                                joystick_look_dead_zone);

        D_PostEvent(&ev);
    }
}

void I_RefreshControllerState(void)
{
    // [PN] Drop stale handles when device was unplugged.
    if (gamepad != NULL && !SDL_GameControllerGetAttached(gamepad))
    {
        I_ShutdownGamepad();
    }

    if (joystick != NULL && !SDL_JoystickGetAttached(joystick))
    {
        I_ShutdownJoystick();
    }

    // [PN] If no active input device is present, try to auto-detect again.
    if (gamepad == NULL && joystick == NULL)
    {
        I_InitJoystick();
    }

    I_UpdateControllerName();
}

const char *I_GetControllerName(void)
{
    return controller_name;
}

int I_HasController(void)
{
    return gamepad != NULL || joystick != NULL;
}

void I_BindJoystickVariables(void)
{
    int i;

    M_BindIntVariable("gamepad_type",          &gamepad_type);
    M_BindStringVariable("joystick_guid",      &joystick_guid);
    M_BindIntVariable("joystick_index",        &joystick_index);
    M_BindIntVariable("joystick_x_axis",       &joystick_x_axis);
    M_BindIntVariable("joystick_y_axis",       &joystick_y_axis);
    M_BindIntVariable("joystick_strafe_axis",  &joystick_strafe_axis);
    M_BindIntVariable("joystick_x_invert",     &joystick_x_invert);
    M_BindIntVariable("joystick_y_invert",     &joystick_y_invert);
    M_BindIntVariable("joystick_strafe_invert",&joystick_strafe_invert);
    M_BindIntVariable("joystick_look_axis",    &joystick_look_axis);
    M_BindIntVariable("joystick_look_invert",  &joystick_look_invert);
    M_BindIntVariable("joystick_x_dead_zone", &joystick_x_dead_zone);
    M_BindIntVariable("joystick_y_dead_zone", &joystick_y_dead_zone);
    M_BindIntVariable("joystick_strafe_dead_zone", &joystick_strafe_dead_zone);
    M_BindIntVariable("joystick_look_dead_zone", &joystick_look_dead_zone);
    M_BindIntVariable("use_analog", &use_analog);
    M_BindIntVariable("joystick_turn_sensitivity", &joystick_turn_sensitivity);
    M_BindIntVariable("joystick_move_sensitivity", &joystick_move_sensitivity);
    M_BindIntVariable("joystick_look_sensitivity", &joystick_look_sensitivity);

    for (i = 0; i < NUM_VIRTUAL_BUTTONS; ++i)
    {
        char name[32];
        M_snprintf(name, sizeof(name), "joystick_physical_button%i", i);
        M_BindIntVariable(name, &joystick_physical_buttons[i]);
    }
}
