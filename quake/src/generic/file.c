/*
Copyright (C) 2007 Peter Mackay.

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

#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "file.h"
#include "sys.h"

struct file_s
{
	int			used;
	int			handle;
	file_mode_t	mode;

	struct read_mode_fields
	{
		int			length;
		int			position;
	} read;
};

static file_t		files[64];
static const size_t	file_count	= sizeof(files) / sizeof(files[0]);

static file_t* find_free_file_slot(void)
{
	size_t i;

	// Find a free file slot.
	for (i = 0; i < file_count; ++i)
	{
		file_t* const file = &files[i];
		if (!file->used)
		{
			// Use this file slot.
			memset(file, 0, sizeof(*file));
			return file;
		}
	}
}

file_t* File_Open(const char* path, file_mode_t mode)
{
	// Get a free file slot.
	file_t* const file = find_free_file_slot();

	// Open the file.
	switch (mode)
	{
	case file_mode_read:
		{
			int handle = -1;
			const int length = Sys_FileOpenRead(path, &handle);
			if (length >= 0)
			{
				file->used			= 1;
				file->handle		= handle;
				file->mode			= mode;
				file->read.length	= length;
				file->read.position	= 0;
				return file;
			}
		}
		break;

	case file_mode_write:
		{
			const int handle = Sys_FileOpenWrite(path);
			if (handle >= 0)
			{
				file->used		= 1;
				file->handle	= handle;
				file->mode		= mode;
				return file;
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

void File_Close(file_t* file)
{
	Sys_FileClose(file->handle);
	memset(file, 0, sizeof(*file));
}

int File_EOF(file_t* file)
{
	return file->read.position >= file->read.length;
}

int File_GetChar(file_t* file)
{
	if (File_EOF(file))
	{
		return EOF;
	}
	else
	{
		char c;
		if (Sys_FileRead(file->handle, &c, 1) != 1)
		{
			return EOF;
		}
		else
		{
			++file->read.position;
			return c;
		}
	}
}

void File_PrintF(file_t* file, const char* format, ...)
{
	va_list args;
	char	buffer[1024];
	int		len;
	
	va_start(args, format);
	len = vsnprintf(buffer, sizeof(buffer) - 1, format, args);
	va_end(args);

	if (len > (sizeof(buffer) - 1))
	{
		Sys_Error("Error during File_PrintF");
	}

	Sys_FileWrite(file->handle, buffer, len);
}

#ifndef __GNUC__

// This is a way to do _vsscanf without using fancy stack tricks or using the
// "_input" method provided by Microsoft, which is no longer exported as of .NET.
// The function has a limit of 25 arguments (or less if you run out of stack space),
//  but how many arguments do you need?
static int vsscanf( const char *buffer, const char *format, va_list arg_ptr )
{
	int i, ret;
	void *arg_arr[25];

	for ( i = 0; i < 25; i++ )
	{
		arg_arr[i] = va_arg( arg_ptr, void * );
	}

	// This is lame, but the extra arguments won't be used by sscanf //
	ret = sscanf( buffer, format, arg_arr[0], arg_arr[1], arg_arr[2], arg_arr[3],
		arg_arr[4], arg_arr[5], arg_arr[6], arg_arr[7], arg_arr[8], arg_arr[9],
		arg_arr[10], arg_arr[11], arg_arr[12], arg_arr[13], arg_arr[14],
		arg_arr[15], arg_arr[16], arg_arr[17], arg_arr[18], arg_arr[19],
		arg_arr[20], arg_arr[21], arg_arr[22], arg_arr[23], arg_arr[24] );

	return ret;
}

#endif

void File_ScanF(file_t* file, const char* format, ...)
{
	char	buffer[1024];
	va_list	args;
	int		c;
	char*	dst	= buffer;

	// Ignore empty lines.
	do
	{
		c = File_GetChar(file);
	}
	while ((c == '\r') || (c == '\n'));

	// EOF?
	if (c == EOF)
	{
		return;
	}

	// Read until we get to a CR.
	memset(buffer, 0, sizeof(buffer));
	while (dst != &buffer[sizeof(buffer)])
	{
		*dst++ = c;

		c = File_GetChar(file);
		if ((c == EOF) || (c == '\r') || (c == '\n'))
		{
			break;
		}
	}
	
	va_start(args, format);
	vsscanf(buffer, format, args);
	va_end(args);
}
