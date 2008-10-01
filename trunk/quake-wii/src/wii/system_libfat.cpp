/*
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
#include <cctype>
#include <cstddef>

#include <fat.h>

#define DISABLE_WRITING 1

extern "C"
{
#include <sys/stat.h>

#include "../generic/quakedef.h"
}

namespace quake
{
	namespace system
	{
		struct file_slot
		{
			FILE*	handle;
			char	path[MAX_OSPATH + 1];

			bool in_use() const
			{
				return handle != 0;
			}
		};

		static const std::size_t		file_count		= 512;
		static file_slot				files[file_count];

		static void init()
		{
			static bool initialised = false;
			if (!initialised)
			{
				if (!fatInitDefault())
					Sys_Error("Filesystem initialization failed\n");

				for (size_t i = 0; i < file_count; i++)
					files[i].handle = NULL;

				initialised = true;
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

		static void dump()
		{
			// Dump files.
			for (size_t index = 0; index < file_count; ++index)
			{
				const file_slot& file = files[index];
				if (file.in_use())
					printf("File %u: %s\n", index, file.path);
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

	Sys_Printf("Sys_FileOpenRead: %s\n", path);

	// Get the file info.
	struct stat file_info;
	int res = stat(path, &file_info);
	if (res != 0)
	{
		return -1;
	}

	// Find an unused file slot.
	for (std::size_t file_index = 0; file_index < file_count; ++file_index)
	{
		// Is the file slot free?
		file_slot& file = files[file_index];
		if (!file.in_use())
		{
			// Open the file.
			file.handle = fopen(path, "rb");
			if (!file.handle)
			{
				Sys_Error("\tFile not found");
				return -1;
			}

			// Set up the file info.
			strcpy(file.path, path);

			// Done.
			*hndl = file_index;
			return file_info.st_size;
		}
	}

	// This is bad.
	Sys_Error("Out of file slots");
	return -1;
}

int Sys_FileOpenWrite (const char *path)
{
#if DISABLE_WRITING
	return -1;
#endif
	init();

	Sys_Printf("Sys_FileOpenWrite: %s\n", path);

	// Find an unused file slot.
	for (std::size_t file_index = 0; file_index < file_count; ++file_index)
	{
		// Is the file slot free?
		file_slot& file = files[file_index];
		if (!file.in_use())
		{
			// Open the file.
			file.handle = fopen(path, "wb");
			if (!file.handle)
			{
				Sys_Error("File can't be opened for writing");
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
	fclose(file.handle);

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
	fseek(file.handle, position, SEEK_SET);

//	Sys_Printf("Sys_FileSeek OK\n", position);
}

int Sys_FileRead(int handle, void *dest, int count)
{
//	Sys_Printf("Sys_FileRead: %d bytes\n", count);

	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	const int bytes_read = fread(dest, count, 1, file.handle);
	if (bytes_read != 1)
	{
		dump();
		Sys_Error("Read failed (got %u, wanted %u)", bytes_read, 1);
		return 0;
	}

	return count;
}

int Sys_FileWrite (int handle, const void *data, int count)
{
#if DISABLE_WRITING
	return 0;
#endif
	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Read.
	const int bytes_written = fwrite(data, count, 1, file.handle);
	if (bytes_written != 1)
	{
		dump();
		Sys_Error("Write failed (wrote %d, wanted %d", bytes_written, 1);
		return 0;
	}
	else
	{
		return count;
	}
}

int	Sys_FileTime (const char *path)
{
	Sys_Printf("Sys_FileTime: %s\n", path);

	// Get the file info.
	struct stat file_info;
	int res = stat(path, &file_info);
	if (res != 0)
	{
		return -1;
	}
	else
	{
		return file_info.st_mtime;
	}
}

void Sys_mkdir (const char *path)
{
	Sys_Error("Mkdir: %s\n", path);
}
