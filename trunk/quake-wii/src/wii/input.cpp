/*
Quake GameCube port.
Copyright (C) 2007 Peter Mackay

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

extern "C"
{
#include "../generic/quakedef.h"
}

#include <ogc/pad.h>
#include <wiiuse/wpad.h>

#define FORCE_KEY_BINDINGS 0

namespace quake
{
	namespace input
	{
		// wiimote info
		WPADData pad;

		// A map from button mask to Quake key.
		static const size_t	button_count	= sizeof(u16) * 8;
		typedef size_t button_map[button_count];
		// The mapping between physical button and game key.
		static button_map	game_button_map;

		// Now for wiimote
		static const size_t	wiimote_button_count	= sizeof(u16) * 11;
		typedef size_t wiimote_button_map[wiimote_button_count];
		static wiimote_button_map	wiimote_game_button_map;

		// Nuncuhk
		static const size_t	nunchuk_button_count	= sizeof(u16) * 2;
		typedef size_t nunchuk_button_map[nunchuk_button_count];
		static nunchuk_button_map	nunchuk_game_button_map;

		// The previous key state (for checking if things changed).
		static bool			previous_key_state[KEY_COUNT];

		// Converts a bitmask into a bit offset.
		static size_t mask_to_shift(size_t mask)
		{
			// Bad mask?
			if (!mask)
			{
				return 0;
			}

			// Shift right until we hit a 1.
			size_t shift = 0;
			while ((mask & 1) == 0)
			{
				mask >>= 1;
				++shift;
			}
			return shift;
		}

		static float clamp(float value, float minimum, float maximum)
		{
			if (value > maximum)
			{
				return maximum;
			}
			else if (value < minimum)
			{
				return minimum;
			}
			else
			{
				return value;
			}
		}

		static void apply_dead_zone(float* x, float* y, float dead_zone)
		{
			// Either stick out of the dead zone?
			if ((fabsf(*x) >= dead_zone) || (fabsf(*y) >= dead_zone))
			{
				// Nothing to do.
			}
			else
			{
				// Clamp to the dead zone.
				*x = 0.0f;
				*y = 0.0f;
			}
		}
	}
}

using namespace quake;
using namespace quake::input;

void IN_Init (void)
{
	// PAD_Init and WPAD_Init are called from main() as they are required to "OK" error messages.

	// Set up the game mappings.
	game_button_map[mask_to_shift(PAD_BUTTON_LEFT)]		= K_LEFTARROW;
	game_button_map[mask_to_shift(PAD_BUTTON_RIGHT)]	= K_RIGHTARROW;
	game_button_map[mask_to_shift(PAD_BUTTON_DOWN)]		= K_DOWNARROW;
	game_button_map[mask_to_shift(PAD_BUTTON_UP)]		= K_UPARROW;
	game_button_map[mask_to_shift(PAD_TRIGGER_Z)]		= K_JOY7;
	game_button_map[mask_to_shift(PAD_TRIGGER_R)]		= K_JOY6;
	game_button_map[mask_to_shift(PAD_TRIGGER_L)]		= K_JOY5;
	game_button_map[mask_to_shift(PAD_BUTTON_A)]		= K_JOY1;
	game_button_map[mask_to_shift(PAD_BUTTON_B)]		= K_JOY2;
	game_button_map[mask_to_shift(PAD_BUTTON_X)]		= K_JOY3;
	game_button_map[mask_to_shift(PAD_BUTTON_Y)]		= K_JOY4;
	game_button_map[mask_to_shift(PAD_BUTTON_START)]	= K_ESCAPE;

	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_LEFT)]		= K_LEFTARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_RIGHT)]		= K_RIGHTARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_DOWN)]		= K_DOWNARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_UP)]			= K_UPARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_A)]			= K_JOY1;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_B)]			= K_JOY6;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_PLUS)]		= K_ESCAPE;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_MINUS)]		= K_JOY4;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_1)]			= K_JOY2;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_2)]			= K_JOY3;
	nunchuk_game_button_map[mask_to_shift(NUNCHUK_BUTTON_C)]	= K_JOY5;
	nunchuk_game_button_map[mask_to_shift(NUNCHUK_BUTTON_Z)]	= K_JOY7;

#if FORCE_KEY_BINDINGS
	// Set up the key bindings.
	Cbuf_AddText("bind JOY1 +jump\n");
	Cbuf_AddText("bind JOY2 +speed\n");
	Cbuf_AddText("bind JOY4 \"impulse 10\"\n");
	Cbuf_AddText("bind JOY6 +attack\n");
#endif
}

void IN_Shutdown (void)
{
    WPAD_Shutdown();
}

bool wiimote_connected;
bool nunchuk_connected;

void IN_Commands (void)
{
	// Fetch the pad state.
	PAD_ScanPads();
	const u16 buttons = PAD_ButtonsHeld(0);

	u32 conn_dev;
	if (WPAD_Probe(WPAD_CHAN_0, &conn_dev) != WPAD_ERR_NONE)
	{
		wiimote_connected = false;
		nunchuk_connected = false;
	}
	else
	{
		wiimote_connected = true;
		if (conn_dev == WPAD_DEV_NUNCHAKU)
		{
			nunchuk_connected = true;
		}
		else
		{
			nunchuk_connected = false;
		}
	}
	if (wiimote_connected)
		WPAD_Read(WPAD_CHAN_0, &pad);

	const u16 wiimote_buttons = pad.btns_d;
	const u16 nunchuk_buttons = pad.exp.nunchuk.btns;

	// Somewhere to store the key state.
	bool key_state[KEY_COUNT];
	memset(&key_state[0], 0, sizeof(key_state));

	// For each button in the key map...
	for (size_t button = 0; button < button_count; ++button)
	{
		// Is this mapping defined?
		size_t key = game_button_map[button];
		if (key)
		{
			// Set the key state.
			const size_t mask = 1 << button;
			key_state[key] |= buttons & mask;
		}
	}
	if (wiimote_connected)
		for (size_t button = 0; button < wiimote_button_count; ++button)
		{
			// Is this mapping defined?
			size_t key = wiimote_game_button_map[button];
			if (key)
			{
				// Set the key state.
				const size_t mask = 1 << button;
				key_state[key] |= wiimote_buttons & mask;
			}
		}
	if (nunchuk_connected)
		for (size_t button = 0; button < nunchuk_button_count; ++button)
		{
			// Is this mapping defined?
			size_t key = nunchuk_game_button_map[button];
			if (key)
			{
				// Set the key state.
				const size_t mask = 1 << button;
				key_state[key] |= nunchuk_buttons & mask;
			}
		}

	// Accelerometers
	// TODO: something fancy like the button interface
	if (nunchuk_connected && pad.exp.nunchuk.gforce.z < -0.55f)
	{
		key_state[K_JOY1] |= 1;
	}

	// Find any differences between the key states.
	for (int key = 0; key < KEY_COUNT; ++key)
	{
		// Has this key changed?
		if (key_state[key] != previous_key_state[key])
		{
			// Was it pressed?
			if (key_state[key])
			{
				// Send a press event.
				Key_Event(static_cast<key_id_t>(key), qtrue);
			}
			else
			{
				// Send a release event.
				Key_Event(static_cast<key_id_t>(key), qfalse);
			}

			// Copy the key.
			previous_key_state[key] = key_state[key];
		}
	}
}

extern vrect_t  scr_vrect;

void IN_Move (usercmd_t *cmd)
{
	// Read the stick values.
	const s8 stick_x = PAD_StickX(0);
	const s8 stick_y = PAD_StickY(0);
	const s8 sub_stick_x = PAD_SubStickX(0);
	const s8 sub_stick_y = PAD_SubStickY(0);

	// IN_Move always called after IN_Commands on the same frame, this is valid data
	const u8 nunchuk_stick_x = pad.exp.nunchuk.js.pos.x;
	const u8 nunchuk_stick_y = pad.exp.nunchuk.js.pos.y;
	// TODO: sensor bar position correct? aspect ratio correctly set? etc...
	static int last_wiimote_ir_x = pad.ir.x;
	static int last_wiimote_ir_y = pad.ir.y;
	int wiimote_ir_x, wiimote_ir_y;;
	if (pad.ir.x < 1 || (unsigned int)pad.ir.x > pad.ir.vres[0] - 1)
		wiimote_ir_x = last_wiimote_ir_x;
	else
		wiimote_ir_x = pad.ir.x;
	if (pad.ir.y < 1 || (unsigned int)pad.ir.y > pad.ir.vres[1] - 1)
		wiimote_ir_y = last_wiimote_ir_y;
	else
		wiimote_ir_y = pad.ir.y;
	last_wiimote_ir_x = wiimote_ir_x;
	last_wiimote_ir_y = wiimote_ir_y;

	// Convert the inputs to floats in the range [-1, 1].
#if 0
	Con_Printf("pad: %4d, %4d %4d, %4d\n",
		pads[0].stickX,
		pads[0].stickY,
		pads[0].substickX,
		pads[0].substickY);
#endif

	// If the nunchuk is centered, read from the left gamecube pad stick
	float x1;
	if (!nunchuk_connected || (nunchuk_stick_x < 133 && nunchuk_stick_x > 121))
		x1 = clamp(stick_x / 90.0f, -1.0f, 1.0f);
	else
		x1 = clamp(((float)nunchuk_stick_x / 128.0f - 1.0f) * 1.5, -1.0f, 1.0f);

	float y1;
	if (!nunchuk_connected || (nunchuk_stick_y < 133 && nunchuk_stick_y > 121))
		y1 = clamp(stick_y / -90.0f, -1.0f, 1.0f);
	else
		y1 = clamp(((float)nunchuk_stick_y / (-128.0f) + 1.0f) * 1.5, -1.0f, 1.0f);

	// Now the gamecube C-stick has the priority
	float x2;
	if ((sub_stick_x < 6 || sub_stick_x > -6) && wiimote_connected)
		x2 = clamp((float)wiimote_ir_x / (pad.ir.vres[0] / 2.0f) - 1.0f, -1.0f, 1.0f);
	else
		x2 = clamp(sub_stick_x / 80.0f, -1.0f, 1.0f);

	float y2;
	if ((sub_stick_y < 6 || sub_stick_y > -6) && wiimote_connected)
		y2 = clamp((float)wiimote_ir_y / (pad.ir.vres[1] / 2.0f) - 1.0f, -1.0f, 1.0f);
	else
		y2 = clamp(sub_stick_y / -80.0f, -1.0f, 1.0f);

	Cvar_SetValue("cl_crossx", scr_vrect.width / 2 * x2);
	Cvar_SetValue("cl_crossy", scr_vrect.height / 2 * y2);

	// Apply the dead zone.
	const float dead_zone = 0.1f;
	apply_dead_zone(&x1, &y1, dead_zone);
	apply_dead_zone(&x2, &y2, dead_zone);

	// Don't let the pitch drift back to centre if mouse look is on or the right stick is being used.
	//if ((in_mlook.state & 1) || (fabsf(y2) >= dead_zone)) Disabled, always very convenient with a gamepad or wiimote
	{
		V_StopPitchDrift();
	}

	// Look using the main stick?
	float yaw_rate;
	float pitch_rate;
	if (in_mlook.state & 1)
	{
		// Turn using both sticks.
		yaw_rate = clamp(x1 + x2, -1.0f, 1.0f);
		pitch_rate = clamp(y1 + y2, -1.0f, 1.0f);
	}
	else
	{
		// Move using the main stick.
		cmd->sidemove += cl_sidespeed.value * x1;
		cmd->forwardmove -= cl_forwardspeed.value * y1;

		// Turn using the substick.
		yaw_rate = x2;
		pitch_rate = y2;
	}

	// Adjust the yaw.
	const float turn_rate = sensitivity.value * 50.0f;
	cl.viewangles[YAW] -= turn_rate * yaw_rate * host_frametime;

	// How fast to pitch?
	const float pitch_offset = turn_rate * pitch_rate * host_frametime;

	// Do the pitch.
	const bool	invert_pitch = m_pitch.value < 0;
	if (invert_pitch)
	{
		cl.viewangles[PITCH] -= pitch_offset;
	}
	else
	{
		cl.viewangles[PITCH] += pitch_offset;
	}

	// Don't look too far up or down.
	if (cl.viewangles[PITCH] > 80.0f)
	{
		cl.viewangles[PITCH] = 80.0f;
	}
	else if (cl.viewangles[PITCH] < -70.0f)
	{
		cl.viewangles[PITCH] = -70.0f;
	}
}
