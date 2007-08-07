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

// Standard includes.
#include <cstddef>

// OGC includes.
#include <gccore.h>

extern "C"
{
#include "../generic/quakedef.h"
#include "../generic/d_local.h"
}

namespace quake
{
	namespace main
	{
		// Types.
		typedef u32 pixel_pair;

		// Globals.
		extern pixel_pair	(*xfb)[][320];
		extern GXRModeObj*	rmode;
	}

	namespace video
	{
		// Types.
		using main::pixel_pair;

		// Constants.
		static const size_t		max_screen_width	= 640;
		static const size_t		max_screen_height	= 528;
		static const size_t		surface_cache_size	= SURFCACHE_SIZE_AT_320X200 + ((max_screen_width - 320) * (max_screen_height - 200) * 3);

		// Render buffers.
		static pixel_t			render_buffer[max_screen_height][max_screen_width];
		static short			depth_buffer[max_screen_height][max_screen_width];
		static unsigned char	surface_cache[surface_cache_size];

		// Colour fudging.
		static	unsigned int	gamma[256];
		static	pixel_pair		palette[256][256];

		// Packs the component_ts into a pixel pair.
		static inline pixel_pair pack(unsigned int y1, unsigned int cb, unsigned int y2, unsigned int cr)
		{
			return (y1 << 24) | (cb << 16) | (y2 << 8) | cr;
		}

		static inline unsigned int calc_y(unsigned int r, unsigned int g, unsigned int b)
		{
			return gamma[(299 * r + 587 * g + 114 * b) / 1000];
		}

		static inline unsigned int calc_cb(unsigned int r, unsigned int g, unsigned int b)
		{
			return (-16874 * r - 33126 * g + 50000 * b + 12800000) / 100000;
		}

		static inline unsigned int calc_cr(unsigned int r, unsigned int g, unsigned int b)
		{
			return (50000 * r - 41869 * g - 8131 * b + 12800000) / 100000;
		}

		// Blits some pixels from the render buffer to the GameCube's frame buffer.
		template <size_t magnification>
		static inline void blit_pixels(const pixel_t*& src, pixel_pair*& dst);

		// Specialise for single width pixels.
		template <>
		static inline void blit_pixels<1>(const pixel_t*& src, pixel_pair*& dst)
		{
			// Read an integer at a time.
			const unsigned int i	= *(reinterpret_cast<const u32*&>(src))++;
			const unsigned int i1	= i >> 24;
			const unsigned int i2	= (i >> 16) & 0xff;
			const unsigned int i3	= (i >> 8) & 0xff;
			const unsigned int i4	= i & 0xff;

			// Write out 2 pixels.
			*dst++ = palette[i1][i2];
			*dst++ = palette[i3][i4];
		}

		// Specialise for double width pixels.
		template <>
		static inline void blit_pixels<2>(const pixel_t*& src, pixel_pair*& dst)
		{
			// Read an integer at a time.
			const unsigned int i	= *(reinterpret_cast<const u32*&>(src))++;
			const unsigned int i1	= i >> 24;
			const unsigned int i2	= (i >> 16) & 0xff;
			const unsigned int i3	= (i >> 8) & 0xff;
			const unsigned int i4	= i & 0xff;

			// Write out 4 pixels.
			*dst++ = palette[i1][i1];
			*dst++ = palette[i2][i2];
			*dst++ = palette[i3][i3];
			*dst++ = palette[i4][i4];
		}

		// Blits one row from the render buffer to the GameCube's frame buffer.
		template <size_t magnification>
		static inline void blit_row(const pixel_t* src_row_start, const pixel_t* src_row_end, pixel_pair* dst_row_start)
		{
			const pixel_t*	src = src_row_start;
			pixel_pair*		dst = dst_row_start;
			while (src != src_row_end)
			{
				blit_pixels<magnification>(src, dst);
			}
		}

		// Blits the screen from the render buffer to the GameCube's frame buffer.
		template <size_t magnification>
		static void blit_screen()
		{
			// Constants.
			const unsigned int	w	= vid.width;
			const unsigned int	h	= vid.height;

			// For each row...
			for (unsigned int row = 0; row < h; ++row)
			{
				// Blit this row.
				blit_row<magnification>(
					&render_buffer[row][0],
					&render_buffer[row][w],
					&(*main::xfb)[row][0]);
			}
		}
	}
}

using namespace quake;
using namespace quake::video;

// Globals required by Quake.
unsigned short	d_8to16table[256];

void VID_SetPalette(unsigned char* palette)
{
	// How to store the components.
	struct ycbcr
	{
		unsigned char y;
		unsigned char cb;
		unsigned char cr;
	};
	ycbcr components[256];

	// Build the YCBCR table.
	for (unsigned int component = 0; component < 256; ++component)
	{
		const unsigned int r = *palette++;
		const unsigned int g = *palette++;
		const unsigned int b = *palette++;

		components[component].y		= calc_y(r, g, b);
		components[component].cb	= calc_cb(r, g, b);
		components[component].cr	= calc_cr(r, g, b);
	}

	// Build the Y1CBY2CR palette from the YCBCR table.
	for (unsigned int left = 0; left < 256; ++left)
	{
		const unsigned int	y1	= components[left].y;
		const unsigned int	cb1	= components[left].cb;
		const unsigned int	cr1	= components[left].cr;
		
		for (unsigned int right = 0; right < 256; ++right)
		{
			const unsigned int	cb	= (cb1 + components[right].cb) >> 1;
			const unsigned int	cr	= (cr1 + components[right].cr) >> 1;

			video::palette[left][right] = pack(y1, cb, components[right].y, cr);
		}
	}
}

void VID_ShiftPalette(unsigned char* palette)
{
	VID_SetPalette(palette);
}

void VID_Init(unsigned char* palette)
{
	// Set up the gamma table.
	const float factor = 0.5f;
	for (unsigned int in = 0; in < 256; ++in)
	{
		video::gamma[in] = static_cast<unsigned int>(255.0f * powf(in / 255.0f, factor));
	}

	// Get some constants.
	size_t			screen_width	= main::rmode->fbWidth;
	const size_t	screen_height	= main::rmode->xfbHeight;
	if (main::rmode->xfbMode == VI_XFBMODE_SF)
	{
		screen_width /= 2;
	}

	// Set up Quake's video parameters.
	vid.aspect			= vid.aspect = (static_cast<float>(screen_height) / static_cast<float>(screen_width)) * (4.0f / 3.0f);
	vid.buffer			= &render_buffer[0][0];
	vid.colormap		= host_colormap;
	vid.colormap16		= d_8to16table;
	vid.conbuffer		= &render_buffer[0][0];
	vid.conheight		= screen_height;
	vid.conrowbytes		= max_screen_width;
	vid.conwidth		= screen_width;
	vid.direct			= &render_buffer[0][0];
	vid.fullbright		= 256 - LittleLong(*((int *) vid.colormap + 2048));
	vid.height			= screen_height;
	vid.maxwarpheight	= WARP_HEIGHT;
	vid.maxwarpwidth	= WARP_WIDTH;
	vid.numpages		= 1;
	vid.recalc_refdef	= 0;
	vid.rowbytes		= max_screen_width;
	vid.width			= screen_width;
	
	// Set the z buffer address.
	d_pzbuffer			= &depth_buffer[0][0];

	// Initialise the surface cache.
	D_InitCaches(surface_cache, sizeof(surface_cache));

	// Set the palette.
	VID_SetPalette(palette);
}

void VID_Shutdown(void)
{
	// Shut down the display.
}

void VID_Update(vrect_t* rects)
{
	// Single field?
	if (main::rmode->xfbMode == VI_XFBMODE_SF)
	{
		// Double magnification.
		blit_screen<2>();
	}
	else
	{
		// Single magnification.
		blit_screen<1>();
	}
}

void D_BeginDirectRect(int x, int y, byte* pbitmap, int width, int height)
{
}

void D_EndDirectRect(int x, int y, int width, int height)
{
}
