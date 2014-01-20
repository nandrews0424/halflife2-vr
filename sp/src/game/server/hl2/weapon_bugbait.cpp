//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "engine/IEngineSound.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "antlion_maker.h"
#include "grenade_bugbait.h"
#include "gamestats.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Bug Bait Weapon
//

#define NUM_ARC_POINTS 25
#define ARC_TIME_UNIT  .035
#define ARC_SPRITE_SCALE .125 
#define ARC_SPRITE "HUD/ThrowArc.vmt"
#define ARC_SPRITE_IMPACT "HUD/ThrowImpact.vmt"

class CWeaponBugBait : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponBugBait, CBaseHLCombatWeapon );
public:

	DECLARE_SERVERCLASS();

	CWeaponBugBait( void );

	void	Spawn( void );
	void	FallInit( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	void	Drop( const Vector &vecVelocity );
	void	BugbaitStickyTouch( CBaseEntity *pOther );
	void	OnPickedUp( CBaseCombatCharacter *pNewOwner );
	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	
	void	ItemPostFrame( void );
	void	Precache( void );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	ThrowGrenade( CBasePlayer *pPlayer );
	
	bool	HasAnyAmmo( void ) { return true; }
	
	bool	Reload( void );

	void	SetSporeEmitterState( bool state = true );

	bool	ShouldDisplayHUDHint() { return true; }

	DECLARE_DATADESC();

protected:

	bool		m_bDrawBackFinished;
	bool		m_bRedraw;
	bool		m_bEmitSpores;
	EHANDLE		m_hSporeTrail;

	float		m_pulseStart;
	CHandle<CSprite>	m_hArcPoints[NUM_ARC_POINTS];
	
	void	DrawArc( );
	void	HideArc( );
};

IMPLEMENT_SERVERCLASS_ST(CWeaponBugBait, DT_WeaponBugBait)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_bugbait, CWeaponBugBait );
#ifndef HL2MP
PRECACHE_WEAPON_REGISTER( weapon_bugbait );
#endif

BEGIN_DATADESC( CWeaponBugBait )

	DEFINE_FIELD( m_hSporeTrail,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_bRedraw,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bEmitSpores,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bDrawBackFinished,	FIELD_BOOLEAN ),
	DEFINE_AUTO_ARRAY( m_hArcPoints, FIELD_EHANDLE ),

	DEFINE_FUNCTION( BugbaitStickyTouch ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponBugBait::CWeaponBugBait( void )
{
	m_bDrawBackFinished	= false;
	m_bRedraw			= false;
	m_hSporeTrail		= NULL;
	m_pulseStart		= 0;

	for ( int i=0; i < NUM_ARC_POINTS-1; i++ )
	{
		
		m_hArcPoints[i] = CSprite::SpriteCreate(ARC_SPRITE, GetAbsOrigin(), false );
		m_hArcPoints[i]->SetTransparency( kRenderWorldGlow, 255, 255, 255, 64, kRenderFxNoDissipation );
		m_hArcPoints[i]->SetBrightness(80);
		m_hArcPoints[i]->SetScale( ARC_SPRITE_SCALE);
		m_hArcPoints[i]->TurnOff();
	}

	// last one is a different sprite
	int idxImpact = NUM_ARC_POINTS-1;
	m_hArcPoints[idxImpact] = CSprite::SpriteCreate(ARC_SPRITE_IMPACT, GetAbsOrigin(), false );
	m_hArcPoints[idxImpact]->SetTransparency( kRenderWorldGlow, 255, 255, 255, 64, kRenderFxNoDissipation );
	m_hArcPoints[idxImpact]->SetScale(ARC_SPRITE_SCALE*2.5);
	m_hArcPoints[idxImpact]->SetBrightness( 100 );
	m_hArcPoints[idxImpact]->TurnOff();


}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBugBait::Spawn( void )
{
	BaseClass::Spawn();

	// Increase the bugbait's pickup volume. It spawns inside the antlion guard's body,
	// and playtesters seem to be wary about moving into the body.
	SetSize( Vector( -4, -4, -4), Vector(4, 4, 4) );
	CollisionProp()->UseTriggerBounds( true, 100 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBugBait::FallInit( void )
{
	// Bugbait shouldn't be physics, because it musn't roll/move away from it's spawnpoint.
	// The game will break if the player can't pick it up, so it must stay still.
	SetModel( GetWorldModel() );

	VPhysicsDestroyObject();
	SetMoveType( MOVETYPE_FLYGRAVITY );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );

	SetPickupTouch();
	
	SetThink( &CBaseCombatWeapon::FallThink );

	SetNextThink( gpGlobals->curtime + 0.1f );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBugBait::Precache( void )
{
	BaseClass::Precache();
	PrecacheModel( ARC_SPRITE );
	PrecacheModel( ARC_SPRITE_IMPACT );

	UTIL_PrecacheOther( "npc_grenade_bugbait" );

	PrecacheScriptSound( "Weapon_Bugbait.Splat" );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBugBait::Drop( const Vector &vecVelocity )
{
	BaseClass::Drop( vecVelocity );

	// On touch, stick & stop moving. Increase our thinktime a bit so we don't stomp the touch for a bit
	SetNextThink( gpGlobals->curtime + 3.0 );
	SetTouch( &CWeaponBugBait::BugbaitStickyTouch );

	m_hSporeTrail = SporeExplosion::CreateSporeExplosion();
	if ( m_hSporeTrail )
	{
		SporeExplosion *pSporeExplosion = (SporeExplosion *)m_hSporeTrail.Get();

		QAngle	angles;
		VectorAngles( Vector(0,0,1), angles );

		pSporeExplosion->SetAbsAngles( angles );
		pSporeExplosion->SetAbsOrigin( GetAbsOrigin() );
		pSporeExplosion->SetParent( this );

		pSporeExplosion->m_flSpawnRate			= 16.0f;
		pSporeExplosion->m_flParticleLifetime	= 0.5f;
		pSporeExplosion->SetRenderColor( 0.0f, 0.5f, 0.25f, 0.15f );

		pSporeExplosion->m_flStartSize			= 32;
		pSporeExplosion->m_flEndSize			= 48;
		pSporeExplosion->m_flSpawnRadius		= 4;

		pSporeExplosion->SetLifetime( 9999 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stick to the world when we touch it
//-----------------------------------------------------------------------------
void CWeaponBugBait::BugbaitStickyTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsWorld() )
		return;

	// Stop moving, wait for pickup
	SetMoveType( MOVETYPE_NONE );
	SetThink( NULL );
	SetPickupTouch();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPicker - 
//-----------------------------------------------------------------------------
void CWeaponBugBait::OnPickedUp( CBaseCombatCharacter *pNewOwner )
{
	BaseClass::OnPickedUp( pNewOwner );

	if ( m_hSporeTrail )
	{
		UTIL_Remove( m_hSporeTrail );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBugBait::PrimaryAttack( void )
{
	if ( m_bRedraw )
		return;

	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	SendWeaponAnim( ACT_VM_HAULBACK );
	
	m_flTimeWeaponIdle		= FLT_MAX;
	m_flNextPrimaryAttack	= FLT_MAX;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBugBait::SecondaryAttack( void )
{
	// Squeeze!
	CPASAttenuationFilter filter( this );

	EmitSound( filter, entindex(), "Weapon_Bugbait.Splat" );

	if ( CGrenadeBugBait::ActivateBugbaitTargets( GetOwner(), GetAbsOrigin(), true ) == false )
	{
		g_AntlionMakerManager.BroadcastFollowGoal( GetOwner() );
	}

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
		m_iSecondaryAttacks++;
		gamestats->Event_WeaponFired( pOwner, false, GetClassname() );
	}
}

// draw sprites along the arc the thrown object will take

void CWeaponBugBait::DrawArc(  )
{
	int idxImpact = NUM_ARC_POINTS-1;
	float curtime = gpGlobals->curtime;
	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	Vector throwVelocity, shootDirection;
	Vector origin = pPlayer->EyePosition() + pPlayer->EyeToWeaponOffset();
	pPlayer->EyeVectors(&shootDirection);

	pPlayer->GetVelocity( &throwVelocity, NULL );
	throwVelocity +=  shootDirection * 1000;
	throwVelocity *= ARC_TIME_UNIT;  

	Vector vecGravity = Vector(0,0,-GetCurrentGravity() * ARC_TIME_UNIT * ARC_TIME_UNIT);
	
	Vector last = origin;
	bool impacted = false;

	// Reset the indicator pulse time
	float pulseSpeedScale = 1;
	if ( curtime > m_pulseStart + NUM_ARC_POINTS*ARC_TIME_UNIT*pulseSpeedScale*2.5)
		m_pulseStart = curtime;
	
	for ( int i = 0; i < NUM_ARC_POINTS - 1; i ++ )
	{
		if ( impacted )
		{
		 	m_hArcPoints[i]->TurnOff();
			continue;
		}

		// Position at a specific point in time:
		// p(n) = orig + n*vel + ((n^2+n)*accel) / 2
		int t = i+1;
		Vector position = origin + throwVelocity*t + ((t*t+t)*vecGravity) / 2;  
		
		
		m_hArcPoints[i]->TurnOn();
		m_hArcPoints[i]->SetAbsOrigin(position);
		float pct = i/(float) NUM_ARC_POINTS;
		m_hArcPoints[i]->SetBrightness( 80 - 80*pct*pct );
		
		// show a segment brighter if it fits within the pulse window
		float elapsed = curtime-m_pulseStart; 
		int pulseSegment = floor(elapsed / (ARC_TIME_UNIT*pulseSpeedScale));
		
		if ( i == pulseSegment )
			m_hArcPoints[i]->SetScale(ARC_SPRITE_SCALE*2, .2);
		else 
			m_hArcPoints[i]->SetScale(ARC_SPRITE_SCALE, .2);
		
		trace_t tr;
		UTIL_TraceLine(last, position, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		if ( tr.fraction < 1 ) // segment impacted, place impact sprite
		{
			impacted = true;

			m_hArcPoints[i]->TurnOff();
			m_hArcPoints[idxImpact]->SetAbsOrigin(tr.endpos);
			m_hArcPoints[idxImpact]->TurnOn();
		}

		last = position;
	}

	if ( !impacted )
	{
		m_hArcPoints[idxImpact]->TurnOff();
	}
}

void CWeaponBugBait::HideArc( )
{
	for ( int i = 0; i < NUM_ARC_POINTS; i ++ )
	{
		if ( m_hArcPoints[i] != NULL )
		{
			m_hArcPoints[i]->TurnOff();
			
		}
	}
}




void CWeaponBugBait::ThrowGrenade( CBasePlayer *pPlayer )
{
	Vector	vForward, vRight, vUp, vThrowPos, vThrowVel;
	
	pPlayer->EyeVectors( &vForward, &vRight, &vUp );

	vThrowPos = pPlayer->EyePosition() + pPlayer->EyeToWeaponOffset();

	vThrowPos += vForward*3.0f + vRight*3.0f; // adjust slightly for release animation	

	pPlayer->GetVelocity( &vThrowVel, NULL );
	vThrowVel += vForward * 1000;

	CGrenadeBugBait *pGrenade = BugBaitGrenade_Create( vThrowPos, vec3_angle, vThrowVel, QAngle(600,random->RandomInt(-1200,1200),0), pPlayer );

	if ( pGrenade != NULL )
	{
		// If the shot is clear to the player, give the missile a grace period
		trace_t	tr;
		UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + ( vForward * 128 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		
		if ( tr.fraction == 1.0 )
		{
			pGrenade->SetGracePeriod( 0.1f );
		}
	}

	m_bRedraw = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponBugBait::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	switch( pEvent->event )
	{
		case EVENT_WEAPON_SEQUENCE_FINISHED:
			m_bDrawBackFinished = true;
			break;

		case EVENT_WEAPON_THROW:
			ThrowGrenade( pOwner );
			break;

		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponBugBait::Reload( void )
{
	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponBugBait::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	// See if we're cocked and ready to throw
	if ( m_bDrawBackFinished )
	{
		DrawArc(); 

		if ( ( pOwner->m_nButtons & IN_ATTACK ) == false )
		{
			SendWeaponAnim( ACT_VM_THROW );
			m_flNextPrimaryAttack  = gpGlobals->curtime + SequenceDuration();
			HideArc();
			m_bDrawBackFinished = false;
		}
	}
	else
	{
		//See if we're attacking
		if ( ( pOwner->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack < gpGlobals->curtime ) )
		{
			PrimaryAttack();
		}
		else if ( ( pOwner->m_nButtons & IN_ATTACK2 ) && ( m_flNextSecondaryAttack < gpGlobals->curtime ) )
		{
			SecondaryAttack();
		}
	}

	if ( m_bRedraw )
	{
		if ( IsViewModelSequenceFinished() )
		{
			Reload();
		}
	}

	WeaponIdle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponBugBait::Deploy( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return false;

	/*
	if ( m_hSporeTrail == NULL )
	{
		m_hSporeTrail = SporeTrail::CreateSporeTrail();
		
		m_hSporeTrail->m_bEmit				= true;
		m_hSporeTrail->m_flSpawnRate		= 100.0f;
		m_hSporeTrail->m_flParticleLifetime	= 2.0f;
		m_hSporeTrail->m_flStartSize		= 1.0f;
		m_hSporeTrail->m_flEndSize			= 4.0f;
		m_hSporeTrail->m_flSpawnRadius		= 8.0f;

		m_hSporeTrail->m_vecEndColor		= Vector( 0, 0, 0 );

		CBaseViewModel *vm = pOwner->GetViewModel();
		
		if ( vm != NULL )
		{
			m_hSporeTrail->FollowEntity( vm );
		}
	}
	*/

	m_bRedraw = false;
	m_bDrawBackFinished = false;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponBugBait::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	m_bDrawBackFinished = false;
	HideArc();
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : true - 
//-----------------------------------------------------------------------------
void CWeaponBugBait::SetSporeEmitterState( bool state )
{
	m_bEmitSpores = state;
}
