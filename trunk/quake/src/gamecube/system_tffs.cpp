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

extern "C"
{
#include "fileio/sdfileio.h"
#include "../generic/quakedef.h"
}

namespace quake
{
	namespace system
	{
		struct file_slot
		{
			FIL*	handle;
			char	path[MAX_OSPATH + 1];

			struct read_mode_fields
			{
				size_t	length;
				size_t	position;
				char	cache[1024];
				size_t	cache_start;
				size_t	cache_end;
			} read;

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
				SDInit();
				initialised = true;
			}
		}

		static void translate_path(const char* in, char* out)
		{
			// Copy the string.
			strcpy(out, in);

			// Convert all remaining characters.
			for (char* c = out; *c; ++c)
			{
				if (isalpha(*c))
				{
					*c = toupper(*c);
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

		static void dump()
		{
			// Dump files.
			for (size_t index = 0; index < file_count; ++index)
			{
				const file_slot& file = files[index];
				if (file.in_use())
				{
					printf("File %u: %s\n", index, file.path);
					printf("\tlength      = %u\n", file.read.length);
					printf("\tposition    = %u\n", file.read.position);
					printf("\tcache_start = %u\n", file.read.cache_start);
					printf("\tcache_end   = %u\n", file.read.cache_end);
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

	// Get the file info.
	FILINFO file_info;
	FRESULT res = f_stat(translated_path, &file_info);
	if (res != FR_OK)
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
			file.handle = gen_fopen(translated_path, "rb");
			if (!file.handle)
			{
				Sys_Error("\tFile not found");
				return -1;
			}

			// Set up the file info.
			strcpy(file.path, translated_path);
			file.read.length = file_info.fsize;

			// Done.
			*hndl = file_index;
			return file.read.length;
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
			file.handle = gen_fopen(translated_path, "wb");
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
	gen_fclose(file.handle);

	// Clear the file.
	memset(&file, 0, sizeof(file));

//	Sys_Printf("Sys_FileClose OK\n");
}

void Sys_FileSeek (int handle, int position)
{
//	Sys_Printf("Sys_FileSeek: %d bytes\n", position);

	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Seeking inside the cache?
	int offset = position - file.read.position;
	if ((offset >= 0) && ((file.read.cache_start + offset) <= file.read.cache_end))
	{
		// Bump up the cache start.
		file.read.cache_start += offset;
	}
	else
	{
		// Set the position.
		gen_fseek(file.handle, position, SEEK_SET);

		// Flush the cache.
		file.read.cache_start	= 0;
		file.read.cache_end		= 0;
	}

	// Set the position.
	file.read.position = position;

//	Sys_Printf("Sys_FileSeek OK\n", position);
}

int Sys_FileRead(int handle, void *dest, int count)
{
//	Sys_Printf("Sys_FileRead: %d bytes\n", count);

	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// While there is data left to read...
	size_t bytes_left = count;
	while (bytes_left)
	{
		// How many bytes in the cache?
		const size_t cache_used = file.read.cache_end - file.read.cache_start;

		// All the data is in the cache?
		if (cache_used >= bytes_left)
		{
			// Copy from the cache.
			memcpy(dest, &file.read.cache[file.read.cache_start], bytes_left);
			file.read.cache_start += bytes_left;
			bytes_left = 0;
		}
		// Some data cached?
		else if (cache_used > 0)
		{
			// Copy from the cache.
			memcpy(dest, &file.read.cache[file.read.cache_start], cache_used);
			file.read.cache_start += cache_used;
			dest = static_cast<char*>(dest) + cache_used;
			bytes_left -= cache_used;
		}
		// More data wanted than can fit in the cache?
		else if (bytes_left >= sizeof(file.read.cache))
		{
			// Clear the cache.
			file.read.cache_start	= 0;
			file.read.cache_end		= 0;

			// Read the data directly.
			const size_t bytes_read = gen_fread(dest, bytes_left, 1, file.handle);
			if (bytes_read != bytes_left)
			{
				dump();
				Sys_Error("Read failed (got %u, wanted %u)", bytes_read, bytes_left);
				return 0;
			}

			// Set the file pointer.
			file.read.position += bytes_read;
			bytes_left = 0;
		}
		// No data cached.
		else
		{
			// How much can be cached?
			const size_t bytes_left_in_file	= file.read.length - file.read.position;
			const size_t bytes_to_cache		= (bytes_left_in_file >= sizeof(file.read.cache)) ?
				sizeof(file.read.cache) : bytes_left_in_file;

			// Read the data into the cache.
			const size_t bytes_read = gen_fread(file.read.cache, bytes_to_cache, 1, file.handle);
			if (bytes_read != bytes_to_cache)
			{
				dump();
				Sys_Error("Read failed (got %u, wanted %u)", bytes_read, bytes_to_cache);
				return 0;
			}

			// Set the file pointer.
			file.read.position += bytes_read;

			// Set the cache.
			file.read.cache_start	= 0;
			file.read.cache_end		= bytes_to_cache;
		}
	}

	// Done.
	return count;
}

int Sys_FileWrite (int handle, const void *data, int count)
{
	// Get the file.
	file_slot& file = index_to_file(handle, __FUNCTION__);

	// Read.
	const int bytes_written = gen_fwrite(data, count, 1, file.handle);
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

	// Get the file info.
	FILINFO file_info;
	FRESULT res = f_stat(translated_path, &file_info);
	if (res != FR_OK)
	{
		return -1;
	}
	else
	{
		return file_info.ftime | (file_info.fdate << (sizeof(file_info.ftime) * 8));
	}
}

void Sys_mkdir (const char *path)
{
	Sys_Error("Mkdir: %s\n", path);
}
