//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "grenade_frag.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "items.h"
#include "in_buttons.h"
#include "soundent.h"
#include "gamestats.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GRENADE_TIMER	3.0f //Seconds

#define GRENADE_PAUSED_NO			0
#define GRENADE_PAUSED_PRIMARY		1
#define GRENADE_PAUSED_SECONDARY	2

#define GRENADE_RADIUS	4.0f // inches

#define NUM_ARC_POINTS 25
#define ARC_TIME_UNIT  .035
#define ARC_SPRITE_SCALE .125 
#define ARC_SPRITE "HUD/ThrowArc.vmt"
#define ARC_SPRITE_IMPACT "HUD/ThrowImpact.vmt"

static ConVar mt_grenade_throw_scale( "mt_grenade_throw_scale", "1", FCVAR_ARCHIVE, "Scales grenade motion throws (higher makes you throw it further)");

#define MOTION_THROW_SCALE mt_grenade_throw_scale.GetFloat()*5

//-----------------------------------------------------------------------------
// Fragmentation grenades
//-----------------------------------------------------------------------------
class CWeaponFrag: public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeaponFrag, CBaseHLCombatWeapon );
public:
	DECLARE_SERVERCLASS();

public:
	CWeaponFrag();

	void	Precache( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	DecrementAmmo( CBaseCombatCharacter *pOwner );
	void	ItemPostFrame( void );

	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	
	bool	Reload( void );

	bool	ShouldDisplayHUDHint() { return true; }

private:
	void	ThrowGrenade( CBasePlayer *pPlayer );
	void	RollGrenade( CBasePlayer *pPlayer );
	void	LobGrenade( CBasePlayer *pPlayer );
	void	MotionThrowGrenade( CBasePlayer *pPlayer, bool release );

	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
	void	CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc );

	bool	m_bRedraw;	//Draw the weapon again after throwing a grenade
	
	int		m_AttackPaused;
	bool	m_fDrawbackFinished;

	float		m_pulseStart;
	CHandle<CSprite>	m_hArcPoints[NUM_ARC_POINTS];
	
	// readings for motion swings
	Vector	m_lastPosition;
	QAngle	m_lastAngle;
	float	m_lastSample;
	bool	m_bMotionThrow;

	void	DrawArc( bool primary = true );
	void	HideArc( );

	DECLARE_ACTTABLE();

	DECLARE_DATADESC();
};


BEGIN_DATADESC( CWeaponFrag )
	DEFINE_FIELD( m_bRedraw, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_AttackPaused, FIELD_INTEGER ),
	DEFINE_FIELD( m_fDrawbackFinished, FIELD_BOOLEAN ),
END_DATADESC()

acttable_t	CWeaponFrag::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_SLAM, true },
};

IMPLEMENT_ACTTABLE(CWeaponFrag);

IMPLEMENT_SERVERCLASS_ST(CWeaponFrag, DT_WeaponFrag)
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_frag, CWeaponFrag );
PRECACHE_WEAPON_REGISTER(weapon_frag);



CWeaponFrag::CWeaponFrag() : CBaseHLCombatWeapon(), m_bRedraw( false )
{
	m_pulseStart		= 0;
	m_lastSample		= 0;

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
void CWeaponFrag::Precache( void )
{
	BaseClass::Precache();
	
	UTIL_PrecacheOther( "npc_grenade_frag" );
	PrecacheScriptSound( "WeaponFrag.Throw" );
	PrecacheScriptSound( "WeaponFrag.Roll" );
	PrecacheModel( ARC_SPRITE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponFrag::Deploy( void )
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_bRedraw = false;
	m_fDrawbackFinished = false;
	HideArc();
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//			*pOperator - 
//-----------------------------------------------------------------------------
void CWeaponFrag::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	bool fThrewGrenade = false;

	switch( pEvent->event )
	{
		case EVENT_WEAPON_SEQUENCE_FINISHED:
			m_fDrawbackFinished = true;
			break;

		case EVENT_WEAPON_THROW:
			if ( !m_bMotionThrow ) // throw already occurred if it was a motion throw
			{
				ThrowGrenade( pOwner );
				DecrementAmmo( pOwner );
				fThrewGrenade = true;
			}
			break;

		case EVENT_WEAPON_THROW2:
			// VR: We don't do any of this here, as it doesn't occur until the grenade is actually thrown			
			// RollGrenade( pOwner );
			// DecrementAmmo( pOwner );
			// fThrewGrenade = true;
			break;

		case EVENT_WEAPON_THROW3:
			LobGrenade( pOwner );
			DecrementAmmo( pOwner );
			fThrewGrenade = true;
			break;

		default:
			BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
			break;
	}

#define RETHROW_DELAY	0.5
	if( fThrewGrenade )
	{

		m_flNextPrimaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flNextSecondaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
		m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!

		// Make a sound designed to scare snipers back into their holes!
		CBaseCombatCharacter *pOwner = GetOwner();

		if( pOwner )
		{
			Vector vecSrc = pOwner->Weapon_ShootPosition();
			Vector	vecDir;

			AngleVectors( pOwner->EyeAngles(), &vecDir );

			trace_t tr;

			UTIL_TraceLine( vecSrc, vecSrc + vecDir * 1024, MASK_SOLID_BRUSHONLY, pOwner, COLLISION_GROUP_NONE, &tr );

			CSoundEnt::InsertSound( SOUND_DANGER_SNIPERONLY, tr.endpos, 384, 0.2, pOwner );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponFrag::Reload( void )
{
	if ( !HasPrimaryAmmo() )
		return false;

	if ( ( m_bRedraw ) && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		//Redraw the weapon
		SendWeaponAnim( ACT_VM_DRAW );

		//Update our times
		m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();

		//Mark this as done
		m_bRedraw = false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::SecondaryAttack( void )
{
	if ( m_bRedraw )
		return;

	if ( !HasPrimaryAmmo() )
		return;

	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pOwner );
	
	if ( pPlayer == NULL )
		return;

	// Note that this is a secondary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_SECONDARY;
	
	// TODO: REPLACE ME
	SendWeaponAnim( ACT_VM_PULLBACK_LOW ); 

	// Don't let weapon idle interfere in the middle of a throw!
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextSecondaryAttack	= FLT_MAX;

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SwitchToNextBestWeapon( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::PrimaryAttack( void )
{
	if ( m_bRedraw )
		return;

	CBaseCombatCharacter *pOwner  = GetOwner();
	
	if ( pOwner == NULL )
	{ 
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );;

	if ( !pPlayer )
		return;

	// Note that this is a primary attack and prepare the grenade attack to pause.
	m_AttackPaused = GRENADE_PAUSED_PRIMARY;
	SendWeaponAnim( ACT_VM_PULLBACK_HIGH );
	
	// Put both of these off indefinitely. We do not know how long
	// the player will hold the grenade.
	m_flTimeWeaponIdle = FLT_MAX;
	m_flNextPrimaryAttack = FLT_MAX;

	// If I'm now out of ammo, switch away
	if ( !HasPrimaryAmmo() )
	{
		pPlayer->SwitchToNextBestWeapon( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOwner - 
//-----------------------------------------------------------------------------
void CWeaponFrag::DecrementAmmo( CBaseCombatCharacter *pOwner )
{
	pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponFrag::ItemPostFrame( void )
{
	if( m_fDrawbackFinished )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

		if (pOwner)
		{
			switch( m_AttackPaused )
			{
			case GRENADE_PAUSED_PRIMARY:
				DrawArc(); 		
				if( !(pOwner->m_nButtons & IN_ATTACK) )
				{
					m_bMotionThrow = false; 
					SendWeaponAnim( ACT_VM_THROW );
					m_fDrawbackFinished = false;
					HideArc();
				}
				break;

			case GRENADE_PAUSED_SECONDARY:
				if( !(pOwner->m_nButtons & IN_ATTACK2) )
				{
					MotionThrowGrenade( pOwner, true );
				}
				else
				{
					MotionThrowGrenade( pOwner, false );
				}
				break;

			default:
				break;
			}
		}
	}

	BaseClass::ItemPostFrame();

	if ( m_bRedraw )
	{
		if ( IsViewModelSequenceFinished() )
		{
			Reload();
		}
	}
}

	// check a throw from vecSrc.  If not valid, move the position back along the line to vecEye
void CWeaponFrag::CheckThrowPosition( CBasePlayer *pPlayer, const Vector &vecEye, Vector &vecSrc )
{
	trace_t tr;

	UTIL_TraceHull( vecEye, vecSrc, -Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), Vector(GRENADE_RADIUS+2,GRENADE_RADIUS+2,GRENADE_RADIUS+2), 
		pPlayer->PhysicsSolidMaskForEntity(), pPlayer, pPlayer->GetCollisionGroup(), &tr );
	
	if ( tr.DidHit() )
	{
		vecSrc = tr.endpos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::ThrowGrenade( CBasePlayer *pPlayer )
{
	Vector	vecHand = pPlayer->EyePosition() + pPlayer->EyeToWeaponOffset();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecHand + vForward*3.0f + vRight*3.f;
	// CheckThrowPosition( pPlayer, vecHand, vecSrc );
//	vForward[0] += 0.1f;
	vForward[2] += 0.1f;

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 1200;
	Fraggrenade_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(600,random->RandomInt(-1200,1200),0), pPlayer, GRENADE_TIMER, false );

	m_bRedraw = true;

	WeaponSound( SINGLE );

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::LobGrenade( CBasePlayer *pPlayer )
{
	Vector	vecEye = pPlayer->EyePosition();
	Vector	vForward, vRight;

	pPlayer->EyeVectors( &vForward, &vRight, NULL );
	Vector vecSrc = vecEye + vForward * 18.0f + vRight * 8.0f + Vector( 0, 0, -8 );
	CheckThrowPosition( pPlayer, vecEye, vecSrc );
	
	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vForward * 350 + Vector( 0, 0, 50 );
	Fraggrenade_Create( vecSrc, vec3_angle, vecThrow, AngularImpulse(200,random->RandomInt(-600,600),0), pPlayer, GRENADE_TIMER, false );

	WeaponSound( WPN_DOUBLE );

	m_bRedraw = true;

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPlayer - 
//-----------------------------------------------------------------------------
void CWeaponFrag::RollGrenade( CBasePlayer *pPlayer )
{
	// BUGBUG: Hardcoded grenade width of 4 - better not change the model :)
	Vector vecSrc;
	pPlayer->CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.0f ), &vecSrc );
	vecSrc.z += GRENADE_RADIUS;

	Vector vecFacing = pPlayer->BodyDirection2D( );
	// no up/down direction
	vecFacing.z = 0;
	VectorNormalize( vecFacing );
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc - Vector(0,0,16), MASK_PLAYERSOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction != 1.0 )
	{
		// compute forward vec parallel to floor plane and roll grenade along that
		Vector tangent;
		CrossProduct( vecFacing, tr.plane.normal, tangent );
		CrossProduct( tr.plane.normal, tangent, vecFacing );
	}
	vecSrc += (vecFacing * 18.0);
	CheckThrowPosition( pPlayer, pPlayer->WorldSpaceCenter(), vecSrc );

	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	vecThrow += vecFacing * 700;
	// put it on its side
	QAngle orientation(0,pPlayer->GetLocalAngles().y,-90);
	// roll it
	AngularImpulse rotSpeed(0,0,720);
	Fraggrenade_Create( vecSrc, orientation, vecThrow, rotSpeed, pPlayer, GRENADE_TIMER, false );

	WeaponSound( SPECIAL1 );

	m_bRedraw = true; 

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
}

#define MOTION_CHECK_RATE .02
void CWeaponFrag::MotionThrowGrenade( CBasePlayer *pPlayer, bool release )
{
	if ( !release ) // todo: split this out
	{
		if ( gpGlobals->curtime > m_lastSample + MOTION_CHECK_RATE )
		{
			m_lastPosition = pPlayer->EyeToWeaponOffset();
			m_lastAngle = pPlayer->EyeAngles();
			m_lastSample = gpGlobals->curtime; 
		}

		return;
	}
	
	m_bMotionThrow = true;
	SendWeaponAnim( ACT_VM_THROW );
	m_fDrawbackFinished = false;
	
	Vector	handPosition = pPlayer->EyePosition() + pPlayer->EyeToWeaponOffset();

	Vector handMovement = pPlayer->EyeToWeaponOffset() - m_lastPosition;
	QAngle handAngle = pPlayer->EyeAngles();
	
	float dt = gpGlobals->curtime - m_lastSample;
	float dx = AngleDiff(handAngle.x, m_lastAngle.x);
	float dy = AngleDiff(handAngle.y, m_lastAngle.y);
	float dz = AngleDiff(handAngle.z, m_lastAngle.z);

	Vector angularImpulse = AngularImpulse(dx/dt, dy/dt, dz/dt);
	Vector vecThrow;
	pPlayer->GetVelocity( &vecThrow, NULL );
	
	vecThrow += ( handMovement * MOTION_THROW_SCALE ) / dt;

	Fraggrenade_Create( handPosition, handAngle, vecThrow, angularImpulse, pPlayer, GRENADE_TIMER, false );
	DecrementAmmo( pPlayer );
	
	m_bRedraw = true;
	WeaponSound( SINGLE );

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );
	m_flNextPrimaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
	m_flNextSecondaryAttack	= gpGlobals->curtime + RETHROW_DELAY;
	m_flTimeWeaponIdle = FLT_MAX; //NOTE: This is set once the animation has finished up!

	// some weird hack for a sniper sound
	Vector forward;
	pPlayer->EyeVectors(&forward);
	trace_t tr;
	UTIL_TraceLine( handPosition, handPosition + forward*1024, MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
	CSoundEnt::InsertSound( SOUND_DANGER_SNIPERONLY, tr.endpos, 384, 0.2, pPlayer );
}



void CWeaponFrag::DrawArc( bool primary )
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
	shootDirection[2] += .1f;
	throwVelocity +=  shootDirection * 1000;
	throwVelocity *= ARC_TIME_UNIT;  

	Vector vecGravity = Vector(0,0,-GetCurrentGravity() * ARC_TIME_UNIT * ARC_TIME_UNIT);
	
	Vector last = origin;
	bool impacted = false;

	// Reset the indicator pulse time
	float pulseSpeedScale = 1;

	if ( m_pulseStart == 0 )
		m_pulseStart = curtime + NUM_ARC_POINTS*ARC_TIME_UNIT*pulseSpeedScale; // skip it for the first pass

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


void CWeaponFrag::HideArc( )
{
	for ( int i = 0; i < NUM_ARC_POINTS; i ++ )
	{
		if ( m_hArcPoints[i] != NULL )
		{
			m_hArcPoints[i]->TurnOff();
			
		}
	}
}

