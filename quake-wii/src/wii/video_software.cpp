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

// TODO: someone could please test it with a PAL TV? That includes going underwater, for the warp buffer.

// Standard includes.
#include <cstddef>
#include <malloc.h>
#include <algorithm>

// OGC includes.
#include <ogc/cache.h>
#include <ogc/gx.h>
#include <ogc/gx_struct.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/video_types.h>

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
		extern pixel_pair	(*xfb)[][640];
		extern GXRModeObj*	rmode;
	}

	namespace video
	{
		using main::rmode;
		using main::xfb;

		// Quake constants.
		static const size_t		max_screen_width	= 640;
		static const size_t		max_screen_height	= 528;
		static const size_t		surface_cache_size	= SURFCACHE_SIZE_AT_320X200 + ((max_screen_width - 320) * (max_screen_height - 200) * 3);

		// Quake render buffers.
		static pixel_t			render_buffer[max_screen_height][max_screen_width];
		static short			depth_buffer[max_screen_height * max_screen_width];
		static unsigned char	surface_cache[surface_cache_size];

		// GX constants.
		static const size_t		fifo_size			= 1024 * 256;
		static const size_t		texture_width		= max_screen_width;
		static const size_t		texture_height		= max_screen_height;
		static const size_t		texture_tile_width	= 8;
		static const size_t		texture_tile_height	= 4;
		static const size_t		texture_width_in_tiles	= texture_width / texture_tile_width;
		static const size_t		texture_height_in_tiles	= texture_height / texture_tile_height;

		// GX types.
		typedef u8 texture_tile[texture_tile_height][texture_tile_width];
		typedef texture_tile texture_tile_row[texture_width_in_tiles];
		typedef texture_tile_row texture_t[texture_height_in_tiles];

		// GX globals.
		static void*			gp_fifo;
		static u16				palette_data[256] ATTRIBUTE_ALIGN(32);
		static GXTlutObj		palette;
		static texture_t		texture_data ATTRIBUTE_ALIGN(32);
		static GXTexObj			texture;

		static inline void copy_tile_row(u8* dst_row, const pixel_t* src_row)
		{
			dst_row[0] = src_row[0];
			dst_row[1] = src_row[1];
			dst_row[2] = src_row[2];
			dst_row[3] = src_row[3];
			dst_row[4] = src_row[4];
			dst_row[5] = src_row[5];
			dst_row[6] = src_row[6];
			dst_row[7] = src_row[7];
		}

		static inline void copy_tile(texture_tile* tile, size_t tile_x, size_t tile_y)
		{
			const size_t	src_x1	= tile_x * texture_tile_width;
			const size_t	src_y1	= tile_y * texture_tile_height;

			copy_tile_row(&(*tile)[0][0], &render_buffer[src_y1][src_x1]);
			copy_tile_row(&(*tile)[1][0], &render_buffer[src_y1 + 1][src_x1]);
			copy_tile_row(&(*tile)[2][0], &render_buffer[src_y1 + 2][src_x1]);
			copy_tile_row(&(*tile)[3][0], &render_buffer[src_y1 + 3][src_x1]);
		}

		static void copy_texture()
		{
			const size_t	src_w_in_tiles	= std::min(vid.width / texture_tile_width, texture_width_in_tiles);
			const size_t	src_h_in_tiles	= std::min(vid.height / texture_tile_height, texture_height_in_tiles);

			for (size_t tile_y = 0; tile_y < src_h_in_tiles; ++tile_y)
			{
				texture_tile_row* const tile_row = &texture_data[tile_y];

				for (size_t tile_x = 0; tile_x < src_w_in_tiles; ++tile_x)
				{
					texture_tile* const tile = &(*tile_row)[tile_x];
					copy_tile(tile, tile_x, tile_y);
				}
			}
		}
	}
}

using namespace quake;
using namespace quake::video;

cvar_t  vid_tvborder = {"vid_tvborder","0", (qboolean)true};

// Globals required by Quake.
unsigned short	d_8to16table[256];

void VID_SetPalette(unsigned char* palette)
{
	for (unsigned int colour = 0; colour < 256; ++colour)
	{
		const unsigned int r = *palette++ >> 3;
		const unsigned int g = *palette++ >> 2;
		const unsigned int b = *palette++ >> 3;
		palette_data[colour] = b | (g << 5) | (r << 11);
	}

	DCFlushRange(&palette_data, sizeof(palette_data));
	GX_LoadTlut(&::palette, GX_TLUT0);
}

void VID_ShiftPalette(unsigned char* palette)
{
	VID_SetPalette(palette);
}

void VID_Init(unsigned char* palette)
{
	// Initialise GX.
	gp_fifo = MEM_K0_TO_K1(memalign(32, fifo_size));
	memset(gp_fifo, 0, fifo_size);
	GX_Init(gp_fifo, fifo_size);

	GXColor	backgroundColor	= {0, 0, 0,	255};

	GX_SetCopyClear(backgroundColor, GX_MAX_Z24);
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	GX_SetDispCopyYScale((f32)rmode->xfbHeight/(f32)rmode->efbHeight);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,rmode->xfbHeight);
	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,
		GX_TRUE,rmode->vfilter);

	const bool field_rendering = (rmode->viHeight == (2 * rmode->xfbHeight));
	GX_SetFieldMode(rmode->field_rendering, field_rendering ? GX_ENABLE : GX_DISABLE);

	GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_2_2);

	Mtx	projection;
	guOrtho(projection, 1, 0, 0, 1, -1, 1);
	GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,	GX_POS_XYZ,	GX_F32,	0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32,	0);

	// Get some constants.
	const size_t	screen_width	= field_rendering ? (main::rmode->fbWidth / 2) : main::rmode->fbWidth;
	const size_t	screen_height	= main::rmode->xfbHeight;

	const float s = screen_width / static_cast<float>(texture_width);
	const float t = screen_height / static_cast<float>(texture_height);

	static float sts[4][2] ATTRIBUTE_ALIGN(32) =
	{
		{0, t},
		{s, t},
		{s, 0},
		{0, 0}
	};

	GX_SetArray(GX_VA_TEX0,	sts, sizeof(sts[0]));
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetCullMode(GX_CULL_NONE);

	GX_InitTlutObj(&::palette, &palette_data[0], 1, GX_TLUT_256);

	GX_InitTexObjCI(
		&texture, &texture_data[0][0], texture_width, texture_height, GX_TF_CI8, GX_CLAMP, GX_CLAMP, GX_FALSE, GX_TLUT0);
	GX_InitTexObjLOD(
		&texture, GX_NEAR, GX_NEAR, 0, 0, 0, GX_FALSE, GX_FALSE, GX_ANISO_1);
	
	// Set up Quake's video parameters.
	vid.aspect			= vid.aspect = (static_cast<float>(screen_height) / static_cast<float>(screen_width)) * (4.0f / 3.0f);
	vid.buffer			= &render_buffer[0][0];
	vid.colormap		= host_colormap;
	vid.colormap16		= d_8to16table;
	vid.conbuffer		= &render_buffer[0][0];
	vid.conheight		= screen_height;
	vid.conrowbytes		= sizeof(render_buffer[0]);
	vid.conwidth		= screen_width;
	vid.direct			= &render_buffer[0][0];
	vid.fullbright		= 256 - LittleLong(*((int *) vid.colormap + 2048));
	vid.height			= screen_height;
	vid.maxwarpheight	= WARP_HEIGHT;
	vid.maxwarpwidth	= WARP_WIDTH;
	vid.numpages		= 1;
	vid.recalc_refdef	= 0;
	vid.rowbytes		= sizeof(render_buffer[0]);
	vid.width			= screen_width;

	// Set the z buffer address.
	d_pzbuffer			= &depth_buffer[0];

	// Initialise the surface cache.
	D_InitCaches(surface_cache, sizeof(surface_cache));

	// Set the palette.
	VID_SetPalette(palette);

	Cvar_RegisterVariable (&vid_tvborder);
}

void VID_Shutdown(void)
{
	// Shut down the display.

	// Free the FIFO.
	free(MEM_K1_TO_K0(gp_fifo));
	gp_fifo = 0;
}

void VID_Update(vrect_t* rects)
{
	static float vertices[4][3] =
	{
		{0, 0, 0},
		{1, 0, 0},
		{1, 1, 0},
		{0, 1, 0}
	};

	// Copy Quake's frame buffer into the texture data and convert from linear
	// to tiled.
	copy_texture();

	// Flush the CPU data cache.
	DCFlushRange(&texture_data, sizeof(texture_data));

	// Finish up any graphics operations.
	GX_Flush();

	// Clear the texture cache.
	GX_InvalidateTexAll();

	// Load the texture.
	GX_LoadTexObj(&texture, GX_TEXMAP0);

	// Draw a fullscreen quad.
	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, 4);

	GX_Position3f32(vertices[0][0], vertices[0][1] + vid_tvborder.value, vertices[0][2]);
	GX_TexCoord1x8(0);
	GX_Position3f32(vertices[1][0], vertices[1][1] + vid_tvborder.value, vertices[1][2]);
	GX_TexCoord1x8(1);
	GX_Position3f32(vertices[2][0], vertices[2][1] - vid_tvborder.value, vertices[2][2]);
	GX_TexCoord1x8(2);
	GX_Position3f32(vertices[3][0], vertices[3][1] - vid_tvborder.value, vertices[3][2]);
	GX_TexCoord1x8(3);

	GX_End();

	// Mark the end of drawing.
	GX_DrawDone();

	// Start copying the frame buffer every vsync.
	GX_CopyDisp(xfb, GX_TRUE);
}

void D_BeginDirectRect(int x, int y, byte* pbitmap, int width, int height)
{
}

void D_EndDirectRect(int x, int y, int width, int height)
{
}

void R_TranslatePlayerSkin (int playernum)
{
}
