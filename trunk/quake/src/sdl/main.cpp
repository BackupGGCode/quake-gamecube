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

// Standard includes.
#include <cstdlib>
#include <vector>

// SDL includes.
#include <SDL.h>
#include <SDL_main.h>

extern "C"
{
#include "../generic/quakedef.h"
#include "../generic/file.h"
}

#define TIME_DEMO	0

namespace quake
{
	namespace main
	{
		// Set up the heap.
		typedef Uint32 heap_type;
		static heap_type	heap[12 * 1024 * 1024 / sizeof(heap_type)];
	}
}

using namespace quake;
using namespace quake::main;

qboolean isDedicated = qfalse;

int main(int argc, char* argv[])
{
	// Initialise.
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK /*| SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE*/) < 0)
	{
		printf("SDL_Init failed (%s)\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	// Initialise the Common module.
	COM_InitArgv(argc, argv);

	// Initialise the Host module.
	quakeparms_t parms;
	memset(&parms, 0, sizeof(parms));
	parms.argc		= com_argc;
	parms.argv		= com_argv;
	parms.basedir	= ".";
	parms.memsize	= sizeof(heap);
	parms.membase	= &heap[0];
	if (parms.membase == 0)
	{
		Sys_Error("Heap allocation failed");
	}
	memset(parms.membase, 0, parms.memsize);
	Host_Init(&parms);

#if TIME_DEMO
	Cbuf_AddText("map start\n");
	Cbuf_AddText("wait\n");
	Cbuf_AddText("timedemo demo1\n");
#endif

	// Run the main loop.
	Uint32 last_time = SDL_GetTicks();
	for (;;)
	{
		// Get the frame time in ticks.
		const Uint32	current_time	= SDL_GetTicks();
		const Uint32	time_delta		= current_time - last_time;
		const float		seconds			= time_delta * 0.001f;
		last_time = current_time;

		// Run the frame.
		Host_Frame(seconds);
	};

	// Quit (this code is never reached).
	Sys_Quit();
	return 0;
}
