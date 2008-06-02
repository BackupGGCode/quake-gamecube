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

#define ENABLE_PRINTF 0

#include <sys/unistd.h>

#include <ogc/gx_struct.h>
#include <ogc/pad.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <wiiuse/wpad.h>

extern "C"
{
#include "../generic/quakedef.h"
}

namespace quake
{
	namespace main
	{
		extern GXRModeObj*	rmode;
	}

	namespace system
	{
		static volatile unsigned long	frames = 0;

		static void increment_frame_counter(u32)
		{
			++frames;
		}
	}
}

using namespace quake;
using namespace quake::system;

void Sys_Error (const char *error, ...)
{
	// Clear the sound buffer.
	S_ClearBuffer();

	// Put the error message in a buffer.
	va_list args;
	va_start(args, error);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	vsnprintf(buffer, sizeof(buffer) - 1, error, args);
	va_end(args);

	// Print the error message to the debug screen.
	printf("The following error occurred:\n");
	printf("%s\n\n", buffer);
	for (int i = 0; i < 60; ++i)
	{
		VIDEO_WaitVSync();
	};
	printf("Press A to quit.\n");

	// Wait for the user to release the button.
	do
	{
		VIDEO_WaitVSync();
		PAD_ScanPads();
		WPAD_ScanPads();
	}
	while (PAD_ButtonsHeld(0) & PAD_BUTTON_A || WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_A);

	// Wait for the user to press the button.
	do
	{
		VIDEO_WaitVSync();
		PAD_ScanPads();
		WPAD_ScanPads();
	}
	while (((PAD_ButtonsHeld(0) & PAD_BUTTON_A) == 0) && ((WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_A) == 0));

	printf("Sys_Qui();\n");

	// Quit.
	Sys_Quit();
}

void Sys_Printf (const char *fmt, ...)
{
#if ENABLE_PRINTF

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	
# if 0
	for (int i = 0; i < 50; ++i)
	{
		VIDEO_WaitVSync();
	}
# endif
#endif
}

void Sys_Quit (void)
{
	Sys_Printf("%s", "Resetting...\n");

	// Shut down the host system.
	if (host_initialized)
	{
		Host_Shutdown();
	}

	// Exit.
	exit(0);
}

double Sys_FloatTime (void)
{
	static bool init = false;
	if (!init)
	{
		VIDEO_SetPreRetraceCallback(increment_frame_counter);
		init = true;
	}

	if ((main::rmode == &TVPal528IntDf) || (main::rmode == &TVPal264Int))
	{
		return frames * (1.0f / 50.0f);
	}
	else
	{
		return frames * (1.0f / 60.0f);
	}
}

char *Sys_ConsoleInput (void)
{
	return 0;
}

void Sys_SendKeyEvents (void)
{
}

void Sys_LowFPPrecision (void)
{
}

void Sys_HighFPPrecision (void)
{
}
