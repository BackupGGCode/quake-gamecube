#include "../../generic/quakedef.h"

// ELUTODO: problems with higher 2D resolutions and different vid_tvborder values

void OSK_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx + ((vid.width - 320)>>1), line, num);
}

void OSK_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		OSK_DrawCharacter (cx, cy, (*str) + 128);
		str++;
		cx += 8;
	}
}

void OSK_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		OSK_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void OSK_DrawTransPic (int x, int y, qpic_t *pic)
{
	Draw_TransAlphaPic (x + ((vid.width - 320)>>1), y, pic, 0.5);
}

void OSK_DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	OSK_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		OSK_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	OSK_DrawTransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		OSK_DrawTransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			OSK_DrawTransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		OSK_DrawTransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	OSK_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		OSK_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	OSK_DrawTransPic (cx, cy+8, p);
}

void GX_DrawOSK(void)
{
	int i, j;
	int xstart = OSK_XSTART * ((float)vid.width / glwidth);
	int ystart = OSK_YSTART * ((float)vid.height / glheight);

	OSK_DrawTextBox(xstart, ystart, osk_num_col * (osk_col_size / osk_charsize), osk_num_lines * (osk_line_size / osk_charsize));

	for (i = 0; i < osk_num_lines; i++)
		for (j = 0; j < osk_num_col; j++)
		{
			int which = osk_set[i * osk_num_col + j];

			if (!which || which == K_ENTER || which == K_SPACE)
				continue;

			if (osk_selected == which)
				OSK_DrawCharacter (xstart + (j + 1) * osk_col_size, ystart + (i + 1) * osk_line_size, which + 128);
			else
				OSK_DrawCharacter (xstart + (j + 1) * osk_col_size, ystart + (i + 1) * osk_line_size, which);

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

	OSK_DrawCharacter ((osk_coords[0] + osk_col_size) * ((float)vid.width / glwidth), (osk_coords[1] + osk_line_size) * ((float)vid.height / glheight), '\\' + 128);
}
