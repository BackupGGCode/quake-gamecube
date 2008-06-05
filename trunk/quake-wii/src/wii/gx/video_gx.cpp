/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// ELUTODO: horizontal tv border
#include <malloc.h>
#include <ogc/cache.h>
#include <ogc/gx.h>
#include <ogc/gx_struct.h>
#include <ogc/system.h>
#include <ogc/video.h>
#include <ogc/video_types.h>

/*#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>*/

extern "C" {
#include "../../generic/quakedef.h"
}

namespace quake
{
	namespace main
	{
		typedef u32 pixel_pair;
		extern pixel_pair       (*xfb)[][640];

		extern GXRModeObj*     rmode;
	}

	namespace video
	{
		using main::xfb;
		using main::rmode;

		static void								*gp_fifo;
		static const size_t						fifo_size = 1024 * 256;

		#define WARP_WIDTH              640
		#define WARP_HEIGHT             480

		static int scr_width, scr_height;

		static bool vidmode_active = false;
	}
}

/*-----------------------------------------------------------------------*/

unsigned short	d_8to16table[256]; // ELUTODO shouldn't be needed
unsigned		d_8to24table[256];
unsigned char	d_15to8table[65536];

float		gldepthmin, gldepthmax;

static float vid_gamma = 1.0;

/*-----------------------------------------------------------------------*/

// ELUTODO
Mtx view;
Mtx44 perspective;
Mtx model, modelview;

using namespace quake;
using namespace quake::video;

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_Shutdown(void)
{
	if (vidmode_active)
	{
		// Free the FIFO.
		free(MEM_K1_TO_K0(gp_fifo));
		gp_fifo = 0;

		vidmode_active = false;
	}
}

void VID_ShiftPalette(unsigned char *p)
{
//	VID_SetPalette(p);
}

void	VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		k;
	unsigned short i;
	unsigned	*table;
	int dist, bestdist;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff00;	// 255 is transparent

	for (i=0; i < (1<<15); i++) {
		/* Maps
		000000000000000
		000000000011111 = Red  = 0x1F
		000001111100000 = Blue = 0x03E0
		111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0,k=0,bestdist=10000*10000; v<256; v++,pal+=4) {
			r1 = (int)r - (int)pal[0];
			g1 = (int)g - (int)pal[1];
			b1 = (int)b - (int)pal[2];
			dist = (r1*r1)+(g1*g1)+(b1*b1);
			if (dist < bestdist) {
				k=v;
				bestdist = dist;
			}
		}
		d_15to8table[i]=k;
	}
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	f32 yscale;
	u32 xfbHeight;

	GXColor background = {0, 0, 0, 0xff};

	// Initialise GX.
	gp_fifo = memalign(32, fifo_size);
	if (!gp_fifo)
		Sys_Error("VID_Init: !gp_fifo\n");

	gp_fifo = MEM_K0_TO_K1(gp_fifo);
	memset(gp_fifo, 0, fifo_size);
	GX_Init(gp_fifo, fifo_size);

	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, 0x00ffffff);

	// other gx setup
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	/*if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR); // ELUTODO?
	else*/
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(xfb,GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0); // ELUTODO

	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	// setup the vertex attribute table
	// describes the data
	// args: vat location 0-7, type of data, data format, size, scale
	// so for ex. in the first call we are sending position data with
	// 3 values X,Y,Z of size F32. scale sets the number of fractional
	// bits for non float data.
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_InvalidateTexAll();
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;

	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	GX_InvVtxCache();
	GX_InvalidateTexAll();
}


void GL_EndRendering (void)
{
        // Finish up any graphics operations.
        GX_Flush();

		GX_DrawDone();

		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
        // Start copying the frame buffer every vsync.
        GX_CopyDisp(xfb, GX_TRUE);

		// ELUTODO?
		VIDEO_Flush();
		VIDEO_WaitVSync();
/* ELUTODO
	glFlush();
	glXSwapBuffers(dpy, win);
*/
}

static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) == 0) {
		vid_gamma = 0.7; // default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = Q_atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}

void VID_Init(unsigned char *palette)
{
	unsigned int width = 640, height = 480;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

// interpret command-line params

// set vid parameters
	vid.conwidth = 640;
	vid.conwidth &= 0xfff8; // make it a multiple of eight
	if (vid.conwidth < 320)
		vid.conwidth = 320;
	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth*3 / 4;
	if (vid.conheight < 200)
		vid.conheight = 200;

	scr_width = rmode->fbWidth;
	scr_height = rmode->efbHeight;

	if (vid.conheight > height)
		vid.conheight = height;
	if (vid.conwidth > width)
		vid.conwidth = width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 1; // ELUTODO

	GL_Init();

	//Check_Gamma(palette);

	VID_SetPalette(palette);

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);

	vid.recalc_refdef = 1;				// force a surface cache flush

	vidmode_active = true;
}
