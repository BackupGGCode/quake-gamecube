/*
Copyright (C) 2008 Eluan Costa Miranda

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

// Silly, dirty code. But at least I still remembered how it worked 1 year later without any effort. YAY!
// Player textures stuff goes in gx_rmisc.c

#include <ogc/cache.h>
#include <ogc/system.h>
#include <ogc/lwp_heap.h>
#include <ogc/lwp_mutex.h>

#include "../../generic/quakedef.h"

// ELUTODO: GL_Upload32 and GL_Update32 could use some optimizations
// ELUTODO: mipmap and texture filters
// ELUTODO: RGB565 when we are sure we don't need alpha to get 1 extra green bit?

#define TEXTURE_SIZE	2
#define TEXTURE_FORMAT	GX_TF_RGB5A3

cvar_t		gl_max_size = {"gl_max_size", "1024"};

gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

heap_cntrl texture_heap;
void *texture_heap_ptr;
u32 texture_heap_size;

void R_InitTextureHeap (void)
{
	u32 level, size;

	_CPU_ISR_Disable(level);
	texture_heap_ptr = SYS_GetArena2Lo();
	texture_heap_size = 39 * 1024 * 1024;
	if ((u32)texture_heap_ptr + texture_heap_size > (u32)SYS_GetArena2Hi())
	{
		_CPU_ISR_Restore(level);
		Sys_Error("texture_heap + texture_heap_size > (u32)SYS_GetArena2Hi()");
	}	
	else
	{
		SYS_SetArena2Lo(texture_heap_ptr + texture_heap_size);
		_CPU_ISR_Restore(level);
	}

	memset(texture_heap_ptr, 0, texture_heap_size);

	size = __lwp_heap_init(&texture_heap, texture_heap_ptr, texture_heap_size, PPC_CACHE_ALIGNMENT);

	Con_Printf("Allocated %dM texture heap.\n", size / (1024 * 1024));
}

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;

	R_InitTextureHeap();

	Cvar_RegisterVariable (&gl_max_size);

	numgltextures = 0;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}

void GL_Bind0 (int texnum)
{
	if (currenttexture0 == texnum)
		return;

	if (texnum < 0 || texnum >= MAX_GLTEXTURES)
	{
		Con_Printf("Invalid texture0\n");
		return;
	}

	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture0.");

	currenttexture0 = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP0);
}

void GL_Bind1 (int texnum)
{
	if (currenttexture1 == texnum)
		return;

	if (texnum < 0 || texnum >= MAX_GLTEXTURES)
	{
		Con_Printf("Invalid texture1\n");
		return;
	}

	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture1.");

	currenttexture1 = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP1);
}

void QGX_ZMode(qboolean state)
{
	if (state)
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	else
		GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_TRUE);
}

void QGX_Alpha(qboolean state)
{
	if (state)
		GX_SetAlphaCompare(GX_GREATER,0,GX_AOP_AND,GX_ALWAYS,0);
	else
		GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
}

void QGX_Blend(qboolean state)
{
	if (state)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (gltextures[i].used)
			if (!strcmp (identifier, glt->identifier))
				return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

// FIXME, temporary
static	u32	scaled[640*480];
static	u32	trans[640*480];

/*
===============
GL_Upload32
===============
*/
/*
void GL_Upload32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, x, y, s;
	u8			*pos;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Upload32: too big");

	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * sizeof(unsigned));
	}

	destination->data = __lwp_heap_allocate(&texture_heap, scaled_width * scaled_height * sizeof(unsigned));
	if (!destination->data)
		Sys_Error("GL_Upload32: Out of memory.");

	destination->scaled_width = scaled_width;
	destination->scaled_height = scaled_height;

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Upload32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Upload32: destination->data&31");

	pos = (u8 *)destination->data;
	for (y = 0; y < scaled_height; y += 4)
	{
		u8* row1 = (u8 *)&(scaled[scaled_width * (y + 0)]);
		u8* row2 = (u8 *)&(scaled[scaled_width * (y + 1)]);
		u8* row3 = (u8 *)&(scaled[scaled_width * (y + 2)]);
		u8* row4 = (u8 *)&(scaled[scaled_width * (y + 3)]);

		for (x = 0; x < scaled_width; x += 4)
		{
			u8 AR[32];
			u8 GB[32];

			for (i = 0; i < 4; i++)
			{
				u8* ptr1 = &(row1[(x + i) * 4]);
				u8* ptr2 = &(row2[(x + i) * 4]);
				u8* ptr3 = &(row3[(x + i) * 4]);
				u8* ptr4 = &(row4[(x + i) * 4]);

				AR[(i * 2) +  0] = ptr1[0];
				AR[(i * 2) +  1] = ptr1[3];
				AR[(i * 2) +  8] = ptr2[0];
				AR[(i * 2) +  9] = ptr2[3];
				AR[(i * 2) + 16] = ptr3[0];
				AR[(i * 2) + 17] = ptr3[3];
				AR[(i * 2) + 24] = ptr4[0];
				AR[(i * 2) + 25] = ptr4[3];

				GB[(i * 2) +  0] = ptr1[2];
				GB[(i * 2) +  1] = ptr1[1];
				GB[(i * 2) +  8] = ptr2[2];
				GB[(i * 2) +  9] = ptr2[1];
				GB[(i * 2) + 16] = ptr3[2];
				GB[(i * 2) + 17] = ptr3[1];
				GB[(i * 2) + 24] = ptr4[2];
				GB[(i * 2) + 25] = ptr4[1];
			}

			memcpy(pos, AR, sizeof(AR));
			pos += sizeof(AR);
			memcpy(pos, GB, sizeof(GB));
			pos += sizeof(GB);
		}
	}

	GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, *//*mipmap ? GX_TRUE :*//* GX_FALSE);

	DCFlushRange(destination->data, scaled_width * scaled_height * sizeof(unsigned));
}*/

int GX_RGBA_To_RGB5A3(u32 srccolor)
{
	u16 color;

	u32 r, g, b, a;
	r = srccolor & 0xFF;
	srccolor >>= 8;
	g = srccolor & 0xFF;
	srccolor >>= 8;
	b = srccolor & 0xFF;
	srccolor >>= 8;
	a = srccolor & 0xFF;

	if (a > 0xe0)
	{
		r = r >> 3;
		g = g >> 3;
		b = b >> 3;

		color = (r << 10) | (g << 5) | b;
		color |= 0x8000;
	}
	else
	{
		r = r >> 4;
		g = g >> 4;
		b = b >> 4;
		a = a >> 5;

		color = (a << 12) | (r << 8) | (g << 4) | b;
	}

	return color;
}

int GX_LinearToTiled(int x, int y, int width)
{
	int x0, x1, y0, y1;
	int offset;

	x0 = x & 3;
	x1 = x >> 2;
	y0 = y & 3;
	y1 = y >> 2;
	offset = x0 + 4 * y0 + 16 * x1 + 4 * width * y1;

	return offset;
}


/*
===============
GL_CopyRGB5A3

Converts from linear to tiled during copy
===============
*/
void GX_CopyRGB5A3(u16 *dest, u16 *src, int x1, int y1, int x2, int y2, int src_width)
{
	int i, j;

	for (i = y1; i < y2; i++)
		for (j = x1; j < x2; j++)
			dest[GX_LinearToTiled(j, i, src_width)] = src[j + i * src_width];
}

/*
===============
GL_CopyRGB5A3

Converts from linear RGBA8 to tiled RGB5A3 during copy
===============
*/
void GX_CopyRGBA8_To_RGB5A3(u16 *dest, u32 *src, int x1, int y1, int x2, int y2, int src_width)
{
	int i, j;

	for (i = y1; i < y2; i++)
		for (j = x1; j < x2; j++)
			dest[GX_LinearToTiled(j, i, src_width)] = GX_RGBA_To_RGB5A3(src[j + i * src_width]);
}

/*
===============
GL_UploadR32
===============
*/
void GL_Upload32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			s;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Upload32: too big");

	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * 4);
	}

	destination->data = __lwp_heap_allocate(&texture_heap, scaled_width * scaled_height * TEXTURE_SIZE);
	if (!destination->data)
		Sys_Error("GL_Upload32: Out of memory.");

	destination->scaled_width = scaled_width;
	destination->scaled_height = scaled_height;

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Upload32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Upload32: destination->data&31");

	GX_CopyRGBA8_To_RGB5A3((u16 *)destination->data, scaled, 0, 0, scaled_width, scaled_height, scaled_width);

	GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, TEXTURE_FORMAT, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

	DCFlushRange(destination->data, scaled_width * scaled_height * TEXTURE_SIZE);
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;

	s = width*height;

	if (s&3)
		Sys_Error ("GL_Upload8: s&3");

	for (i = 0; i < s; i += 4)
	{
		trans[i] = d_8to24table[data[i]];
		trans[i + 1] = d_8to24table[data[i + 1]];
		trans[i + 2] = d_8to24table[data[i + 2]];
		trans[i + 3] = d_8to24table[data[i + 3]];
	}

	GL_Upload32 (destination, trans, width, height, mipmap, alpha);
}

/*
===============
GL_UploadLightmapRGB5A3

Assumes scale is alright
===============
*/
void GL_UploadLightmapRGB5A3 (gltexture_t *destination, u16 *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			s;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/TEXTURE_SIZE)
		Sys_Error ("GL_UploadLightmapRGB5A3: too big");

	if (scaled_width != width || scaled_height != height)
	{
		Sys_Error ("GL_UploadLightmapRGB5A3: scaled_width != width || scaled_height != height");
	}

	destination->data = __lwp_heap_allocate(&texture_heap, scaled_width * scaled_height * TEXTURE_SIZE);
	if (!destination->data)
		Sys_Error("GL_UploadLightmapRGB5A3: Out of memory.");

	destination->scaled_width = scaled_width;
	destination->scaled_height = scaled_height;

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_UploadLightmapRGB5A3: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_UploadLightmapRGB5A3: destination->data&31");

	GX_CopyRGB5A3((u16 *)destination->data, data, 0, 0, scaled_width, scaled_height, scaled_width);

	GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, TEXTURE_FORMAT, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

	DCFlushRange(destination->data, scaled_width * scaled_height * TEXTURE_SIZE);
}

/*
===============
GL_UploadLightmap32
===============
*/
void GL_UploadLightmap32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;

	s = width*height;

	if (s&3)
		Sys_Error ("GL_UploadLightmap32: s&3");

	for (i = 0; i < s; i += 4)
	{
		((u16 *)trans)[i] = GX_RGBA_To_RGB5A3(data[i]);
		((u16 *)trans)[i + 1] = GX_RGBA_To_RGB5A3(data[i + 1]);
		((u16 *)trans)[i + 2] = GX_RGBA_To_RGB5A3(data[i + 2]);
		((u16 *)trans)[i + 3] = GX_RGBA_To_RGB5A3(data[i + 3]);
	}

	GL_UploadLightmapRGB5A3 (destination, (u16 *)trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep)
{
	int			i;
	gltexture_t	*glt;

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (glt->used)
			{
				// ELUTODO: causes problems if we compare to a texture with NO name?
				if (!strcmp (identifier, glt->identifier))
				{
					if (width != glt->width || height != glt->height)
					{
						//Con_DPrintf ("GL_LoadTexture: cache mismatch, reloading");
						if (!__lwp_heap_free(&texture_heap, glt->data))
							Sys_Error("GL_LoadTexture: Error freeing data.");
						goto reload; // best way to do it
					}
					return glt->texnum;
				}
			}
		}
	}

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (!glt->used)
			break;
	}

	if (i == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES\n");

reload:
	strcpy (glt->identifier, identifier);
	glt->texnum = i;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->type = 0;
	glt->keep = keep;
	glt->used = true;

	GL_Upload8 (glt, data, width, height, mipmap, alpha);

	if (glt->texnum == numgltextures)
		numgltextures++;

	return glt->texnum;
}

/*
======================
GL_LoadLightmapTexture
======================
*/
int GL_LoadLightmapTexture (char *identifier, int width, int height, byte *data)
{
	gltexture_t	*glt;

	// They need to be allocated sequentially
	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadLightmapTexture: numgltextures == MAX_GLTEXTURES\n");

	glt = &gltextures[numgltextures];
	strcpy (glt->identifier, identifier);
	glt->texnum = numgltextures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = true; // ELUTODO
	glt->type = 1;
	glt->keep = false;
	glt->used = true;

	GL_UploadLightmap32 (glt, (u32 *)data, width, height, true, true);

	if (width != glt->scaled_width || height != glt->scaled_height)
		Sys_Error("GL_LoadLightmapTexture: Tried to scale lightmap\n");

	numgltextures++;

	return glt->texnum;
}

/*
===============
GL_Update32
===============
*/
/*
void GL_Update32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, x, y, s;
	u8			*pos;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Update32: too big");

	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * sizeof(unsigned));
	}

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Update32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Update32: destination->data&31");

	pos = (u8 *)destination->data;
	for (y = 0; y < scaled_height; y += 4)
	{
		u8* row1 = (u8 *)&(scaled[scaled_width * (y + 0)]);
		u8* row2 = (u8 *)&(scaled[scaled_width * (y + 1)]);
		u8* row3 = (u8 *)&(scaled[scaled_width * (y + 2)]);
		u8* row4 = (u8 *)&(scaled[scaled_width * (y + 3)]);

		for (x = 0; x < scaled_width; x += 4)
		{
			u8 AR[32];
			u8 GB[32];

			for (i = 0; i < 4; i++)
			{
				u8* ptr1 = &(row1[(x + i) * 4]);
				u8* ptr2 = &(row2[(x + i) * 4]);
				u8* ptr3 = &(row3[(x + i) * 4]);
				u8* ptr4 = &(row4[(x + i) * 4]);

				AR[(i * 2) +  0] = ptr1[0];
				AR[(i * 2) +  1] = ptr1[3];
				AR[(i * 2) +  8] = ptr2[0];
				AR[(i * 2) +  9] = ptr2[3];
				AR[(i * 2) + 16] = ptr3[0];
				AR[(i * 2) + 17] = ptr3[3];
				AR[(i * 2) + 24] = ptr4[0];
				AR[(i * 2) + 25] = ptr4[3];

				GB[(i * 2) +  0] = ptr1[2];
				GB[(i * 2) +  1] = ptr1[1];
				GB[(i * 2) +  8] = ptr2[2];
				GB[(i * 2) +  9] = ptr2[1];
				GB[(i * 2) + 16] = ptr3[2];
				GB[(i * 2) + 17] = ptr3[1];
				GB[(i * 2) + 24] = ptr4[2];
				GB[(i * 2) + 25] = ptr4[1];
			}

			memcpy(pos, AR, sizeof(AR));
			pos += sizeof(AR);
			memcpy(pos, GB, sizeof(GB));
			pos += sizeof(GB);
		}
	}

	DCFlushRange(destination->data, scaled_width * scaled_height * sizeof(unsigned));
	GX_InvalidateTexAll();
}*/

/*
===============
GL_Update32
===============
*/
void GL_Update32 (gltexture_t *destination, u32 *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			s;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Update32: too big");

	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * 4);
	}

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Update32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Update32: destination->data&31");

	GX_CopyRGBA8_To_RGB5A3((u16 *)destination->data, scaled, 0, 0, scaled_width, scaled_height, scaled_width);

	DCFlushRange(destination->data, scaled_width * scaled_height * TEXTURE_SIZE);
	GX_InvalidateTexAll();
}

/*
===============
GL_Update8
===============
*/
void GL_Update8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;

	s = width*height;

	if (s&3)
		Sys_Error ("GL_Update8: s&3");

	for (i = 0; i < s; i += 4)
	{
		trans[i] = d_8to24table[data[i]];
		trans[i + 1] = d_8to24table[data[i + 1]];
		trans[i + 2] = d_8to24table[data[i + 2]];
		trans[i + 3] = d_8to24table[data[i + 3]];
	}

	GL_Update32 (destination, trans, width, height, mipmap, alpha);
}

/*
================
GL_UpdateTexture
================
*/
void GL_UpdateTexture (int pic_id, char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha)
{
	gltexture_t	*glt;

	// see if the texture is allready present
	glt = &gltextures[pic_id];

	if (strcmp (identifier, glt->identifier) || width != glt->width || height != glt->height || mipmap != glt->mipmap || glt->type != 0 || !glt->used)
			Sys_Error ("GL_UpdateTexture: cache mismatch");

	GL_Update8 (glt, data, width, height, mipmap, alpha);
}

/*
================================
GL_UpdateLightmapTextureRegion32
================================
*/
void GL_UpdateLightmapTextureRegion32 (gltexture_t *destination, unsigned *data, int width, int height, int xoffset, int yoffset, qboolean mipmap, qboolean alpha)
{
	int			x, y;
	int			realwidth = width + xoffset;
	int			realheight = height + yoffset;
	u16			*dest = (u16 *)destination->data;

	// ELUTODO: mipmaps

	if ((int)destination->data & 31)
		Sys_Error ("GL_UpdateLightmapTextureRegion32: destination->data&31");

	for (y = yoffset; y < realheight; y++)
		for (x = xoffset; x < realwidth; x++)
			dest[GX_LinearToTiled(x, y, width)] = GX_RGBA_To_RGB5A3(data[x + y * realwidth]);


	// ELUTODO: flush region only
	DCFlushRange(destination->data, destination->scaled_width * destination->scaled_height * TEXTURE_SIZE);
	GX_InvalidateTexAll();
}

/*
==============================
GL_UpdateLightmapTextureRegion
==============================
*/
// ELUTODO: doesn't work if the texture doesn't follow the default quake format. Needs improvements.
void GL_UpdateLightmapTextureRegion (int pic_id, int width, int height, int xoffset, int yoffset, byte *data)
{
	gltexture_t	*destination;

	// see if the texture is allready present
	destination = &gltextures[pic_id];

	GL_UpdateLightmapTextureRegion32 (destination, (unsigned *)data, width, height, xoffset, yoffset, true, true);
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	// ELUTODO: loading too much with "" fills the memory with repeated data? Hope not... Check later.
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, true);
}

// ELUTODO: clean the disable/enable multitexture calls around the engine

void GL_DisableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
}

void GL_EnableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	GX_SetNumTexGens(2);
	GX_SetNumTevStages(2);
}

void GL_ClearTextureCache(void)
{
	int i;
	int oldnumgltextures = numgltextures;
	void *newdata;

	numgltextures = 0;

	for (i = 0; i < oldnumgltextures; i++)
	{
		if (gltextures[i].used)
		{
			if (gltextures[i].keep)
			{
				numgltextures = i + 1;

				newdata = __lwp_heap_allocate(&texture_heap, gltextures[i].scaled_width * gltextures[i].scaled_height * TEXTURE_SIZE);
				if (!newdata)
					Sys_Error("GL_ClearTextureCache: Out of memory.");

				// ELUTODO Pseudo-defragmentation that helps a bit :)
				memcpy(newdata, gltextures[i].data, gltextures[i].scaled_width * gltextures[i].scaled_height * TEXTURE_SIZE);

				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");

				gltextures[i].data = newdata;
				GX_InitTexObj(&gltextures[i].gx_tex, gltextures[i].data, gltextures[i].scaled_width, gltextures[i].scaled_height, TEXTURE_FORMAT, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

				DCFlushRange(gltextures[i].data, gltextures[i].scaled_width * gltextures[i].scaled_height * TEXTURE_SIZE);
			}
			else
			{
				gltextures[i].used = false;
				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");
			}
		}
	}

	GX_InvalidateTexAll();
}
