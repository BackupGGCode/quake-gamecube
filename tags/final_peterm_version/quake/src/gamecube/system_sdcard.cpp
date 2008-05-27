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

#include <cctype>
#include <cstddef>

#include <sdcard.h>

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
			sd_file*	handle;
			char		path[MAX_OSPATH + 1];

			bool in_use() const
			{
				return handle != 0;
			}
		};

		static const std::size_t		file_count		= 64;
		static file_slot				files[file_count];

		static void init()
		{
			static bool initialised = false;
			if (!initialised)
			{
				SDCARD_Init();
				initialised = true;
			}
		}

		static void translate_path(const char* in, char* out)
		{
			// Copy the device name.
			strcpy(out, "dev0:");

			// Copy the string.
			strcat(out, in);

			// Convert all remaining characters.
			for (char* c = &out[5]; *c; ++c)
			{
				if ((*c) == '/')
				{
					*c = '\\';
				}
			}
		}

		static inline file_slot& index_to_file(size_t index, const char* function)
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

		static const char* code_to_string(int code)
		{
			switch (code)
			{
			case SDCARD_ERROR_READY:
				return "SDCARD_ERROR_READY";
			case SDCARD_ERROR_EOF:
				return "SDCARD_ERROR_EOF";
			case SDCARD_ERROR_BUSY:
				return "SDCARD_ERROR_BUSY";
			case SDCARD_ERROR_WRONGDEVICE:
				return "SDCARD_ERROR_WRONGDEVICE";
			case SDCARD_ERROR_NOCARD:
				return "SDCARD_ERROR_NOCARD";
			case SDCARD_ERROR_IOERROR:
				return "SDCARD_ERROR_IOERROR";
			case SDCARD_ERROR_OUTOFMEMORY:
				return "SDCARD_ERROR_OUTOFMEMORY";
			case SDCARD_ERROR_FATALERROR:
				return "SDCARD_ERROR_FATALERROR";
			default:
				return "SDCARD_ERROR_???";
			}
		}

		static void check(const char* function, int code)
		{
			if (code < 0)
			{
				Sys_Error("check: %s failed with code %d (%s).\n", function, code, code_to_string(code));
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
					printf("File %u: %s\n", index, file.path);
				}
			}
		}
	}
}

using namespace quake;
using namespace quake::system;

int Sys_FileOpenRead (const char *path, int *hndl)
{
	init();

	*hndl = -1;

	// Translate the file name into one which works for the SDCARD library?
	char translated_path[MAX_OSPATH + 1];
	translate_path(path, translated_path);
	Sys_Printf("Sys_FileOpenRead: %s\n", translated_path);

	// Find an unused file slot.
	for (std::size_t file_index = 0; file_index < file_count; ++file_index)
	{
		// Is the file slot free?
		file_slot& file = files[file_index];
		if (!file.in_use())
		{
			// Open the file.
			file.handle = SDCARD_OpenFile(translated_path, "rb");
			if (!file.handle)
			{
				Sys_Printf("\tFile not found");
				return -1;
			}

			// Set up the file info.
			strcpy(file.path, translated_path);

			// Done.
			*hndl = file_index;
			const int length = SDCARD_GetFileSize(file.handle);
			check("SDCARD_GetFileSize", length);
			return length;
		}
	}

	// This is bad.
	Sys_Error("Out of file slots");
	return -1;
}

int Sys_FileOpenWrite (const char *path)
{
	init();

	// Translate the file name into one which works for the SDCARD library?
	char translated_path[MAX_OSPATH + 1];
	translate_path(path, translated_path);
	Sys_Printf("Sys_FileOpenWrite: %s\n", translated_path);

	// Find an unused file slot.
	for (std::size_t file_index = 0; file_index < file_count; ++file_index)
	{
		// Is the file slot free?
		file_slot& file = files[file_index];
		if (!file.in_use())
		{
			// Open the file.
			file.handle = SDCARD_OpenFile(translated_path, "wb");
			if (!file.handle)
			{
				Sys_Error("File can't be opened for writing");
				return -1;
			}

			// Set up the file info.
			strcpy(file.path, translated_path);

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
	check("SDCARD_CloseFile", SDCARD_CloseFile(file.handle));

	// Clear the file.
	memset(&file, 0, sizeof(file));

//	Sys_Printf("Sys_FileClose OK\n");
}

void Sys_FileSeek (int handle, int position)
{
	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	Sys_Printf("Sys_FileSeek: %s %d bytes\n", file.path, position);

	// Set the position.
	check("SDCARD_SeekFile", SDCARD_SeekFile(file.handle, position, SDCARD_SEEK_SET));

//	Sys_Printf("Sys_FileSeek OK\n", position);
}

int Sys_FileRead(int handle, void *dest, int count)
{
	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	Sys_Printf("Sys_FileRead: %s %d bytes\n", file.path, count);

	// Read the data directly.
	const int bytes_read = SDCARD_ReadFile(file.handle, dest, count);
	check("SDCARD_ReadFile", bytes_read);
	if (bytes_read != count)
	{
		dump();
		Sys_Error("Read failed (got %u, wanted %u)", bytes_read, count);
		return 0;
	}

	// Done.
	return count;
}

int Sys_FileWrite (int handle, const void *data, int count)
{
	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Read.
	const int bytes_written = SDCARD_WriteFile(file.handle, data, count);
	check("SDCARD_WriteFile", bytes_written);
	if (count != bytes_written)
	{
		dump();
		Sys_Error("Read failed (wrote %d, wanted %d", bytes_written, count);
		return 0;
	}
	else
	{
		return count;
	}
}

int	Sys_FileTime (const char *path)
{
	char translated_path[MAX_OSPATH + 1];
	translate_path(path, translated_path);
	Sys_Printf("Sys_FileTime: %s\n", translated_path);

	// Open the file.
	sd_file* const handle = SDCARD_OpenFile(translated_path, "rb");
	if (handle)
	{
		// Get the stats.
		SDSTAT stat;
		memset(&stat, 0, sizeof(stat));
		check("SDCARD_GetStats", SDCARD_GetStats(handle, &stat));

		// Return the time.
		return mktime(&stat.ftime);
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
