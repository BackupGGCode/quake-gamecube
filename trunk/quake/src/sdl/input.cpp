/*
Quake SDL port.
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

namespace quake
{
	namespace input
	{/*
		// A map from button mask to Quake key.
		static const size_t	button_count	= sizeof(u16) * 8;
		typedef size_t button_map[button_count];

		// The mapping between physical button and game key.
		static button_map	game_button_map;

		// The previous key state (for checking if things changed).
		static bool			previous_key_state[KEY_COUNT];

		// The pad state.
		static PADStatus	pads[PAD_CHANMAX];

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
		}*/
	}
}

using namespace quake;
using namespace quake::input;

void IN_Init (void)
{/*
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

	// Set up the key bindings.
	Cbuf_AddText("bind JOY1 +jump\n");
	Cbuf_AddText("bind JOY2 +speed\n");
	Cbuf_AddText("bind JOY4 \"impulse 10\"\n");
	Cbuf_AddText("bind JOY6 +attack\n");*/
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{/*
	// Read the pad state.
	memset(&pads[0], 0, sizeof(pads));
	PAD_Read(&pads[0]);

	// Any errors reading the pad?
	switch (pads[0].err)
	{
		// Pad read okay.
	case PAD_ERR_NONE:
		{
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
					key_state[key] |= pads[0].button & mask;
				}
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
		break;

		// No controller inserted, release all buttons.
	case PAD_ERR_NO_CONTROLLER:
		{
			// Find any pressed keys.
			for (int key = 0; key < KEY_COUNT; ++key)
			{
				// Was this key pressed?
				if (previous_key_state[key])
				{
					// Send a release event.
					Key_Event(static_cast<key_id_t>(key), qfalse);

					// Release the key.
					previous_key_state[key] = false;
				}
			}
		}
		break;

		// Pad not ready, do nothing.
	case PAD_ERR_NOT_READY:
		Con_Printf("%s", "PAD_ERR_NOT_READY\n");
		memset(&pads[0], 0, sizeof(pads));
		break;

		// Transfer error, do nothing.
	case PAD_ERR_TRANSFER:
		memset(&pads[0], 0, sizeof(pads));
		break;

		// Unhandled error code.
	default:
		Sys_Error("Unhandled PAD_Read error %d", pads[0].err);
	}*/
}

void IN_Move (usercmd_t *cmd)
{/*
	// Convert the inputs to floats in the range [-1, 1].
#if 0
	Con_Printf("pad: %4d, %4d %4d, %4d\n",
		pads[0].stickX,
		pads[0].stickY,
		pads[0].substickX,
		pads[0].substickY);
#endif
	float x1 = clamp(pads[0].stickX / 90.0f, -1.0f, 1.0f);
	float y1 = clamp(pads[0].stickY / -90.0f, -1.0f, 1.0f);
	float x2 = clamp(pads[0].substickX / 80.0f, -1.0f, 1.0f);
	float y2 = clamp(pads[0].substickY / -80.0f, -1.0f, 1.0f);

	// Apply the dead zone.
	const float dead_zone = 0.1f;
	apply_dead_zone(&x1, &y1, dead_zone);
	apply_dead_zone(&x2, &y2, dead_zone);

	// Don't let the pitch drift back to centre if mouse look is on or the right stick is being used.
	if ((in_mlook.state & 1) || (fabsf(y2) >= dead_zone))
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
	}*/
}
