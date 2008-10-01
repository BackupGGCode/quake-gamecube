/*
Quake GameCube port.
Copyright (C) 2007 Peter Mackay
Copyright (C) 2008 Eluan Miranda

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

// ELUTODO: I use libfat here, move everything to system_libfat.h and use generic calls here

// Handy switches.
#define CONSOLE_DEBUG		0
#define TIME_DEMO			0
#define USE_THREAD			1
#define TEST_CONNECTION		0
#define USBGECKO_DEBUG		0

// Standard includes.
#include <cstdio>
#include <vector>
#include <string>

// OGC includes.
#include <ogc/lwp.h>
#include <ogc/lwp_mutex.h>
#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include "input_wiimote.h"

#if USBGECKO_DEBUG
#include <debug.h>
#endif

extern "C"
{
#include <sys/stat.h>
#include <sys/dir.h>
#include <unistd.h>

#include "../generic/quakedef.h"
}

#define QUAKE_WII_BASEDIR	"/apps/quake"

int want_to_reset = 0;
int want_to_shutdown = 0;

extern void Sys_Reset (void);
extern void Sys_Shutdown (void);

void reset_system(void)
{
	want_to_reset = 1;
}

void shutdown_system(void)
{
	want_to_shutdown = 1;
}

namespace quake
{
	namespace main
	{
		// Video globals.
		void		*framebuffer[2]		= {NULL, NULL};
		u32		fb			= 0;
		GXRModeObj	*rmode			= 0;

		// Set up the heap.
		static const size_t	heap_size	= 19 * 1024 * 1024;
		static char		*heap;

		inline void *align32 (void *p)
		{
			return (void*)((((int)p + 31)) & 0xffffffe0);
		}

		static void init()
		{
			fb = 0;

			// Initialise the video system.
			VIDEO_Init();

			rmode = VIDEO_GetPreferredMode(NULL);

			// Allocate the frame buffer.
			framebuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
			framebuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

			// Set up the video system with the chosen mode.
			VIDEO_Configure(rmode);

			// Set the frame buffer.
			VIDEO_SetNextFramebuffer(framebuffer[fb]);

			VIDEO_SetBlack(FALSE);
			VIDEO_Flush();
			VIDEO_WaitVSync();
			if (rmode->viTVMode & VI_NON_INTERLACE)
			{
				VIDEO_WaitVSync();
			}

			// Initialise the debug console.
			// ELUTODO: only one framebuffer with it?
			console_init(framebuffer[0], 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * 2);

			// Initialise the controller library.
			PAD_Init();

#ifndef DISABLE_WIIMOTE
			if (WPAD_Init() != WPAD_ERR_NONE)
				Sys_Error("WPAD_Init() failed.\n");
#endif

			wiimote_ir_res_x = rmode->fbWidth;
			wiimote_ir_res_y = rmode->xfbHeight;
		}

		static void check_pak_file_exists()
		{
			int handle = -1;
			if (Sys_FileOpenRead(QUAKE_WII_BASEDIR"/id1/pak0.pak", &handle) < 0)
			{
				Sys_Error(
					QUAKE_WII_BASEDIR"/ID1/PAK0.PAK was not found.\n"
					"\n"
					"This file comes with the full or demo version of Quake\n"
					"and is necessary for the game to run.\n"
					"\n"
					"Please make sure it is on your SD card in the correct\n"
					"location.\n"
					"\n"
					"If you are absolutely sure the file is correct, your SD\n"
					"card may not be compatible with the SD card lib which\n"
					"Quake uses, or the Wii. Please check the issue tracker.");
				return;
			}
			else
			{
				Sys_FileClose(handle);
			}
		}

		// ELUTODO: ugly and beyond quake's limits, I think
		int parms_number = 0;
		char parms[1024];
		char *parms_ptr = parms;
		char *parms_array[64];
		static void add_parm(const char *parm)
		{
			if (strlen(parm) + ((u32)parms_ptr - (u32)parms) > 1023)
				Sys_Error("cmdline > 1024");
			
			strcpy(parms_ptr, parm);
			parms_array[parms_number++] = parms_ptr;
			parms_ptr += strlen(parm) + 1;
		}

		void frontend(void)
		{

			u32 cursor = 0;
			u32 cursor_modulus = 4;

			// option 0
			u32 missionpack_selected = 0;
			u32 missionpack_have = 1; // bitmask 1 = standard, 2 = scourge of armagon (hipnotic), 4 = dissolution of eternity (rogue)
			const char *missionpack_names[3] = {"Standard Quake", "Scourge of Armagon", "Dissolution of Eternity" };

			// option 1
			u32 mods_selected = 0;
			std::vector<std::string> mods_names;

			// option 2
			u32 network_disable = 1;

			// option 3
			u32 listen_players = 4;

			// find mods / mission packs, some code from snes9x-gx 1.51
			mods_names.push_back("None");

			DIR_ITER *fatdir;
			char filename[MAXPATHLEN];
			struct stat filestat;

			fatdir = diropen(QUAKE_WII_BASEDIR);
			if (!fatdir)
				Sys_Error("Error opening %s for read.\n", QUAKE_WII_BASEDIR);

			while (dirnext(fatdir, filename, &filestat) == 0)
			{
				if ((filestat.st_mode & _IFDIR) && strcmp(filename, ".") && strcmp(filename, "..") && strcasecmp(filename, "ID1"))
				{
					if(!strcasecmp(filename, "hipnotic"))
						missionpack_have |= 2;
					else if(!strcasecmp(filename, "rogue"))
						missionpack_have |= 4;
					else
						mods_names.push_back(filename);
				}
			}

			dirclose(fatdir);
	
			sort(mods_names.begin(), mods_names.end());

			while (1)
			{
				PAD_ScanPads();
				WPAD_ScanPads();

				u32 gcpress, wmpress;
				gcpress = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
				wmpress = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
				bool up = (gcpress & PAD_BUTTON_UP) | (wmpress & WPAD_BUTTON_UP);
				bool down = (gcpress & PAD_BUTTON_DOWN) | (wmpress & WPAD_BUTTON_DOWN);
				bool left = (gcpress & PAD_BUTTON_LEFT) | (wmpress & WPAD_BUTTON_LEFT);
				bool right = (gcpress & PAD_BUTTON_RIGHT) | (wmpress & WPAD_BUTTON_RIGHT);
				bool start = (gcpress & PAD_BUTTON_START) | (wmpress & WPAD_BUTTON_PLUS);

				printf("\x1b[2;0H");
				// ELUTODO: use CONF module to configure certain settings according to the wii's options
				printf("\n\n\n\n\n\n     If the Nunchuk isn't detected, please reconnect it to the wiimote.\n     Oh, and don't forget to put your wrist wrap! :)\n\n");

				if (up)
					cursor = (cursor - 1 + cursor_modulus) % cursor_modulus;
				if (down)
					cursor = (cursor + 1) % cursor_modulus;
				if (left)
				{
					switch (cursor)
					{
						case 0:
							missionpack_selected = (missionpack_selected - 1 + 3) % 3;
							break;
						case 1:
							mods_selected = (mods_selected - 1 + mods_names.size()) % mods_names.size();
							break;
						case 2:
							network_disable = !network_disable;
							break;
						case 3:
							listen_players--;
							if (listen_players < 4)
								listen_players = 4;
							break;
						default:
							Sys_Error("frontend: Invalid cursor position");
							break;
					}
				}
				if (right)
				{
					switch (cursor)
					{
						case 0:
							missionpack_selected = (missionpack_selected + 1) % 3;
							break;
						case 1:
							mods_selected = (mods_selected + 1) % mods_names.size();
							break;
						case 2:
							network_disable = !network_disable;
							break;
						case 3:
							listen_players++;
							if (listen_players > 16)
								listen_players = 16;
							break;
						default:
							Sys_Error("frontend: Invalid cursor position");
							break;
					}
				}


				if (start)
				{
					printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n     Starting Quake...\n\n\n\n");
					break;
				}

				printf("\n\n     Press UP, DOWN, LEFT, RIGHT to make your selections\n");
				printf("     Press START/PLUS to start the game\n\n");

				printf("     %c Mission Pack:      %s%s\n", cursor == 0 ? '>' : ' ', missionpack_names[missionpack_selected],
					(missionpack_have & (1 << missionpack_selected)) ? "                              " : " (You don't have it!)         ");

				const u32 mods_maxprintsize = 32;
				char mods_printvar[mods_maxprintsize];
				strncpy(mods_printvar, mods_names[mods_selected].c_str(), mods_maxprintsize);
				size_t mods_printsize = strlen(mods_printvar);
				u32 i;
				for (i = mods_printsize; i < mods_maxprintsize - 1; i++)
					mods_printvar[i] = ' ';
				mods_printvar[i] = '\0';

				printf("     %c Mod:               %s\n", cursor == 1 ? '>' : ' ', mods_printvar);

				printf("     %c Disable Network:   %s\n", cursor == 2 ? '>' : ' ', network_disable ? "yes" : "no ");

				printf("     %c Max Network Slots: %u   \n", cursor == 3 ? '>' : ' ', listen_players);

				printf("\n\n\n     Network support is still NOT IMPLEMENTED.\n     It's here only to enable easy bot matches.\n");

				VIDEO_WaitVSync();

			}

			// Initialise the Common module.
			add_parm("Quake");
#if CONSOLE_DEBUG
			add_parm("-condebug");
#endif
			if (missionpack_have & (1 << missionpack_selected))
			{
				switch(missionpack_selected)
				{
					case 0:
						break;
					case 1:
						add_parm("-hipnotic");
						break;
					case 2:
						add_parm("-rogue");
						break;
					default:
						Sys_Error("frontend: Invalid mission pack selected");
						break;
				}
			}

			if (mods_selected)
			{
				add_parm("-game");
				add_parm(mods_names[mods_selected].c_str()); // ELUTODO: bad thing to do?
			}

			if (network_disable)
			{
				add_parm("-noudp");
			}
			else
			{
				char temp_num[32];
				snprintf(temp_num, 32, "%u", listen_players);
				add_parm("-listen");
				add_parm(temp_num);
			}
		}

		static void* main_thread_function(void*)
		{
			u32 level, real_heap_size;

			// hope the parms are all set by now
			COM_InitArgv(parms_number, parms_array);

			_CPU_ISR_Disable(level);
			heap = (char *)align32(SYS_GetArena2Lo());
			real_heap_size = heap_size - ((u32)heap - (u32)SYS_GetArena2Lo());
			if ((u32)heap + real_heap_size > (u32)SYS_GetArena2Hi())
			{
				_CPU_ISR_Restore(level);
				Sys_Error("heap + real_heap_size > (u32)SYS_GetArena2Hi()");
			}	
			else
			{
				SYS_SetArena2Lo(heap + real_heap_size);
				_CPU_ISR_Restore(level);
			}

			VIDEO_SetBlack(TRUE);

			// Initialise the Host module.
			quakeparms_t parms;
			memset(&parms, 0, sizeof(parms));
			parms.argc		= com_argc;
			parms.argv		= com_argv;
			parms.basedir	= QUAKE_WII_BASEDIR;
			parms.memsize	= real_heap_size;
			parms.membase	= heap;
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
#if TEST_CONNECTION
			Cbuf_AddText("connect 192.168.0.2");
#endif

			SYS_SetResetCallback(reset_system);
			SYS_SetPowerCallback(shutdown_system);

			VIDEO_SetBlack(FALSE);

			// Run the main loop.
			u64 last_time = gettime();
			for (;;)
			{
				if (want_to_reset)
					Sys_Reset();
				if (want_to_shutdown)
					Sys_Shutdown();

				// Get the frame time in ticks.
				const u64		current_time	= gettime();
				const u64		time_delta		= current_time - last_time;
				const double	seconds	= time_delta * (0.001f / TB_TIMER_CLOCK);
				last_time = current_time;

				// Run the frame.
				Host_Frame(seconds);
			};

			// Quit (this code is never reached).
			Sys_Quit();
			return 0;
		}
	}
}

using namespace quake;
using namespace quake::main;

qboolean isDedicated = qfalse;

int main(int argc, char* argv[])
{
	void *qstack = malloc(4 * 1024 * 1024);

#if USBGECKO_DEBUG
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	_break();
#endif

	// Initialize.
	init();
	check_pak_file_exists();

	frontend();

	// Start the main thread.
	lwp_t thread;
	LWP_CreateThread(&thread, &main_thread_function, 0, qstack, 4 * 1024 * 1024, 64);

	// Wait for it to finish.
	void* result;
	LWP_JoinThread(thread, &result);

	exit(0);
	return 0;
}
