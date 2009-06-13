/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2008 Eluan Miranda

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

// ELUTODO: MANY assumptions about the pictures sizes

#include <ogc/system.h>
#include <ogc/cache.h>

#include "../../generic/quakedef.h"

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

int GL_LoadPicTexture (qpic_t *pic);

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	gl->texnum = GL_LoadPicTexture (p);
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
	gl->texnum = GL_LoadPicTexture (dat);
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

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i=0 ; i<256*64 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// now turn them into textures
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, true);

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
	gl->texnum = GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, false, true);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	// This is done in video_gx.c now too
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);

	player_pic = Draw_CachePic("gfx/menuplyr.lmp");
	// save a texture slot for translated picture
	translate_texture = GL_LoadTexture("player_translate", player_pic->width, player_pic->height, player_pic->data,
		false, false, true);

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");

	white_texturenum = GL_LoadTexture("white_texturenum", 8, 8, white_texture, false, false, true);
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
	if (x < 0 || (unsigned)(x + pic->width) > vid.conwidth || y < 0 ||
		 (unsigned)(y + pic->height) > vid.conheight)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_Pic (x, y, pic);
}

/*
==================
Draw_TransAlphaPic
==================
*/
void Draw_TransAlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	if (x < 0 || (unsigned)(x + pic->width) > vid.conwidth || y < 0 ||
		 (unsigned)(y + pic->height) > vid.conheight)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_AlphaPic (x, y, pic, alpha);
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
	int y = (vid.conheight * 3) >> 2;

	if (lines > y)
		Draw_Pic(0, lines - vid.conheight, conback);
	else
		Draw_AlphaPic (0, lines - vid.conheight, conback, (float)(1.2 * lines)/y);
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
Draw_AlphaTileClear

This repeats a 64*64 alpha blended tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_AlphaTileClear (int x, int y, int w, int h, float alpha)
{
	GL_Bind0 (*(int *)draw_backtile->data);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
	GX_TexCoord2f32(x / 64.0, y / 64.0);

	GX_Position3f32(x + w, y, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
	GX_TexCoord2f32((x + w) / 64.0, y / 64.0);

	GX_Position3f32(x + w, y + h, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
	GX_TexCoord2f32((x + w) / 64.0, (y + h) / 64.0);

	GX_Position3f32(x, y + h, 0.0f);
	GX_Color4u8(0xff, 0xff, 0xff, (u8)(0xff * alpha));
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
	// ELUTODO: do not use a texture
	GL_Bind0 (white_texturenum);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(host_basepal[c*3], host_basepal[c*3+1], host_basepal[c*3+2], 0xff);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(x + w, y, 0.0f);
	GX_Color4u8(host_basepal[c*3], host_basepal[c*3+1], host_basepal[c*3+2], 0xff);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(x + w, y + h, 0.0f);
	GX_Color4u8(host_basepal[c*3], host_basepal[c*3+1], host_basepal[c*3+2], 0xff);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(x, y + h, 0.0f);
	GX_Color4u8(host_basepal[c*3], host_basepal[c*3+1], host_basepal[c*3+2], 0xff);
	GX_TexCoord2f32(0, 1);
	GX_End();
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

	GX_Position3f32(vid.conwidth, 0, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(vid.conwidth, vid.conheight, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(0, vid.conheight, 0.0f);
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
	Draw_Pic (vid.conwidth - 24, 0, draw_disc);
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
	GX_SetViewport(glx, gly, glwidth, glheight, 0.0f, 1.0f);

	guOrtho(perspective,0, vid.conheight, 0, vid.conwidth, ZMIN2D, ZMAX2D);
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
