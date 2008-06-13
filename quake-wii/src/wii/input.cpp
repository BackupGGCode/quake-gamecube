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

// ELUTODO: do something about lookspring and lookstrafe
// ELUTODO: keys to: stop wiimote turning, nunchuk turn and nunchuk look up/down

extern "C"
{
#include "../generic/quakedef.h"
}

#include <ogc/pad.h>
#include <wiiuse/wpad.h>
#include "input_wiimote.h"

#define FORCE_KEY_BINDINGS 0

u32 wiimote_ir_res_x;
u32 wiimote_ir_res_y;

namespace quake
{
	namespace input
	{
		// wiimote info
		WPADData *pad;
		bool wiimote_connected = false;
		bool nunchuk_connected = false;

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

/*
 * [QUOTE=eke-eke]
 * the nunchuk analog stick seems to act weird sometime, here's the functions I added for reading the Sticks values like the old PAD_ library, maybe you would find this useful:
 */
static s8 WPAD_StickX(WPADData *data,u8 which)
{
	float mag = 0.0;
	float ang = 0.0;

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (which == 0)
			{
				mag = data->exp.nunchuk.js.mag;
				ang = data->exp.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (which == 0)
			{
				mag = data->exp.classic.ljs.mag;
				ang = data->exp.classic.ljs.ang;
			}
			else
			{
				mag = data->exp.classic.rjs.mag;
				ang = data->exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle needs to be converted into radians) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * sin(M_PI * ang/180.0f);

	return (s8)(val * 128.0f);
}

static s8 WPAD_StickY(WPADData *data,u8 which)
{
	float mag = 0.0;
	float ang = 0.0;

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (which == 0)
			{
				mag = data->exp.nunchuk.js.mag;
				ang = data->exp.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (which == 0)
			{
				mag = data->exp.classic.ljs.mag;
				ang = data->exp.classic.ljs.ang;
			}
			else
			{
				mag = data->exp.classic.rjs.mag;
				ang = data->exp.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * cos(M_PI * ang/180.0f);

	return (s8)(val * 128.0f);
}

void IN_Init (void)
{
	// PAD_Init and WPAD_Init are called from main() as they are required to "OK" error messages.

	// Set up the game mappings.
	game_button_map[mask_to_shift(PAD_BUTTON_LEFT)]		= K_LEFTARROW;
	game_button_map[mask_to_shift(PAD_BUTTON_RIGHT)]	= K_RIGHTARROW;
	game_button_map[mask_to_shift(PAD_BUTTON_DOWN)]		= K_DOWNARROW;
	game_button_map[mask_to_shift(PAD_BUTTON_UP)]		= K_UPARROW;
	game_button_map[mask_to_shift(PAD_BUTTON_START)]	= K_ESCAPE;
	game_button_map[mask_to_shift(PAD_TRIGGER_Z)]		= K_JOY7;
	game_button_map[mask_to_shift(PAD_TRIGGER_R)]		= K_JOY6;
	game_button_map[mask_to_shift(PAD_TRIGGER_L)]		= K_JOY5;
	game_button_map[mask_to_shift(PAD_BUTTON_A)]		= K_JOY1;
	game_button_map[mask_to_shift(PAD_BUTTON_B)]		= K_JOY2;
	game_button_map[mask_to_shift(PAD_BUTTON_X)]		= K_JOY3;
	game_button_map[mask_to_shift(PAD_BUTTON_Y)]		= K_JOY4;

	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_LEFT)]		= K_LEFTARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_RIGHT)]		= K_RIGHTARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_DOWN)]		= K_DOWNARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_UP)]			= K_UPARROW;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_PLUS)]		= K_ESCAPE;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_A)]			= K_JOY8;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_B)]			= K_JOY9;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_MINUS)]		= K_JOY10;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_1)]			= K_JOY11;
	wiimote_game_button_map[mask_to_shift(WPAD_BUTTON_2)]			= K_JOY12;
	nunchuk_game_button_map[mask_to_shift(NUNCHUK_BUTTON_C)]	= K_JOY13;
	nunchuk_game_button_map[mask_to_shift(NUNCHUK_BUTTON_Z)]	= K_JOY14;

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
}

void IN_Commands (void)
{
	// Fetch the pad state.
	PAD_ScanPads();
	const u16 buttons = PAD_ButtonsHeld(0);

#ifdef DISABLE_WIIMOTE
	wiimote_connected = false;
	nunchuk_connected = false;
#else
	u32 conn_dev;
	if (WPAD_Probe(WPAD_CHAN_0, &conn_dev) != WPAD_ERR_NONE)
	{
		wiimote_connected = false;
		nunchuk_connected = false;
	}
	else
	{
		// TODO: better way to do this? a callback or something
		if (!wiimote_connected)
		{
			WPAD_SetVRes(WPAD_CHAN_0, wiimote_ir_res_x, wiimote_ir_res_y);
			WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
			WPAD_SetIdleTimeout(60); // thanks eke-eke for the confirmation that this is the timeout in seconds
		}

		wiimote_connected = true;
		if (conn_dev == WPAD_EXP_NUNCHUK)
		{
			nunchuk_connected = true;
		}
		else
		{
			nunchuk_connected = false;
		}
	}
#endif
	if (wiimote_connected)
	{
		WPAD_ScanPads();
		pad = WPAD_Data(WPAD_CHAN_0);
	}

	const u16 wiimote_buttons = wiimote_connected ? pad->btns_h : 0;
	const u16 nunchuk_buttons = nunchuk_connected ? pad->exp.nunchuk.btns_held : 0;

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
	if (nunchuk_connected && pad->exp.nunchuk.gforce.z < -0.50f)
	{
		key_state[K_JOY8] |= 1;
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

void IN_Move (usercmd_t *cmd)
{
	// Read the stick values.
	const s8 stick_x = PAD_StickX(0);
	const s8 stick_y = PAD_StickY(0);
	const s8 sub_stick_x = PAD_SubStickX(0);
	const s8 sub_stick_y = PAD_SubStickY(0);

	// IN_Move always called after IN_Commands on the same frame, this is valid data
	// TODO: new issue, if the wiimote gets resynced during game, sometimes we get invalid nunchuk data! Has it been fixed?
	const s8 nunchuk_stick_x = nunchuk_connected ? WPAD_StickX(pad, 0) : 0;
	const s8 nunchuk_stick_y = nunchuk_connected ? WPAD_StickY(pad, 0) : 0;
	// TODO: sensor bar position correct? aspect ratio correctly set? etc...
	static int last_wiimote_ir_x = wiimote_connected ? pad->ir.x : 0;
	static int last_wiimote_ir_y = wiimote_connected ? pad->ir.y : 0;
	int wiimote_ir_x = 0, wiimote_ir_y = 0;
	if (wiimote_connected)
	{
		if (pad->ir.x < 1 || (unsigned int)pad->ir.x > pad->ir.vres[0] - 1)
			wiimote_ir_x = last_wiimote_ir_x;
		else
			wiimote_ir_x = pad->ir.x;
		if (pad->ir.y < 1 || (unsigned int)pad->ir.y > pad->ir.vres[1] - 1)
			wiimote_ir_y = last_wiimote_ir_y;
		else
			wiimote_ir_y = pad->ir.y;
	}
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
	if (nunchuk_stick_x < 6 && nunchuk_stick_x > -6)
		x1 = clamp(stick_x / 90.0f, -1.0f, 1.0f);
	else
		x1 = clamp(((float)nunchuk_stick_x / 128.0f) * 1.5, -1.0f, 1.0f);

	float y1;
	if (nunchuk_stick_y < 6 && nunchuk_stick_y > -6)
		y1 = clamp(stick_y / -90.0f, -1.0f, 1.0f);
	else
		y1 = clamp(((float)nunchuk_stick_y / (-128.0f)) * 1.5, -1.0f, 1.0f);

	// Now the gamecube C-stick has the priority
	static bool using_c_stick = false;
	static int last_irx = -1, last_iry = -1;
	float x2;
	if ((sub_stick_x < 6 && sub_stick_x > -6) && wiimote_connected && (using_c_stick == false || (using_c_stick == true && last_irx != wiimote_ir_x)))
	{
		x2 = clamp((float)wiimote_ir_x / (pad->ir.vres[0] / 2.0f) - 1.0f, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossx", scr_vrect.width / 2 * x2);
		using_c_stick = false;
	}
	else
	{
		x2 = clamp(sub_stick_x / 80.0f, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossx", 0);
		using_c_stick = true;
	}

	float y2;
	if ((sub_stick_y < 6 && sub_stick_y > -6) && wiimote_connected && (using_c_stick == false || (using_c_stick == true && last_iry != wiimote_ir_y)))
	{
		y2 = clamp((float)wiimote_ir_y / (pad->ir.vres[1] / 2.0f) - 1.0f, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossy", scr_vrect.height / 2 * y2);
		using_c_stick = false;
	}
	else
	{
		y2 = clamp(sub_stick_y / -80.0f, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossy", 0);
		using_c_stick = true;
	}

	last_irx = wiimote_ir_x;
	last_iry = wiimote_ir_y;

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
		cmd->forwardmove -= cl_forwardspeed.value * y1; /* TODO: use cl_backspeed when going backwards? */

		if (in_speed.state & 1)
		{
			if (cl_forwardspeed.value > 200)
			{
				cmd->forwardmove /= cl_movespeedkey.value;
				cmd->sidemove /= cl_movespeedkey.value;
			}
			else
			{
				cmd->forwardmove *= cl_movespeedkey.value;
				cmd->sidemove *= cl_movespeedkey.value; /* TODO: always seem to be at the max and I'm too sleepy now to figure out why */
			}
		}

		// Turn using the substick.
		yaw_rate = x2;
		pitch_rate = y2;
	}

	// TODO: Use yawspeed and pitchspeed

	// Adjust the yaw.
	const float turn_rate = sensitivity.value * 50.0f;
	if (in_speed.state & 1)
	{
		if (cl_forwardspeed.value > 200)
			cl.viewangles[YAW] -= turn_rate * yaw_rate * host_frametime / cl_anglespeedkey.value;
		else
			cl.viewangles[YAW] -= turn_rate * yaw_rate * host_frametime * cl_anglespeedkey.value;
	}
	else
		cl.viewangles[YAW] -= turn_rate * yaw_rate * host_frametime;

	// How fast to pitch?
	float pitch_offset;
	if (in_speed.state & 1)
	{
		if (cl_forwardspeed.value > 200)
			pitch_offset = turn_rate * pitch_rate * host_frametime / cl_anglespeedkey.value;
		else
			pitch_offset = turn_rate * pitch_rate * host_frametime * cl_anglespeedkey.value;
	}
	else
		pitch_offset = turn_rate * pitch_rate * host_frametime;

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
