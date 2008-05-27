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

#ifndef FILE_H
#define FILE_H

typedef struct file_s file_t;

typedef enum file_mode_e
{
	file_mode_read,
	file_mode_write
} file_mode_t;

file_t* File_Open(const char* path, file_mode_t mode);
void File_Close(file_t* file);
int File_EOF(file_t* file);
int File_GetChar(file_t* file);
void File_PrintF(file_t* file, const char* format, ...);
void File_ScanF(file_t* file, const char* format, ...);

#endif
