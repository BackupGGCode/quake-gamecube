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
// d_scan.c
//
// Portable C scan-level rasterization code, all pixel depths.

extern "C"
{
#include "quakedef.h"
#include "r_local.h"
#include "d_local.h"
}

static unsigned char	*r_turb_pbase, *r_turb_pdest;
static fixed16_t		r_turb_s, r_turb_t, r_turb_sstep, r_turb_tstep;
static int				*r_turb_turb;
static int				r_turb_spancount;

template<int other> static inline int Min(const int value)
{
	return (value < other) ? value : other;
}

template<int minimum> static inline int Clamp(const int maximum, const int value)
{
	if (value < minimum)
	{
		return minimum;
	}
	else if (value > maximum)
	{
		return maximum;
	}
	else
	{
		return value;
	}
}


/*
=============
D_WarpScreen

// this performs a slight compression of the screen at the same time as
// the sine warp, to keep the edges from wrapping
=============
*/
void D_WarpScreen (void)
{
	int		w, h;
	int		u,v;
	byte	*dest;
	int		*turb;
	int		*col;
	byte	**row;
	byte	*rowptr[MAXHEIGHT+(AMP2*2)];
	int		column[MAXWIDTH+(AMP2*2)];
	float	wratio, hratio;

	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;

	wratio = w / (float)scr_vrect.width;
	hratio = h / (float)scr_vrect.height;

	for (v=0 ; v<scr_vrect.height+AMP2*2 ; v++)
	{
		rowptr[v] = d_viewbuffer + (r_refdef.vrect.y * screenwidth) +
				 (screenwidth * (int)((float)v * hratio * h / (h + AMP2 * 2)));
	}

	for (u=0 ; u<scr_vrect.width+AMP2*2 ; u++)
	{
		column[u] = r_refdef.vrect.x +
				(int)((float)u * wratio * w / (w + AMP2 * 2));
	}

	turb = intsintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	dest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;

	for (v=0 ; v<scr_vrect.height ; v++, dest += vid.rowbytes)
	{
		col = &column[turb[v]];
		row = &rowptr[v];

		for (u=0 ; u<scr_vrect.width ; u+=4)
		{
			dest[u+0] = row[turb[u+0]][col[u+0]];
			dest[u+1] = row[turb[u+1]][col[u+1]];
			dest[u+2] = row[turb[u+2]][col[u+2]];
			dest[u+3] = row[turb[u+3]][col[u+3]];
		}
	}
}


/*
=============
D_DrawTurbulent8Span
=============
*/
static void D_DrawTurbulent8Span (void)
{
	int		sturb, tturb;

	do
	{
		sturb = ((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63;
		tturb = ((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63;
		*r_turb_pdest++ = *(r_turb_pbase + (tturb<<6) + sturb);
		r_turb_s += r_turb_sstep;
		r_turb_t += r_turb_tstep;
	} while (--r_turb_spancount > 0);
}


/*
=============
Turbulent8
=============
*/
void Turbulent8 (espan_t *pspan)
{
	int				count;
	fixed16_t		snext, tnext;
	float			sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float			sdivz16stepu, tdivz16stepu, zi16stepu;
	
	r_turb_turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));

	r_turb_sstep = 0;	// keep compiler happy
	r_turb_tstep = 0;	// ditto

	r_turb_pbase = (unsigned char *)cacheblock;

	sdivz16stepu = d_sdivzstepu * 16;
	tdivz16stepu = d_tdivzstepu * 16;
	zi16stepu = d_zistepu * 16;

	do
	{
		r_turb_pdest = (unsigned char *)((byte *)d_viewbuffer +
				(screenwidth * pspan->v) + pspan->u);

		count = pspan->count;

	// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

		r_turb_s = (int)(sdivz * z) + sadjust;
		if (r_turb_s > bbextents)
			r_turb_s = bbextents;
		else if (r_turb_s < 0)
			r_turb_s = 0;

		r_turb_t = (int)(tdivz * z) + tadjust;
		if (r_turb_t > bbextentt)
			r_turb_t = bbextentt;
		else if (r_turb_t < 0)
			r_turb_t = 0;

		do
		{
		// calculate s and t at the far end of the span
			if (count >= 16)
				r_turb_spancount = 16;
			else
				r_turb_spancount = count;

			count -= r_turb_spancount;

			if (count)
			{
			// calculate s/z, t/z, zi->fixed s and t at far end of span,
			// calculate s and t steps across span by shifting
				sdivz += sdivz16stepu;
				tdivz += tdivz16stepu;
				zi += zi16stepu;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 16)
					snext = 16;	// prevent round-off error on <0 steps from
								//  from causing overstepping & running off the
								//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 16)
					tnext = 16;	// guard against round-off error on <0 steps

				r_turb_sstep = (snext - r_turb_s) >> 4;
				r_turb_tstep = (tnext - r_turb_t) >> 4;
			}
			else
			{
			// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
			// can't step off polygon), clamp, calculate s and t steps across
			// span by division, biasing steps low so we don't run off the
			// texture
				spancountminus1 = (float)(r_turb_spancount - 1);
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				if (snext > bbextents)
					snext = bbextents;
				else if (snext < 16)
					snext = 16;	// prevent round-off error on <0 steps from
								//  from causing overstepping & running off the
								//  edge of the texture

				tnext = (int)(tdivz * z) + tadjust;
				if (tnext > bbextentt)
					tnext = bbextentt;
				else if (tnext < 16)
					tnext = 16;	// guard against round-off error on <0 steps

				if (r_turb_spancount > 1)
				{
					r_turb_sstep = (snext - r_turb_s) / (r_turb_spancount - 1);
					r_turb_tstep = (tnext - r_turb_t) / (r_turb_spancount - 1);
				}
			}

			r_turb_s = r_turb_s & ((CYCLE<<16)-1);
			r_turb_t = r_turb_t & ((CYCLE<<16)-1);

			D_DrawTurbulent8Span ();

			r_turb_s = snext;
			r_turb_t = tnext;

		} while (count > 0);

	} while ((pspan = pspan->pnext) != NULL);
}


/*
=============
D_DrawSpans
=============
*/
template<int pixels_to_draw>
static inline void DrawAffineRun(pixel_t*&, const pixel_t* const, fixed16_t&, fixed16_t&, const fixed16_t, const fixed16_t);

template<>
static inline void DrawAffineRun<0>(pixel_t*&, const pixel_t* const, fixed16_t&, fixed16_t&, const fixed16_t, const fixed16_t)
{
}

template<int pixels_to_draw>
static inline void DrawAffineRun(pixel_t*& pdest, const pixel_t* const pbase, fixed16_t& s, fixed16_t& t, const fixed16_t sstep, const fixed16_t tstep)
{
	*pdest++ = *(pbase + (s >> 16) + ((t >> 16) * cachewidth));
	s += sstep;
	t += tstep;

	DrawAffineRun<pixels_to_draw - 1>(pdest, pbase, s, t, sstep, tstep);
}

static inline fixed16_t CalculateCoord(fixed16_t bbextent, fixed16_t divz, fixed16_t z, fixed16_t adjust)
{
	return Clamp<0>(bbextent, (divz >> 4) * (z >> 12) + adjust);
}

template<int affine_run_size>
static void DrawSpans(const espan_t *span)
{
	// Variables that don't change for each span.
	const pixel_t* const	pbase			= cacheblock;
	const fixed16_t			sdivzstepu		= static_cast<fixed16_t>(d_sdivzstepu * 65536);
	const fixed16_t			tdivzstepu		= static_cast<fixed16_t>(d_tdivzstepu * 65536);
	const fixed16_t			sdivz16stepu	= sdivzstepu * affine_run_size;
	const fixed16_t			tdivz16stepu	= tdivzstepu * affine_run_size;
	const float				zi16stepu		= d_zistepu * affine_run_size;
	const int				rowbytes		= vid.rowbytes;

	// While there are spans to draw...
	do
	{
		// Variables which change per span.
		pixel_t*		pdest		= d_viewbuffer + (rowbytes * span->v) + span->u;
		int				pixels_left	= span->count;
		const int		du			= span->u;
		const int		dv			= span->v;
		fixed16_t		sdivz		= static_cast<fixed16_t>((d_sdivzorigin + (dv * d_sdivzstepv) + (du * d_sdivzstepu)) * 65536);
		fixed16_t		tdivz		= static_cast<fixed16_t>((d_tdivzorigin + (dv * d_tdivzstepv) + (du * d_tdivzstepu)) * 65536);
		float			zi			= d_ziorigin + (dv * d_zistepv) + (du * d_zistepu);
		const fixed16_t	z			= static_cast<fixed16_t>(65536 / zi);
		fixed16_t		s			= CalculateCoord(bbextents, sdivz, z, sadjust);
		fixed16_t		t			= CalculateCoord(bbextentt, tdivz, z, tadjust);

		// Draw fixed size runs while we can...
		while (pixels_left >= affine_run_size)
		{
			// Interpolate S/Z, T/Z and 1/Z.
			sdivz += sdivz16stepu;
			tdivz += tdivz16stepu;
			zi += zi16stepu;

			// Calculate the S & T step.
			const fixed16_t	z		= static_cast<fixed16_t>(65536 / zi);
			const fixed16_t	snext	= CalculateCoord(bbextents, sdivz, z, sadjust);
			const fixed16_t	tnext	= CalculateCoord(bbextentt, tdivz, z, tadjust);
			const fixed16_t	sstep	= (snext - s) / affine_run_size;
			const fixed16_t	tstep	= (tnext - t) / affine_run_size;

			// Draw a run of pixels.
			DrawAffineRun<affine_run_size>(pdest, pbase, s, t, sstep, tstep);

			// Update the number of pixels left to draw.
			pixels_left -= affine_run_size;
		}

		// Draw any remaining pixels.
		if (pixels_left)
		{
			// Interpolate S/Z, T/Z and 1/Z.
			const int pixels_left_minus_one = pixels_left - 1;
			sdivz += sdivzstepu * pixels_left_minus_one;
			tdivz += tdivzstepu * pixels_left_minus_one;
			zi += d_zistepu * pixels_left_minus_one;

			// Calculate the S & T step.
			const fixed16_t	z		= static_cast<fixed16_t>(65536 / zi);
			const fixed16_t snext	= CalculateCoord(bbextents, sdivz, z, sadjust);
			const fixed16_t	tnext	= CalculateCoord(bbextentt, tdivz, z, tadjust);
			const fixed16_t	sstep	= (snext - s) / pixels_left;
			const fixed16_t	tstep	= (tnext - t) / pixels_left;

			// Draw the remaining pixels.
			do
			{
				*pdest++ = *(pbase + (s >> 16) + ((t >> 16) * cachewidth));
				s += sstep;
				t += tstep;
				--pixels_left;
			}
			while (pixels_left > 0);
		}

		// Move on to the next span.
		span = span->pnext;
	}
	while (span);
}

void D_DrawSpans(espan_t *pspan)
{
	DrawSpans<16>(pspan);
}


/*
=============
D_DrawZSpans
=============
*/
void D_DrawZSpans (espan_t *pspan)
{
	// FIXME: check for clamping/range problems
	// we count on FP exceptions being turned off to avoid range problems
	const int izistep = static_cast<int>(d_zistepu * 0x8000 * 0x10000);

	// While there are spans to draw...
	do
	{
		const int	du	= pspan->u;
		const int	dv	= pspan->v;
		const float	zi	= d_ziorigin + (dv * d_zistepv) + (du * d_zistepu);

		short*	pdest	= d_pzbuffer + (d_zwidth * dv) + du;
		int		count	= pspan->count;
		int		izi		= static_cast<int>(zi * 0x8000 * 0x10000);

		if (reinterpret_cast<long>(pdest) & 0x02)
		{
			*pdest++ = static_cast<short>(izi >> 16);
			izi += izistep;
			count--;
		}

		int doublecount = count >> 1;
		if (doublecount > 0)
		{
			do
			{
				unsigned int ltemp = izi >> 16;
				izi += izistep;
				ltemp |= izi & 0xFFFF0000;
				izi += izistep;
				*(int *)pdest = ltemp;
				pdest += 2;
			}
			while (--doublecount > 0);
		}

		if (count & 1)
		{
			*pdest = (short)(izi >> 16);
		}

		// Next span.
		pspan = pspan->pnext;
	}
	while (pspan);
}
