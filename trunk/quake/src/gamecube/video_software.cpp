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
		typedef unsigned char	component_t;

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
			return (299 * r + 587 * g + 114 * b) / 1000;
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
			const unsigned int i = *(reinterpret_cast<const u32*&>(src))++;

			// Write out 2 pixels.
			const unsigned int i1 = i >> 24;
			const unsigned int i2 = (i >> 16) & 0xff;
			*dst++ = palette[i1][i2];
			const unsigned int i3 = (i >> 8) & 0xff;
			const unsigned int i4 = i & 0xff;
			*dst++ = palette[i3][i4];
		}

		// Specialise for double width pixels.
		template <>
		static inline void blit_pixels<2>(const pixel_t*& src, pixel_pair*& dst)
		{
			// Read an integer at a time.
			const unsigned int i = *(reinterpret_cast<const u32*&>(src))++;

			// Write out 4 pixels.
			const unsigned int i1 = i >> 24;
			*dst++ = palette[i1][i1];
			const unsigned int i2 = (i >> 16) & 0xff;
			*dst++ = palette[i2][i2];
			const unsigned int i3 = (i >> 8) & 0xff;
			*dst++ = palette[i3][i3];
			const unsigned int i4 = i & 0xff;
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
			const unsigned int	src_row_length	= vid.width;
			const pixel_t (* const src_row_end)[max_screen_width] = &render_buffer[vid.height];

			// Variables which change per row.
			const pixel_t (*src_row)[max_screen_width]	= &render_buffer[0];
			pixel_pair (*dst_row)[320]					= &(*main::xfb)[0];

			// For each row...
			while (src_row != src_row_end)
			{
				// Blit this row.
				blit_row<magnification>(&(*src_row)[0], &(*src_row)[src_row_length], &(*dst_row++)[0]);

				// Next row.
				++src_row;
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
		component_t y;
		component_t cb;
		component_t cr;
	};
	ycbcr components[256];

	// Build the YCBCR table.
	const ycbcr* const components_end = &components[256];
	for (ycbcr* component = &components[0]; component != components_end; ++component)
	{
		const unsigned int r = video::gamma[*palette++];
		const unsigned int g = video::gamma[*palette++];
		const unsigned int b = video::gamma[*palette++];

		component->y	= calc_y(r, g, b);
		component->cb	= calc_cb(r, g, b);
		component->cr	= calc_cr(r, g, b);
	}

	// Build the Y1CBY2CR palette from the YCBCR table.
	const ycbcr*	left_component				= &components[0];
	pixel_pair		(*left_pixel_pairs)[256]	= &video::palette[0];
	while (left_component != components_end)
	{
		const unsigned int	y1	= left_component->y;
		const unsigned int	cb1	= left_component->cb;
		const unsigned int	cr1	= left_component->cr;
		
		pixel_pair*	packed_pixel_pair	= &(*left_pixel_pairs)[0];
		for (const ycbcr* right_component = &components[0]; right_component != components_end; ++right_component)
		{
			const unsigned int	cb	= (cb1 + right_component->cb) >> 1;
			const unsigned int	cr	= (cr1 + right_component->cr) >> 1;
			*packed_pixel_pair++ = pack(y1, cb, right_component->y, cr);
		}

		++left_component;
		++left_pixel_pairs;
	}
}

void VID_ShiftPalette(unsigned char* palette)
{
	VID_SetPalette(palette);
}

void VID_Init(unsigned char* palette)
{
	// Set up the gamma table.
	const float factor = 0.75f;
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
	vid.conbuffer		= vid.buffer;
	vid.conheight		= screen_height;
	vid.conrowbytes		= max_screen_width;
	vid.conwidth		= screen_width;
	vid.direct			= vid.buffer;
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

	// Start a render.

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
