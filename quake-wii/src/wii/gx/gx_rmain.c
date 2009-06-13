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
// r_main.c

#include "../../generic/quakedef.h"

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

int			currenttexture0 = -1;		// to avoid unnecessary texture sets
int			currenttexture1 = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

int			particletexture;	// little dot for particles
int			playertextures[MAX_SCOREBOARD];		// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
cvar_t	r_wateralpha = {"r_wateralpha","0.5"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

extern	cvar_t	gl_ztrick;
float viewport_size[4];

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	// ELUTODO: check for failure cases (rendering to an aspect different of that of the quake-calculated frustum, etc
	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

Vector axis2 = {0,0,1};
Vector axis1 = {0,1,0};
Vector axis0 = {1,0,0};

void R_RotateForEntity (entity_t *e)
{
	Mtx temp;

	// ELUTODO: change back to asm when ALL functions have been corrected
	c_guMtxTrans(temp, e->origin[0],  e->origin[1],  e->origin[2]);
	c_guMtxConcat(model, temp, model);

	c_guMtxRotAxisRad(temp, &axis2, DegToRad(e->angles[1]));
	c_guMtxConcat(model, temp, model);
	c_guMtxRotAxisRad(temp, &axis1, DegToRad(-e->angles[0]));
	c_guMtxConcat(model, temp, model);
	c_guMtxRotAxisRad(temp, &axis0, DegToRad(e->angles[2]));
	c_guMtxConcat(model, temp, model);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	GL_DisableMultitexture();

    GL_Bind0(frame->gl_texturenum);

	QGX_Alpha(true);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(0, 1);

	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(0, 0);

	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(1, 0);

	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(1, 1);

	GX_End();
	QGX_Alpha(false);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../../generic/anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	float 	l;
	trivertx_t	*verts;
	int		*order;
	int		count;

lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			GX_Begin (GX_TRIANGLEFAN, GX_VTXFMT0, count);
		}
		else
			GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, count);

		do
		{
			// normals and vertexes come from the frame list
			GX_Position3f32(verts->v[0], verts->v[1], verts->v[2]);
			l = shadedots[verts->lightnormalindex] * shadelight;
			l *= 255; if (l > 255.0f) l = 255.0f;
			GX_Color4u8(l, l, l, 0xff);

			// texture coordinates come from the draw list
			GX_TexCoord2f32(((float *)order)[0], ((float *)order)[1]);

			order += 2;
			verts++;
		} while (--count);

		GX_End ();
	}
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	float	s, t, l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;
/* ELUTODO
	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}
*/	
}



/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose);
}



/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			i;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	float		an;
	int			anim;
	Mtx			temp;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;


	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//

	ambientlight = shadelight = R_LightPoint (currententity->origin);

	// allways give the gun some light
	if (e == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0) {
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// ZOID: never allow players to go totally black
	i = currententity - cl_entities;
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		if (ambientlight < 8)
			ambientlight = shadelight = 8;

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (!strcmp (clmodel->name, "progs/flame2.mdl")
		|| !strcmp (clmodel->name, "progs/flame.mdl") )
		ambientlight = shadelight = 256;

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;
	
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	GL_DisableMultitexture();

	c_guMtxIdentity(model);
	R_RotateForEntity (e);

	if (!strcmp (clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value) {
		c_guMtxTrans (temp, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		c_guMtxConcat(model, temp, model);
// double size of eyes, since they are really hard to see in gl
		c_guMtxScale (temp, paliashdr->scale[0]*2, paliashdr->scale[1]*2, paliashdr->scale[2]*2);
		c_guMtxConcat(model, temp, model);
	} else {
		c_guMtxTrans (temp, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		c_guMtxConcat(model, temp, model);
		c_guMtxScale (temp, paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
		c_guMtxConcat(model, temp, model);
	}

	c_guMtxConcat(view,model,modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);

	anim = (int)(cl.time*10) & 3;
    GL_Bind0(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i<=cl.maxclients)
		    GL_Bind0(playertextures[i - 1]);
	}

	/* ELUTODO if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);*/

	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	/* ELUTODO
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);*/

	R_SetupAliasFrame (currententity->frame, paliashdr);

	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);

/* ELUTODO
	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);*/

	/* ELUTODO if (r_shadows.value)
	{
		glPushMatrix ();
		R_RotateForEntity (e);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
		glColor4f (1,1,1,1);
		glPopMatrix ();
	}
*/
}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity);
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}

	GX_LoadPosMtxImm(view, GX_PNMTX0);
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;

			default:
				break;
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;

	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun
	ambientlight = j;
	shadelight = j;

// add dynamic lights		
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2], viewport_size[3], 0.0f, 0.3f);
	R_DrawAliasModel (currententity);
	GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2], viewport_size[3], 0.0f, 1.0f);
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	Mtx temp;

	if (!gl_polyblend.value)
		return;
	if (!v_blend[3] && v_gamma.value == 1.0f)
		return;

	QGX_Alpha(false);
	QGX_Blend(true);
	QGX_ZMode(false);
	GL_Bind0(white_texturenum); // ELUTODO: do not use a texture
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	c_guMtxIdentity(view);
	c_guMtxRotAxisRad(temp, &axis0, DegToRad(-90.0f));		// put Z going up
	c_guMtxConcat(view, temp, view);
	c_guMtxRotAxisRad(temp, &axis2, DegToRad(90.0f));		// put Z going up
	c_guMtxConcat(view, temp, view);
	GX_LoadPosMtxImm(view, GX_PNMTX0);

	// ELUTODO: check if v_blend gets bigger than 1.0f
	if (v_blend[3])
	{
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3f32(10.0f, 100.0f, 100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(1.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, 100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(0.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, -100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(0.0f, 0.0f);

		GX_Position3f32(10.0f, 100.0f, -100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(1.0f, 0.0f);

		GX_End();
	}

	// ELUTODO quick hack
	if (v_gamma.value != 1.0f)
	{
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3f32(10.0f, 100.0f, 100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(1.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, 100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(0.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, -100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(0.0f, 0.0f);

		GX_Position3f32(10.0f, 100.0f, -100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(1.0f, 0.0f);

		GX_End();
	}

	QGX_Blend(false);
	QGX_Alpha(true);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
}


int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;
	Mtx		temp;

	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = r_refdef.vrect.y * glheight/vid.height;
	y2 = (r_refdef.vrect.y + r_refdef.vrect.height) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y > 0)
		y--;
	if (y2 < glheight)
		y2++;

	w = x2 - x;
	h = y2 - y;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	viewport_size[0] = glx + x;
	viewport_size[1] = gly + y;
	viewport_size[2] = w;
	viewport_size[3] = h;
	GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2], viewport_size[3], 0.0f, 1.0f);
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	guPerspective (perspective, r_refdef.fov_y, screenaspect, ZMIN3D, ZMAX3D);

	// ELUTODO: perspective is 4x4, these ops are 4x3
	if (mirror)
	{
		if (mirror_plane->normal[2])
		{
			c_guMtxScale (temp, 1, -1, 1);
			c_guMtxConcat(perspective, temp, perspective);
		}
		else
			c_guMtxScale (temp, -1, 1, 1);
			c_guMtxConcat(perspective, temp, perspective);
		GX_SetCullMode(GX_CULL_FRONT);
	}
	else
		GX_SetCullMode(GX_CULL_BACK);

	GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

	c_guMtxIdentity(view);

	c_guMtxRotAxisRad(temp, &axis0, DegToRad(-90.0f));		// put Z going up
	c_guMtxConcat(view, temp, view);
	c_guMtxRotAxisRad(temp, &axis2, DegToRad(90.0f));		// put Z going up
	c_guMtxConcat(view, temp, view);

	c_guMtxRotAxisRad(temp, &axis0, DegToRad(-r_refdef.viewangles[2]));
	c_guMtxConcat(view, temp, view);
	c_guMtxRotAxisRad(temp, &axis1, DegToRad(-r_refdef.viewangles[0]));
	c_guMtxConcat(view, temp, view);
	c_guMtxRotAxisRad(temp, &axis2, DegToRad(-r_refdef.viewangles[1]));
	c_guMtxConcat(view, temp, view);


	c_guMtxTrans(temp, -r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);
	c_guMtxConcat(view, temp, view);

	// ELUTODOglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (!gl_cull.value)
		GX_SetCullMode(GX_CULL_NONE);

	QGX_Blend(false);
	QGX_Alpha(false);
	QGX_ZMode(true);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	GX_LoadPosMtxImm(view, GX_PNMTX0);
	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	// for the entities, we load the matrices separately
	R_DrawEntitiesOnList ();

	GL_DisableMultitexture();

	GX_LoadPosMtxImm(view, GX_PNMTX0);
	R_DrawParticles ();
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	// Not needed, GX clears the efb while copying to the xfb
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
	msurface_t	*s;
	entity_t	*ent;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}
/* ELUTODO
	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	R_RenderScene ();
	R_DrawWaterSurfaces ();

	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	// blend on top
	glEnable (GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	if (mirror_plane->normal[2])
		glScalef (1,-1,1);
	else
		glScalef (-1,1,1);
	glCullFace(GL_FRONT);
	glMatrixMode(GL_MODELVIEW);

	glLoadMatrixf (r_base_world_matrix);

	glColor4f (1,1,1,r_mirroralpha.value);
	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glDisable (GL_BLEND);
	glColor4f (1,1,1,1);
*/
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	double	time1, time2;
	// ELUTODO GLfloat colors[4] = {(GLfloat) 0.0, (GLfloat) 0.0, (GLfloat) 1, (GLfloat) 0.20};

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		// ELUTODO glFinish ();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

/* ELUTODO
	if (gl_finish.value)
		glFinish ();
*/

	R_Clear ();

	// render normal view

/***** Experimental silly looking fog ******
****** Use r_fullbright if you enable ******
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, colors);
	glFogf(GL_FOG_END, 512.0);
	glEnable(GL_FOG);
********************************************/

	R_RenderScene ();
	R_DrawViewModel ();
	GX_LoadPosMtxImm(view, GX_PNMTX0);
	R_DrawWaterSurfaces ();

//  More fog right here :)
//	glDisable(GL_FOG);
//  End of all fog code...

	// render mirror view
	R_Mirror ();

	R_PolyBlend ();

	if (r_speeds.value)
	{
//		glFinish ();
		time2 = Sys_FloatTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
	}
}
