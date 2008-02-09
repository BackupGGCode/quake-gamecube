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
#include <climits>
#include <cstddef>
#include <vector>

// SDL includes.
#include <SDL_mouse.h>
#include <SDL_video.h>

extern "C"
{
#include "../generic/quakedef.h"
#include "../generic/d_local.h"
}

namespace quake
{
	namespace video
	{
		// Screen.
		static SDL_Surface* screen;
		static SDL_Surface*	back_buffer;

		// Render buffers.
		static std::vector<short>			depth_buffer;
		static std::vector<unsigned char>	surface_cache;
	}
}

using namespace quake;
using namespace quake::video;

// Globals required by Quake.
unsigned short	d_8to16table[256];

void VID_SetPalette(unsigned char* palette)
{
	SDL_Color colors[256];
	const SDL_Color* const	dst_end	= &colors[256];
	for (SDL_Color* dst = colors; dst != dst_end; ++dst)
	{
		dst->r = *palette++;
		dst->g = *palette++;
		dst->b = *palette++;
	}
	SDL_SetColors(screen, colors, 0, 256);
	if (screen != back_buffer)
	{
		SDL_SetColors(back_buffer, colors, 0, 256);
	}
}

void VID_ShiftPalette(unsigned char* palette)
{
	VID_SetPalette(palette);
}

void VID_Init(unsigned char* palette)
{
	// Set the window title.
#ifdef _DEBUG
	const char* const title = "Quake (Debug)";
#else
	const char* const title = "Quake";
#endif
	SDL_WM_SetCaption(title, title);

	// Create the screen.
	screen = SDL_SetVideoMode(640, 480, 8, SDL_SWSURFACE);
	if (!screen)
	{
		Sys_Error("SDL_SetVideoMode failed (%s)", SDL_GetError());
		return;
	}

	// Hide the cursor if full screen.
	if (screen->flags & SDL_FULLSCREEN)
	{
		SDL_ShowCursor(0);
	}

	// Quake needs write access to the screen at all times.
	if (SDL_MUSTLOCK(screen))
	{
		back_buffer = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h, 8, 0, 0, 0, 0);
		if (!back_buffer)
		{
			Sys_Error("SDL_CreateRGBSurface failed (%s)", SDL_GetError());
			return;
		}
		
		if (SDL_MUSTLOCK(back_buffer))
		{
			Sys_Error("Back buffer requires locking");
			return;
		}
	}
	else
	{
		back_buffer = screen;
		++back_buffer->refcount;
	}

	// Allocate the buffers.
	//render_buffer.resize(screen->w * screen->h);
	depth_buffer.resize(back_buffer->w * back_buffer->h);
	surface_cache.resize(D_SurfaceCacheForRes(back_buffer->w, back_buffer->h));

	// Set up Quake's video parameters.
	vid.aspect			= vid.aspect = (static_cast<float>(screen->h) / static_cast<float>(screen->w)) * (4.0f / 3.0f);
	vid.buffer			= static_cast<pixel_t*>(back_buffer->pixels);
	vid.colormap		= host_colormap;
	vid.colormap16		= d_8to16table;
	vid.conbuffer		= static_cast<pixel_t*>(back_buffer->pixels);
	vid.conheight		= back_buffer->h;
	vid.conrowbytes		= back_buffer->pitch;
	vid.conwidth		= back_buffer->w;
	vid.direct			= static_cast<pixel_t*>(back_buffer->pixels);
	vid.fullbright		= 256 - LittleLong(*((int *) vid.colormap + 2048));
	vid.height			= back_buffer->h;
	vid.maxwarpheight	= WARP_HEIGHT;
	vid.maxwarpwidth	= WARP_WIDTH;
	vid.numpages		= 2;
	vid.recalc_refdef	= 0;
	vid.rowbytes		= back_buffer->pitch;
	vid.width			= back_buffer->w;
	
	// Set the z buffer address.
	d_pzbuffer			= &depth_buffer.at(0);

	// Initialise the surface cache.
	D_InitCaches(&surface_cache.at(0), surface_cache.size());

	// Set the palette.
	VID_SetPalette(palette);
}

void VID_Shutdown(void)
{
	SDL_FreeSurface(back_buffer);
	back_buffer = 0;
	screen = 0;
}

void VID_Update(vrect_t* rects)
{
	if (back_buffer != screen)
	{
		SDL_BlitSurface(back_buffer, 0, screen, 0);
	}
	SDL_Flip(screen);
}

void D_BeginDirectRect(int x, int y, byte* pbitmap, int width, int height)
{
}

void D_EndDirectRect(int x, int y, int width, int height)
{
}
