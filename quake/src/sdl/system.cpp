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

#define ENABLE_PRINTF 0

#include <cassert>
#include <cstddef>
#include <cstdarg>
#include <cstring>
#include <cstdio>

#include <SDL.h>

extern "C"
{
#include "../generic/quakedef.h"
}

namespace quake
{
	namespace system
	{
		struct file_slot
		{
			SDL_RWops*	handle;
			char		path[MAX_OSPATH + 1];

			bool in_use() const
			{
				return handle != 0;
			}
		};

		static const std::size_t		file_count		= 64;
		static file_slot				files[file_count];

		static volatile unsigned long	frames = 0;

		static file_slot& index_to_file(size_t index, const char* function)
		{
			if ((index < 0) || (index >= file_count))
			{
				Sys_Error("%s: File index out of bounds (%d, 0x%08x)\n", function, index, index);
				for (;;)
				{
				}
			}
			else
			{
				file_slot& f = files[index];
				if (!f.in_use())
				{
					Sys_Error("%s: Attempt to access closed file.\n", function, index);
				}

				return f;
			}
		}

		static void dump()
		{
			// Dump files.
			for (size_t index = 0; index < file_count; ++index)
			{
				const file_slot& file = files[index];
				if (file.in_use())
				{
					Sys_Printf("File %u: %s\n", index, file.path);
				}
			}
		}

		static key_id_t sdl_key_to_quake_key(SDLKey in)
		{
			key_id_t out;

			// Some characters map directly.
			if ((in < KEY_COUNT) && isprint(in))
			{
				out = static_cast<key_id_t>(in);
			}
			else
			{
				// Handle the rest.
				switch (in)
				{

#define MAP2(sdl, quake)	case sdl: out = quake; break
#define MAP1(key)			MAP2(SDLK_##key, K_##key)

					MAP1(BACKSPACE);
					MAP1(TAB);
					MAP2(SDLK_RETURN, K_ENTER);
					MAP1(PAUSE);
					MAP1(ESCAPE);
					MAP2(SDLK_DELETE, K_DEL);
					MAP2(SDLK_KP0, static_cast<key_id_t>('0'));
					MAP2(SDLK_KP1, static_cast<key_id_t>('1'));
					MAP2(SDLK_KP2, static_cast<key_id_t>('2'));
					MAP2(SDLK_KP3, static_cast<key_id_t>('3'));
					MAP2(SDLK_KP4, static_cast<key_id_t>('4'));
					MAP2(SDLK_KP5, static_cast<key_id_t>('5'));
					MAP2(SDLK_KP6, static_cast<key_id_t>('6'));
					MAP2(SDLK_KP7, static_cast<key_id_t>('7'));
					MAP2(SDLK_KP8, static_cast<key_id_t>('8'));
					MAP2(SDLK_KP9, static_cast<key_id_t>('9'));
					MAP2(SDLK_KP_PERIOD, static_cast<key_id_t>('.'));
					MAP2(SDLK_KP_DIVIDE, static_cast<key_id_t>('/'));
					MAP2(SDLK_KP_MULTIPLY, static_cast<key_id_t>('*'));
					MAP2(SDLK_KP_MINUS, static_cast<key_id_t>('0'));
					MAP2(SDLK_KP_PLUS, static_cast<key_id_t>('+'));
					MAP2(SDLK_KP_ENTER, K_ENTER);
					MAP2(SDLK_KP_EQUALS, static_cast<key_id_t>('-'));
					MAP2(SDLK_UP, K_UPARROW);
					MAP2(SDLK_DOWN, K_DOWNARROW);
					MAP2(SDLK_RIGHT, K_RIGHTARROW);
					MAP2(SDLK_LEFT, K_LEFTARROW);
					MAP2(SDLK_INSERT, K_INS);
					MAP1(HOME);
					MAP1(END);
					MAP2(SDLK_PAGEUP, K_PGUP);
					MAP2(SDLK_PAGEDOWN, K_PGDN);
					MAP1(F1);
					MAP1(F2);
					MAP1(F3);
					MAP1(F4);
					MAP1(F5);
					MAP1(F6);
					MAP1(F7);
					MAP1(F8);
					MAP1(F9);
					MAP1(F10);
					MAP1(F11);
					MAP1(F12);
					MAP2(SDLK_RSHIFT, K_SHIFT);
					MAP2(SDLK_LSHIFT, K_SHIFT);
					MAP2(SDLK_RCTRL, K_CTRL);
					MAP2(SDLK_LCTRL, K_CTRL);
					MAP2(SDLK_RALT, K_ALT);
					MAP2(SDLK_LALT, K_ALT);

#undef MAP1
#undef MAP2

				default:
					out = static_cast<key_id_t>(0);
				}
			}

			assert(out >= 0);
			assert(out < KEY_COUNT);
			return out;
		}
	}
}

using namespace quake;
using namespace quake::system;

int Sys_FileOpenRead (const char *path, int *hndl)
{
	// Find an unused file slot.
	for (std::size_t file_index = 0; file_index < file_count; ++file_index)
	{
		// Is the file slot free?
		file_slot& file = files[file_index];
		if (!file.in_use())
		{
			// Open the file.
			file.handle = SDL_RWFromFile(path, "rb");
			if (!file.handle)
			{
				*hndl = -1;
				return -1;
			}

			// Get the length.
			const int length = SDL_RWseek(file.handle, 0, RW_SEEK_END);
			SDL_RWseek(file.handle, 0, RW_SEEK_SET);

			// Set up the file info.
			strcpy(file.path, path);

			// Done.
			*hndl = file_index;
			return length;
		}
	}

	// This is bad.
	Sys_Error("Out of file slots");
	*hndl = -1;
	return -1;
}

int Sys_FileOpenWrite (const char *path)
{
	// Find an unused file slot.
	for (std::size_t file_index = 0; file_index < file_count; ++file_index)
	{
		// Is the file slot free?
		file_slot& file = files[file_index];
		if (!file.in_use())
		{
			// Open the file.
			file.handle = SDL_RWFromFile(path, "wb");
			if (!file.handle)
			{
				return -1;
			}

			// Set up the file info.
			strcpy(file.path, path);

			// Done.
			return file_index;
		}
	}

	// This is bad.
	Sys_Error("Out of file slots");
	return -1;
}

void Sys_FileClose (int handle)
{
//	Sys_Printf("Sys_FileClose\n");

	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Close the file.
	SDL_RWclose(file.handle);

	// Clear the file.
	memset(&file, 0, sizeof(file));

//	Sys_Printf("Sys_FileClose OK\n");
}

void Sys_FileSeek (int handle, int position)
{
//	Sys_Printf("Sys_FileSeek: %d bytes\n", position);

	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Set the position.
	SDL_RWseek(file.handle, position, RW_SEEK_SET);

//	Sys_Printf("Sys_FileSeek OK\n", position);
}

int Sys_FileRead (int handle, void *dest, int count)
{
//	Sys_Printf("Sys_FileRead: %d bytes\n", count);

	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Read.
	const int chunks_read = SDL_RWread(file.handle, dest, count, 1);
	if (chunks_read != 1)
	{
		dump();
		Sys_Error("Read failed (got %d, wanted %d", chunks_read, 1);
		return 0;
	}
	else
	{
		return count;
	}
}

int Sys_FileWrite (int handle, const void *data, int count)
{
	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Write.
	const int chunks_written = SDL_RWwrite(file.handle, data, count, 1);
	if (chunks_written != 1)
	{
		dump();
		Sys_Error("Write failed (got %d, wanted %d", chunks_written, 1);
		return 0;
	}
	else
	{
		return count;
	}
}

int	Sys_FileTime (const char *path)
{
	// Get the file info.
	SDL_RWops* const handle = SDL_RWFromFile(path, "rb");
	if (handle)
	{
		SDL_RWclose(handle);
		return 0;
	}
	else
	{
		return -1;
	}
}

void Sys_mkdir (const char *path)
{
	Sys_Error("Mkdir: %s\n", path);
}

void Sys_Error (char *error, ...)
{
	// Clear the sound buffer.
	S_ClearBuffer();

	// Write the error.
	va_list args;
	va_start(args, error);
	vfprintf(stderr, error, args);
	va_end(args);

	// Wait.
	SDL_Delay(5000);

	// Quit.
	Sys_Quit();
}

void Sys_Printf (char *fmt, ...)
{
#if ENABLE_PRINTF

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	
# if 0
	SDL_Delay(1000);
# endif
#endif
}

void Sys_Quit (void)
{
	// Shut down the host system.
	if (host_initialized)
	{
		Host_Shutdown();
	}

	// Exit.
	SDL_Quit();
	exit(0);
}

double Sys_FloatTime (void)
{
	return SDL_GetTicks() * 0.001;
}

char *Sys_ConsoleInput (void)
{
	return 0;
}

void Sys_SendKeyEvents (void)
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
		case SDL_QUIT:
			Sys_Quit();
			break;

		case SDL_KEYDOWN:
			Key_Event(sdl_key_to_quake_key(e.key.keysym.sym), qtrue);
			break;

		case SDL_KEYUP:
			Key_Event(sdl_key_to_quake_key(e.key.keysym.sym), qfalse);
			break;
		}
	}
}

void Sys_LowFPPrecision (void)
{
}

void Sys_HighFPPrecision (void)
{
}
