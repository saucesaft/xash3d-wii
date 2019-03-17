/*
vid_sdl.c - SDL vid component
Copyright (C) 2018 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "gl_local.h"
#include "gl_export.h"

ref_api_t      gEngfuncs;
ref_globals_t *gpGlobals;

static void R_ClearScreen( void )
{
	pglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	pglClear( GL_COLOR_BUFFER_BIT );
}

static qboolean IsNormalPass( void )
{
	return RP_NORMALPASS();
}

static void R_IncrementSpeedsCounter( int type )
{
	switch( type )
	{
	case RS_ACTIVE_TENTS:
		r_stats.c_active_tents_count++;
		break;
	default:
		gEngfuncs.Host_Error( "R_IncrementSpeedsCounter: unsupported type %d\n", type );
	}
}

static const byte *R_GetTextureOriginalBuffer( unsigned int idx )
{
	gl_texture_t *glt = R_GetTexture( idx );

	if( !glt || !glt->original || !glt->original->buffer )
		return NULL;

	return glt->original->buffer;
}

static int R_GetBuiltinTexture( enum ref_shared_texture_e type )
{
	switch( type )
	{
	case REF_DEFAULT_TEXTURE: return tr.defaultTexture;
	case REF_GRAY_TEXTURE: return tr.grayTexture;
	case REF_WHITE_TEXTURE: return tr.whiteTexture;
	case REF_SOLIDSKY_TEXTURE: return tr.solidskyTexture;
	case REF_ALPHASKY_TEXTURE: return tr.alphaskyTexture;
	default: gEngfuncs.Host_Error( "R_GetBuiltinTexture: unsupported type %d\n", type );
	}

	return 0;
}

static void R_FreeSharedTexture( enum ref_shared_texture_e type )
{
	int num = 0;

	switch( type )
	{
	case REF_SOLIDSKY_TEXTURE:
		num = tr.solidskyTexture;
		tr.solidskyTexture = 0;
		break;
	case REF_ALPHASKY_TEXTURE:
		num = tr.alphaskyTexture;
		tr.alphaskyTexture = 0;
		break;
	case REF_DEFAULT_TEXTURE:
	case REF_GRAY_TEXTURE:
	case REF_WHITE_TEXTURE:
		gEngfuncs.Host_Error( "R_FreeSharedTexture: invalid type %d\n", type );
	default: gEngfuncs.Host_Error( "R_FreeSharedTexture: unsupported type %d\n", type );
	}

	GL_FreeTexture( num );
}

/*
=============
CL_FillRGBA

=============
*/
static void CL_FillRGBA( float _x, float _y, float _w, float _h, int r, int g, int b, int a )
{
	pglDisable( GL_TEXTURE_2D );
	pglEnable( GL_BLEND );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	pglColor4f( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );

	pglBegin( GL_QUADS );
		pglVertex2f( _x, _y );
		pglVertex2f( _x + _w, _y );
		pglVertex2f( _x + _w, _y + _h );
		pglVertex2f( _x, _y + _h );
	pglEnd ();

	pglColor3f( 1.0f, 1.0f, 1.0f );
	pglEnable( GL_TEXTURE_2D );
	pglDisable( GL_BLEND );
}

/*
=============
pfnFillRGBABlend

=============
*/
static void GAME_EXPORT CL_FillRGBABlend( float _x, float _y, float _w, float _h, int r, int g, int b, int a )
{
	pglDisable( GL_TEXTURE_2D );
	pglEnable( GL_BLEND );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglColor4f( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );

	pglBegin( GL_QUADS );
		pglVertex2f( _x, _y );
		pglVertex2f( _x + _w, _y );
		pglVertex2f( _x + _w, _y + _h );
		pglVertex2f( _x, _y + _h );
	pglEnd ();

	pglColor3f( 1.0f, 1.0f, 1.0f );
	pglEnable( GL_TEXTURE_2D );
	pglDisable( GL_BLEND );
}

static void Mod_LoadModel( modtype_t desiredType, model_t *mod, const byte *buf, qboolean *loaded, int flags )
{
	switch( desiredType )
	{
	case mod_studio:
		// Mod_LoadStudioModel( mod, buf, loaded );
		break;
	case mod_sprite:
		Mod_LoadSpriteModel( mod, buf, loaded, flags );
		break;
	case mod_alias:
		Mod_LoadAliasModel( mod, buf, loaded );
		break;
	case mod_brush:
		// Mod_LoadBrushModel( mod, buf, loaded );
		break;

	default: gEngfuncs.Host_Error( "Mod_LoadModel: unsupported type %d\n", mod->type );
	}
}

qboolean Mod_ProcessRenderData( model_t *mod, qboolean create, const byte *buf )
{
	qboolean loaded = true;

	if( create )
	{


		switch( mod->type )
		{
			case mod_studio:
				// Mod_LoadStudioModel( mod, buf, loaded );
				break;
			case mod_sprite:
				Mod_LoadSpriteModel( mod, buf, &loaded, mod->numtexinfo );
				break;
			case mod_alias:
				Mod_LoadAliasModel( mod, buf, &loaded );
				break;
			case mod_brush:
				// Mod_LoadBrushModel( mod, buf, loaded );
				break;

			default: gEngfuncs.Host_Error( "Mod_LoadModel: unsupported type %d\n", mod->type );
		}
	}

	if( loaded && gEngfuncs.drawFuncs->Mod_ProcessUserData )
		gEngfuncs.drawFuncs->Mod_ProcessUserData( mod, create, buf );

	if( !create )
		Mod_UnloadTextures( mod );

	return loaded;
}

static int GL_RenderGetParm( int parm, int arg )
{
	gl_texture_t *glt;

	switch( parm )
	{
	case PARM_TEX_WIDTH:
		glt = R_GetTexture( arg );
		return glt->width;
	case PARM_TEX_HEIGHT:
		glt = R_GetTexture( arg );
		return glt->height;
	case PARM_TEX_SRC_WIDTH:
		glt = R_GetTexture( arg );
		return glt->srcWidth;
	case PARM_TEX_SRC_HEIGHT:
		glt = R_GetTexture( arg );
		return glt->srcHeight;
	case PARM_TEX_GLFORMAT:
		glt = R_GetTexture( arg );
		return glt->format;
	case PARM_TEX_ENCODE:
		glt = R_GetTexture( arg );
		return glt->encode;
	case PARM_TEX_MIPCOUNT:
		glt = R_GetTexture( arg );
		return glt->numMips;
	case PARM_TEX_DEPTH:
		glt = R_GetTexture( arg );
		return glt->depth;
	case PARM_TEX_SKYBOX:
		Assert( arg >= 0 && arg < 6 );
		return tr.skyboxTextures[arg];
	case PARM_TEX_SKYTEXNUM:
		return tr.skytexturenum;
	case PARM_TEX_LIGHTMAP:
		arg = bound( 0, arg, MAX_LIGHTMAPS - 1 );
		return tr.lightmapTextures[arg];
	case PARM_WIDESCREEN:
		return gpGlobals->wideScreen;
	case PARM_FULLSCREEN:
		return gpGlobals->fullScreen;
	case PARM_SCREEN_WIDTH:
		return gpGlobals->width;
	case PARM_SCREEN_HEIGHT:
		return gpGlobals->height;
	case PARM_TEX_TARGET:
		glt = R_GetTexture( arg );
		return glt->target;
	case PARM_TEX_TEXNUM:
		glt = R_GetTexture( arg );
		return glt->texnum;
	case PARM_TEX_FLAGS:
		glt = R_GetTexture( arg );
		return glt->flags;
	case PARM_ACTIVE_TMU:
		return glState.activeTMU;
	case PARM_LIGHTSTYLEVALUE:
		arg = bound( 0, arg, MAX_LIGHTSTYLES - 1 );
		return tr.lightstylevalue[arg];
	case PARM_MAX_IMAGE_UNITS:
		return GL_MaxTextureUnits();
	case PARM_REBUILD_GAMMA:
		return glConfig.softwareGammaUpdate;
	case PARM_SURF_SAMPLESIZE:
		if( arg >= 0 && arg < WORLDMODEL->numsurfaces )
			return gEngfuncs.Mod_SampleSizeForFace( &WORLDMODEL->surfaces[arg] );
		return LM_SAMPLE_SIZE;
	case PARM_GL_CONTEXT_TYPE:
		return glConfig.context;
	case PARM_GLES_WRAPPER:
		return glConfig.wrapper;
	case PARM_STENCIL_ACTIVE:
		return glState.stencilEnabled;
	case PARM_SKY_SPHERE:
		return gEngfuncs.CL_GetRenderParm( parm, arg ) && !tr.fCustomSkybox;
	default:
		return gEngfuncs.CL_GetRenderParm( parm, arg );
	}
	return 0;
}

static void R_GetDetailScaleForTexture( int texture, float *xScale, float *yScale )
{
	gl_texture_t *glt = R_GetTexture( texture );

	if( xScale ) *xScale = glt->xscale;
	if( yScale ) *yScale = glt->yscale;
}

static void R_GetExtraParmsForTexture( int texture, byte *red, byte *green, byte *blue, byte *density )
{
	gl_texture_t *glt = R_GetTexture( texture );

	if( red ) *red = glt->fogParams[0];
	if( green ) *green = glt->fogParams[1];
	if( blue ) *blue = glt->fogParams[2];
	if( density ) *density = glt->fogParams[3];
}


static void R_SetCurrentEntity( cl_entity_t *ent )
{
	RI.currententity = ent;

	// set model also
	if( RI.currententity != NULL )
	{
		RI.currentmodel = RI.currententity->model;
	}
}

static void R_SetCurrentModel( model_t *mod )
{
	RI.currentmodel = mod;
}

static float R_GetFrameTime( void )
{
	return tr.frametime;
}

static const char *GL_TextureName( unsigned int texnum )
{
	return R_GetTexture( texnum )->name;
}

const byte *GL_TextureData( unsigned int texnum )
{
	rgbdata_t *pic = R_GetTexture( texnum )->original;

	if( pic != NULL )
		return pic->buffer;
	return NULL;
}

void Mod_BrushUnloadTextures( model_t *mod )
{
	int i;

	for( i = 0; i < mod->numtextures; i++ )
	{
		texture_t *tx = mod->textures[i];
		if( !tx || tx->gl_texturenum == tr.defaultTexture )
			continue;	// free slot

		GL_FreeTexture( tx->gl_texturenum );	// main texture
		GL_FreeTexture( tx->fb_texturenum );	// luma texture
	}
}

void Mod_UnloadTextures( model_t *mod )
{
	int		i, j;

	Assert( mod != NULL );

	switch( mod->type )
	{
	case mod_studio:
		Mod_StudioUnloadTextures( mod->cache.data );
		break;
	case mod_alias:
		Mod_AliasUnloadTextures( mod->cache.data );
		break;
	case mod_brush:
		Mod_BrushUnloadTextures( mod );
		break;
	case mod_sprite:
		Mod_SpriteUnloadTextures( mod->cache.data );
		break;
	default: gEngfuncs.Host_Error( "Mod_UnloadModel: unsupported type %d\n", mod->type );
	}
}

void R_ProcessEntData( qboolean allocate )
{
	if( !allocate )
	{
		tr.draw_list->num_solid_entities = 0;
		tr.draw_list->num_trans_entities = 0;
		tr.draw_list->num_beam_entities = 0;
	}

	if( gEngfuncs.drawFuncs->R_ProcessEntData )
		gEngfuncs.drawFuncs->R_ProcessEntData( allocate );
}

ref_interface_t gReffuncs =
{
	R_Init,
	R_Shutdown,

	GL_SetupAttributes,
	GL_OnContextCreated,
	GL_InitExtensions,
	GL_ClearExtensions,

	R_BeginFrame,
	R_RenderScene,
	R_EndFrame,
	R_PushScene,
	R_PopScene,
	GL_BackendStartFrame,
	GL_BackendEndFrame,

	R_ClearScreen,
	R_AllowFog,
	GL_SetRenderMode,

	R_AddEntity,
	CL_AddCustomBeam,
	R_ProcessEntData,

	IsNormalPass,

	R_ShowTextures,
	R_ShowTree,
	R_IncrementSpeedsCounter,

	R_GetTextureOriginalBuffer,
	GL_LoadTextureFromBuffer,
	R_GetBuiltinTexture,
	R_FreeSharedTexture,
	GL_ProcessTexture,
	R_SetupSky,

	R_Set2DMode,
	R_DrawStretchRaw,
	R_DrawStretchPic,
	R_DrawTileClear,
	CL_FillRGBA,
	CL_FillRGBABlend,

	VID_ScreenShot,
	VID_CubemapShot,

	R_LightPoint,

	R_DecalShoot,
	R_DecalRemoveAll,
	R_CreateDecalList,
	R_ClearAllDecals,

	R_StudioEstimateFrame,
	R_StudioLerpMovement,
	CL_InitStudioAPI,

	R_InitSkyClouds,
	GL_SubdivideSurface,
	CL_RunLightStyles,

	R_GetSpriteParms,
	R_GetSpriteTexture,

	Mod_LoadMapSprite,
	Mod_ProcessRenderData,
	Mod_StudioLoadTextures,

	CL_DrawParticles,
	CL_DrawTracers,
	CL_DrawBeams,
	R_BeamCull,

	GL_RenderGetParm,
	R_GetDetailScaleForTexture,
	R_GetExtraParmsForTexture,
	R_GetFrameTime,

	R_SetCurrentEntity,
	R_SetCurrentModel,

	GL_FindTexture,
	GL_TextureName,
	GL_TextureData,
	GL_LoadTexture,
	GL_CreateTexture,
	GL_LoadTextureArray,
	GL_CreateTextureArray,
	GL_FreeTexture,

	DrawSingleDecal,
	R_DecalSetupVerts,
	R_EntityRemoveDecals,

	R_UploadStretchRaw,

	GL_Bind,
	GL_SelectTexture,
	GL_LoadTexMatrixExt,
	GL_LoadIdentityTexMatrix,
	GL_CleanUpTextureUnits,
	GL_TexGen,
	GL_TextureTarget,
	GL_SetTexCoordArrayMode,
	GL_UpdateTexSize,
	NULL,
	NULL,

	CL_DrawParticlesExternal,
	R_LightVec,
	R_StudioGetTexture,

	R_RenderFrame,
	GL_BuildLightmaps,
	Mod_SetOrthoBounds,
	R_SpeedsMessage,
	Mod_GetCurrentVis,
	R_NewMap,
	R_ClearScene,

	TriRenderMode,
	TriBegin,
	TriEnd,
	_TriColor4f,
	TriColor4ub,
	TriTexCoord2f,
	TriVertex3fv,
	TriVertex3f,
	TriWorldToScreen,
	TriFog,
	R_ScreenToWorld,
	TriGetMatrix,
	TriFogParams,
	TriCullFace,

	VGUI_DrawInit,
	VGUI_DrawShutdown,
	VGUI_SetupDrawingText,
	VGUI_SetupDrawingRect,
	VGUI_SetupDrawingImage,
	VGUI_BindTexture,
	VGUI_EnableTexture,
	VGUI_CreateTexture,
	VGUI_UploadTexture,
	VGUI_UploadTextureBlock,
	VGUI_DrawQuad,
	VGUI_GetTextureSizes,
	VGUI_GenerateTexture,
};

int GAME_EXPORT GetRefAPI( int version, ref_interface_t *funcs, ref_api_t *engfuncs, ref_globals_t *globals )
{
	if( version != REF_API_VERSION )
		return 0;

	// fill in our callbacks
	memcpy( funcs, &gReffuncs, sizeof( ref_interface_t ));
	memcpy( &gEngfuncs, engfuncs, sizeof( ref_api_t ));
	gpGlobals = globals;

	return REF_API_VERSION;
}
