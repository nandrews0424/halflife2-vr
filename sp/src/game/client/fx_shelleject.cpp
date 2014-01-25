//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		tempents->EjectBrass( data.m_vOrigin, data.m_vAngles, pRenderable->GetRenderAngles(), 0, true );
	}
}

DECLARE_CLIENT_EFFECT( "ShellEject", ShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RifleShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		tempents->EjectBrass( data.m_vOrigin, data.m_vAngles, pRenderable->GetRenderAngles(), 1, false );
	}
}

DECLARE_CLIENT_EFFECT( "RifleShellEject", RifleShellEjectCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShotgunShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell
	IClientRenderable *pRenderable = data.GetRenderable();
	if ( pRenderable )
	{
		tempents->EjectBrass( data.m_vOrigin, data.m_vAngles, pRenderable->GetRenderAngles(), 2, false );
	}
}

DECLARE_CLIENT_EFFECT( "ShotgunShellEject", ShotgunShellEjectCallback );



//-----------------------------------------------------------------------------
// Purpose: Magnum shell ejection includes all 6 shells from the shell attach points
//-----------------------------------------------------------------------------
void MagnumShellEjectCallback( const CEffectData &data )
{
	// Use the gun angles to orient the shell^M
	CBaseEntity* entity = data.GetEntity();
	if ( !entity )
		return;

	CBaseEntity* pOwner = entity->GetOwnerEntity();
	if ( !pOwner )
		return;
	
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	
	if  ( pOwner != pLocalPlayer )
		return;
	
	C_BaseViewModel* vm = pLocalPlayer->GetViewModel(0);
	if ( !vm ) 
		return;
	
	Vector attachOrigin;
	QAngle attachAngles;
	
	Msg("TODO: About to eject 6 shells from the magnum");

	char buf[32];
	for ( int i = 1; i <= 6; i++ )
	{
		Q_snprintf( buf, sizeof( buf ), "Bullet%i", i ); 
		int iAttachment = vm->LookupAttachment(buf);
		vm->GetAttachment(iAttachment, attachOrigin, attachAngles);
		tempents->EjectBrass( attachOrigin, attachAngles, vm->GetAbsAngles(), 3, true );
	}
}

DECLARE_CLIENT_EFFECT( "MagnumShellEject", MagnumShellEjectCallback );
