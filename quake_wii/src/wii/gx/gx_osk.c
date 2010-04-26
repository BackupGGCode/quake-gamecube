/*
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
#include "../../generic/quakedef.h"

// ELUTODO: problems with higher 2D resolutions and different vid_tvborder values

void OSK_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		Draw_Character (cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
}

void OSK_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		Draw_Character (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	Draw_TransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		Draw_TransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			Draw_TransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		Draw_TransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	Draw_TransPic (cx, cy+8, p);
}

void GX_DrawOSK(void)
{
	int i, j;
	int xstart = OSK_XSTART * ((float)vid.conwidth / glwidth);
	int ystart = OSK_YSTART * ((float)vid.conheight / glheight);

	DrawTextBox(xstart, ystart, osk_num_col * (osk_col_size / osk_charsize), osk_num_lines * (osk_line_size / osk_charsize));

	for (i = 0; i < osk_num_lines; i++)
		for (j = 0; j < osk_num_col; j++)
		{
			int which = osk_set[i * osk_num_col + j];

			if (!which || which == K_ENTER || which == K_SPACE)
				continue;

			if (osk_selected == which)
				Draw_Character (xstart + (j + 1) * osk_col_size, ystart + (i + 1) * osk_line_size, which + 128);
			else
				Draw_Character (xstart + (j + 1) * osk_col_size, ystart + (i + 1) * osk_line_size, which);

		}

	// hardcoded

	if (osk_selected == K_ENTER)
		OSK_Print(xstart + 13 * osk_col_size, ystart + 4 * osk_line_size, "Enter");
	else
		OSK_PrintWhite(xstart + 13 * osk_col_size, ystart + 4 * osk_line_size, "Enter");

	if (osk_selected == K_SPACE)
		OSK_Print(xstart + 5 * osk_col_size, ystart + 5 * osk_line_size, "Spacebar");
	else
		OSK_PrintWhite(xstart + 5 * osk_col_size, ystart + 5 * osk_line_size, "Spacebar");

	Draw_Character ((osk_coords[0] + osk_col_size - 24) * ((float)vid.width / glwidth), (osk_coords[1] + osk_line_size - 24) * ((float)vid.height / glheight), '\\' + 128);
}
