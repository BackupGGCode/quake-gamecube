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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include <ogc/system.h>

#include "../../generic/quakedef.h"

cvar_t		gl_max_size = {"gl_max_size", "1024"};

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

int			translate_texture;
int			char_texture;

int			white_texturenum;

typedef struct
{
	int			texnum;
	float		sl, tl, sh, th;
} glpic_t;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

int		texels;

typedef struct
{
	int			texnum;
	GXTexObj	gx_tex;
	char		identifier[64];
	int			width, height;
	qboolean	mipmap;
	unsigned	*data;
	void		*allocated_area;
	int			scaled_width, scaled_height;

	// ELUTODO: make sure textures loaded without an identifier are loaded only one time, if "keep" is on
	// What about mid-game gamma changes?
	qboolean	type; // 0 = normal, 1 = lightmap
	qboolean	keep;

	int			*texnumpointer;
	int			texnumpointer_cnt; // ELUTODO: bad hack
} gltexture_t;

#define	MAX_GLTEXTURES	2048
gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

// ELUTODO: clean this currenttexture
void GL_Bind0 (int texnum)
{
	if (currenttexture == texnum)
		return;
	currenttexture = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP0);
}

void GL_Bind1 (int texnum)
{
	if (currenttexture == texnum)
		return;
	currenttexture = texnum;
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


//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

void GL_LoadPicTexture (qpic_t *pic, int *dest);

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	GL_LoadPicTexture (p, &gl->texnum);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path);	
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	GL_LoadPicTexture (dat, &gl->texnum);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


void Draw_CharToConback (int num, byte *dest)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int		i;
	qpic_t	*cb;
	qpic_t	*player_pic;
	byte	*dest;
	int		x, y;
	char	ver[40];
	glpic_t	*gl;
	int		start;
	byte	*ncdata;
	byte	white_texture[64] = { // ELUTODO assumes 0xfe is white
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe
								}; // ELUTODO no necessity to be this big?

	numgltextures = 0;

	Cvar_RegisterVariable (&gl_max_size);

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i=0 ; i<256*64 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// now turn them into textures
	GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, true, &char_texture, 1);

	start = Hunk_LowMark();

	cb = (qpic_t *)COM_LoadTempFile ("gfx/conback.lmp");	
	if (!cb)
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	// hack the version number directly into the pic
	sprintf (ver, "(WiiGX %4.2f) Quake %4.2f", (float)WIIGX_VERSION, (float)VERSION);

	dest = cb->data + 320*186 + 320 - 11 - 8*strlen(ver);
	y = strlen(ver);
	for (x=0 ; x<y ; x++)
		Draw_CharToConback (ver[x], dest+(x<<3));

	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;

	gl = (glpic_t *)conback->data;
	GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, false, true, &gl->texnum, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);

	player_pic = Draw_CachePic("gfx/menuplyr.lmp");
	// save a texture slot for translated picture
	GL_LoadTexture("player_translate", player_pic->width, player_pic->height, player_pic->data,
		false, false, true, &translate_texture, 1);

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	GL_LoadTexture("white_texturenum", 8, 8, white_texture, false, false, true, &white_texturenum, 1);
}



/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	GL_Bind0 (char_texture);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(fcol, frow);

	GX_Position3f32(x + 8, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(fcol + size, frow);

	GX_Position3f32(x + 8, y + 8, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(fcol + size, frow + size);

	GX_Position3f32(x, y + 8, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(fcol, frow + size);
	GX_End();
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str)
	{
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t			*gl;

	gl = (glpic_t *)pic->data;

	QGX_Alpha(false);
	QGX_Blend(true);

	GL_Bind0 (gl->texnum);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
	GX_TexCoord2f32(gl->sl, gl->tl);

	GX_Position3f32(x + pic->width, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
	GX_TexCoord2f32(gl->sh, gl->tl);

	GX_Position3f32(x + pic->width, y + pic->height, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
	GX_TexCoord2f32(gl->sh, gl->th);

	GX_Position3f32(x, y + pic->height, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
	GX_TexCoord2f32(gl->sl, gl->th);
	GX_End();

	QGX_Blend(false);
	QGX_Alpha(true);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t			*gl;

	gl = (glpic_t *)pic->data;

	GL_Bind0 (gl->texnum);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(gl->sl, gl->tl);

	GX_Position3f32(x + pic->width, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(gl->sh, gl->tl);

	GX_Position3f32(x + pic->width, y + pic->height, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(gl->sh, gl->th);

	GX_Position3f32(x, y + pic->height, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(gl->sl, gl->th);
	GX_End();
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_Pic (x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, c;
	byte			trans[64*64];
	int				p;

	c = pic->width * pic->height;

	for (v = 0; v < c; v++)
	{
		p = menuplyr_pixels[v];
		if (p == 255)
			trans[v] = p;
		else
			trans[v] = translation[p];
	}

	GL_UpdateTexture (translate_texture, gltextures[translate_texture].identifier, gltextures[translate_texture].width,
		gltextures[translate_texture].height, trans, gltextures[translate_texture].mipmap, false);

	GL_Bind0 (translate_texture);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(x + pic->width, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(x + pic->width, y + pic->height, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(x, y + pic->height, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(0, 1);
	GX_End();
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	int y = (vid.height * 3) >> 2;

	if (lines > y)
		Draw_Pic(0, lines - vid.height, conback);
	else
		Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	GL_Bind0 (*(int *)draw_backtile->data);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(x / 64.0, y / 64.0);

	GX_Position3f32(x + w, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32((x + w) / 64.0, y / 64.0);

	GX_Position3f32(x + w, y + h, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32((x + w) / 64.0, (y + h) / 64.0);

	GX_Position3f32(x, y + h, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(x / 64.0, (y + h) / 64.0);
	GX_End();
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
/* ELUTODO
	glDisable (GL_TEXTURE_2D);
	glColor3f (host_basepal[c*3]/255.0,
		host_basepal[c*3+1]/255.0,
		host_basepal[c*3+2]/255.0);

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
*/
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	// ELUTODO: do not use a texture

	QGX_Alpha(false);
	QGX_Blend(true);

	GL_Bind0 (white_texturenum);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(0, 0, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(vid.width, 0, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(vid.width, vid.height, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(0, vid.height, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(0, 1);
	GX_End();

	QGX_Blend(false);
	QGX_Alpha(true);

	Sbar_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
	if (!draw_disc)
		return;
	// ELUTODO glDrawBuffer  (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	// ELUTODO glDrawBuffer  (GL_BACK);
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	GX_SetViewport(glx,gly,glwidth,glheight, 0.0f, 1.0f);

	guOrtho(perspective,0, vid.conheight,0,vid.conwidth,ZMIN2D,ZMAX2D);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	c_guMtxIdentity(modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);

	// ELUODO: filtering is making some borders
	QGX_ZMode(false);
	QGX_Blend(true);
	GX_SetCullMode(GX_CULL_NONE);
	QGX_Alpha(true);

	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	GL_DisableMultitexture();
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

inline void *Align_To_32_Bytes (void *p)
{
	return (void*)(((int)(p + 31)) & 0xffffffe0);
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, x, y, s;
	u8			*pos;
	int			samples;
	static	unsigned	scaled[1024*512];	// [512*256];
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
		Sys_Error ("GL_LoadTexture: too big");

	// ELUTODO samples = alpha ? GX_TF_RGBA8 : GX_TF_RGBA8;

	texels += scaled_width * scaled_height;

	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * sizeof(unsigned));
	}

	// ELUTODO manage properly
	destination->allocated_area = SYS_GetArena2Lo();
	destination->data = Align_To_32_Bytes(destination->allocated_area);
	if ((u32)destination->data + scaled_width * scaled_height * sizeof(unsigned) >= 0x933e0000)
		Sys_Error("GL_Upload32: Out of memory.\nnumgltextures = %d\narena2lo = %.8x\narena2hi = %.8x",
			numgltextures, (u32)SYS_GetArena2Lo(), (u32)SYS_GetArena2Hi());
	SYS_SetArena2Lo(destination->data + scaled_width * scaled_height);

	// ELUTODO use cache
	destination->data = MEM_K0_TO_K1(destination->data);

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

	GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

	/* ELUTODO powercallback SYS_SetPowerCallback(powercallback cb);*/
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	static	unsigned	trans[640*480];		// FIXME, temporary
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (destination, trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
void GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep, int *dest, int dest_count)
{
	int			i;
	gltexture_t	*glt;

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			// ELUTODO: causes problems if we compare to a texture with NO name?
			if (!strcmp (identifier, glt->identifier))
			{
				if (width != glt->width || height != glt->height)
					Sys_Error ("GL_LoadTexture: cache mismatch");
				*dest = gltextures[i].texnum;
				return;
			}
		}
	}

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES\n");

	glt = &gltextures[numgltextures];
	strcpy (glt->identifier, identifier);
	glt->texnum = numgltextures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->type = 0;
	glt->keep = keep;

	GL_Upload8 (glt, data, width, height, mipmap, alpha);

	numgltextures++;

	glt->texnumpointer_cnt = dest_count;
	glt->texnumpointer = dest;
	if (dest)
	{
		for (i = 0; i < dest_count; i++)
			dest[i] = glt->texnum;
	}
}

/*
==================
GL_UploadLightmap8
==================
*/
void GL_UploadLightmap8 (gltexture_t *destination, byte *data, int width, int height)
{
	static	unsigned	trans[640*480];		// FIXME, temporary
	u32 p;
	int			i, s;

	s = width*height;

	if (s&3)
		Sys_Error ("GL_Upload8: s&3");

	for (i=0 ; i<s ; i++)
	{
		p = 0xff - data[i];
		trans[i] = (p << 16) + (p << 8) + p;
	}

	// ELUTODO: really mipmap?
	GL_Upload32 (destination, trans, width, height, true, true);
}

/*
======================
GL_LoadLightmapTexture
======================
*/
// ELUTODO: only acceps 1 lightmap byte
int GL_LoadLightmapTexture (char *identifier, int width, int height, byte *data)
{
	gltexture_t	*glt;

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES\n");

	glt = &gltextures[numgltextures];
	strcpy (glt->identifier, identifier);
	glt->texnum = numgltextures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = true; // ELUTODO
	glt->type = 1;
	glt->keep = false;
	glt->texnumpointer = NULL;

	GL_UploadLightmap8 (glt, data, width, height);

	numgltextures++;

	return glt->texnum;
}

/*
===============
GL_Update32
===============
*/
void GL_Update32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, x, y, s;
	u8			*pos;
	int			samples;
	static	unsigned	scaled[1024*512];	// [512*256];
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
		Sys_Error ("GL_LoadTexture: too big");

	// ELUTODO samples = alpha ? GX_TF_RGBA8 : GX_TF_RGBA8;

	texels += scaled_width * scaled_height;

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

	GX_InvalidateTexAll(); // ELUTODO: invalidate region
}

/*
===============
GL_Update8
===============
*/
void GL_Update8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	static	unsigned	trans[640*480];		// FIXME, temporary
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Update32 (destination, trans, width, height, mipmap, alpha);
}

/*
================
GL_UpdateTexture
================
*/
int GL_UpdateTexture (int pic_id, char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha)
{
	gltexture_t	*glt;

	// see if the texture is allready present
	glt = &gltextures[pic_id];

	if (strcmp (identifier, glt->identifier) || width != glt->width || height != glt->height || mipmap != glt->mipmap || glt->type != 0)
			Sys_Error ("GL_UpdateTexture: cache mismatch");

	GL_Update8 (glt, data, width, height, mipmap, alpha);

	return glt->texnum;
}

/*
===============
GL_Update8
===============
*/
void GL_UpdateLightmap8 (gltexture_t *destination, byte *data, int width, int height)
{
	static	unsigned	trans[640*480];		// FIXME, temporary
	u32 p;
	int			i, s;

	s = width*height;

	if (s&3)
		Sys_Error ("GL_Upload8: s&3");

	for (i=0 ; i<s ; i++)
	{
		p = 0xff - data[i];
		trans[i] = (p << 16) + (p << 8) + p;
	}

	// ELUTODO: really mipmap?
	GL_Update32 (destination, trans, width, height, true, true);
}

/*
========================
GL_UpdateLightmapTexture
========================
*/
int GL_UpdateLightmapTexture (int pic_id, char *identifier, int width, int height, byte *data)
{
	gltexture_t	*glt;

	// see if the texture is allready present
	glt = &gltextures[pic_id];

	if (strcmp (identifier, glt->identifier) || width != glt->width || height != glt->height || glt->mipmap != true /* ELUODO: really mipmap? */ || glt->type != 1)
			Sys_Error ("GL_UpdateTexture: cache mismatch");

	GL_UpdateLightmap8 (glt, data, width, height);

	return glt->texnum;
}

/*
================
GL_LoadPicTexture
================
*/
void GL_LoadPicTexture (qpic_t *pic, int *dest)
{
	// ELUTODO: loading too much with "" fills the memory with repeated data? Hope not... Check later
	GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, true, dest, 1);
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
	int i, j;
	int oldnumgltextures = numgltextures;

	numgltextures = 0;
	SYS_SetArena2Lo((void *)ARENA2_HI);

	for (i = 0; i < oldnumgltextures; i++)
	{
		if (gltextures[i].keep)
		{
			// We use memmove in the case the data is at the same area
			gltextures[numgltextures].texnum = numgltextures;
			memmove(gltextures[numgltextures].identifier, gltextures[i].identifier, 64);
			gltextures[numgltextures].width = gltextures[i].width;
			gltextures[numgltextures].height = gltextures[i].height;
			gltextures[numgltextures].mipmap = gltextures[i].mipmap;
			gltextures[numgltextures].type = gltextures[i].type;
			gltextures[numgltextures].keep = gltextures[i].keep;
			gltextures[numgltextures].scaled_width = gltextures[i].scaled_width;
			gltextures[numgltextures].scaled_height = gltextures[i].scaled_height;

			// ELUTODO manage properly
			gltextures[numgltextures].allocated_area = SYS_GetArena2Lo();
			gltextures[numgltextures].data = Align_To_32_Bytes(gltextures[numgltextures].allocated_area);
			if ((u32)gltextures[numgltextures].data + gltextures[numgltextures].scaled_width * gltextures[numgltextures].scaled_height * sizeof(unsigned) >= 0x933e0000)
				Sys_Error("GL_Upload32: Out of memory.\nnumgltextures = %d\narena2lo = %.8x\narena2hi = %.8x",
					numgltextures, (u32)SYS_GetArena2Lo(), (u32)SYS_GetArena2Hi());
			SYS_SetArena2Lo(gltextures[numgltextures].data + gltextures[numgltextures].scaled_width * gltextures[numgltextures].scaled_height);

			// ELUTODO use cache
			gltextures[numgltextures].data = MEM_K0_TO_K1(gltextures[numgltextures].data);

			memmove(gltextures[numgltextures].data, gltextures[i].data, gltextures[numgltextures].scaled_width * gltextures[numgltextures].scaled_height * 4);

			GX_InitTexObj(&gltextures[numgltextures].gx_tex, gltextures[numgltextures].data, gltextures[numgltextures].scaled_width, gltextures[numgltextures].scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

			gltextures[numgltextures].texnumpointer = gltextures[i].texnumpointer;
			gltextures[numgltextures].texnumpointer_cnt = gltextures[i].texnumpointer_cnt;
			if (gltextures[i].texnumpointer)
			{
				for (j = 0; j < gltextures[numgltextures].texnumpointer_cnt; j++)
					gltextures[numgltextures].texnumpointer[j] = numgltextures;
			}

			numgltextures++;
		}
	}

	GX_InvalidateTexAll();
}
