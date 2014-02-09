//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		357 - hand gun
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "soundent.h"
#include "game.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "te_effect_dispatch.h"
#include "gamestats.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MOTION_RELOAD true

//-----------------------------------------------------------------------------
// CWeapon357
//-----------------------------------------------------------------------------

class CWeapon357 : public CBaseHLCombatWeapon
{
	DECLARE_CLASS( CWeapon357, CBaseHLCombatWeapon );
public:

	CWeapon357( void );

	void	Precache( void );
	void	PrimaryAttack( void );
	bool	Reload( void );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	void	ItemPostFrame();
	
	float	WeaponAutoAimScale()	{ return 0.6f; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

private:
	float m_iShotsFired;
	float m_iLastShotTime;
	
	// motion reload
	bool	ShouldOpenCylinder( CBasePlayer* pPlayer );
	bool	ShouldCloseCylinder( CBasePlayer* pPlayer );
	bool	ShouldEmptyShells( CBasePlayer* pPlayer );
	void	EmptyShells( CBasePlayer* pPlayer );
	bool	GetLocalAcceleration( CBasePlayer* pPlayer, Vector& acceleration );
	
	bool m_bCylinderLatched;
	float m_fLastReloadActivityDone;
	Activity m_NextReloadActivity;
	bool m_bFullNewShells;
	
	inline bool LastReloadActivityDone() { return gpGlobals->curtime > m_fLastReloadActivityDone; }
	inline void SetNextReloadActivity(Activity a) 
	{ 
		m_NextReloadActivity = a;
		m_fLastReloadActivityDone = gpGlobals->curtime + SequenceDuration(); 
	}
	
	float  m_fLastUpdate;
	Vector m_lastOrigin;
	QAngle m_lastAngle;
	Vector m_lastVelocity;




};

LINK_ENTITY_TO_CLASS( weapon_357, CWeapon357 );

PRECACHE_WEAPON_REGISTER( weapon_357 );

IMPLEMENT_SERVERCLASS_ST( CWeapon357, DT_Weapon357 )
END_SEND_TABLE()

BEGIN_DATADESC( CWeapon357 )
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon357::CWeapon357( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= false;
	m_bFullNewShells = false;

	m_bCylinderLatched = true;
	m_NextReloadActivity = ACT_VM_RELOAD_RELEASE_DYNAMIC;
	
	m_lastVelocity.Init();

	m_iShotsFired = 0;
	m_iLastShotTime = 0;
	m_fLastUpdate = -1;
}

void CWeapon357::Precache()
{
	PrecacheParticleSystem( "weapon_muzzle_smoke" );
	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	switch( pEvent->event )
	{
		case EVENT_WEAPON_RELOAD:
			{
				CEffectData data;

				// Emit six spent shells
				for ( int i = 0; i < 6; i++ )
				{
					data.m_vOrigin = pOwner->WorldSpaceCenter() + RandomVector( -4, 4 );
					data.m_vAngles = QAngle( 90, random->RandomInt( 0, 360 ), 0 );
					data.m_nEntIndex = entindex();

					DispatchEffect( "ShellEject", data );
				}

				break;
			}
	}
}



bool CWeapon357::Reload( void )
{
	if ( !MOTION_RELOAD )
		return BaseClass::Reload();
	
	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return BaseClass::Reload();

	CBasePlayer *pPlayer = ToBasePlayer( pOwner );
	
	if ( pPlayer == NULL )
		return BaseClass::Reload();
		
	
	if ( m_bCylinderLatched == true )
	{
		m_bCylinderLatched = false;
		m_flTimeWeaponIdle		= FLT_MAX;
		m_flNextPrimaryAttack	= FLT_MAX;
				
		SendWeaponAnim( ACT_VM_RELOAD_RELEASE_DYNAMIC );
		SetNextReloadActivity( ACT_VM_RELOAD_OPEN_DYNAMIC );
	}

	return true;
}


void CWeapon357::ItemPostFrame()
{
	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( pOwner == NULL )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( pOwner );
	
	if ( pPlayer == NULL )
		return;
	
	if ( !m_bCylinderLatched )
	{

		if ( m_NextReloadActivity == ACT_VM_RELOAD_OPEN_DYNAMIC && LastReloadActivityDone() )
		{
			// while open, keep running the same animation
			SendWeaponAnim( ACT_VM_RELOAD_IDLE_DYNAMIC  );  
			
			if ( ShouldOpenCylinder( pPlayer ) )
			{
				SendWeaponAnim( ACT_VM_RELOAD_OPEN_DYNAMIC );
				SetNextReloadActivity(ACT_VM_RELOAD_FULL_DYNAMIC);
				m_bFullNewShells = false;
			}
		}
		else if (  m_NextReloadActivity == ACT_VM_RELOAD_FULL_DYNAMIC && !m_bFullNewShells && LastReloadActivityDone() ) 
		{
			// while open, keep running the same animation
			SendWeaponAnim( ACT_VM_RELOAD_FULL_DYNAMIC );
			
			if ( ShouldEmptyShells( pPlayer ))
			{
				EmptyShells( pPlayer );

				SendWeaponAnim( ACT_VM_RELOAD_EMPTY_DYNAMIC );  
				SetNextReloadActivity( ACT_VM_RELOAD_INSERT_DYNAMIC  );
			}
			else if ( ShouldCloseCylinder( pPlayer ))
			{
				// just closed without a reload
				SendWeaponAnim( ACT_VM_RELOAD_CLOSE_DYNAMIC );  
				SetNextReloadActivity( ACT_VM_RELOAD  );
			}
		}
		else if ( m_NextReloadActivity == ACT_VM_RELOAD_INSERT_DYNAMIC  && LastReloadActivityDone() )
		{
			SendWeaponAnim( ACT_VM_RELOAD_INSERT_DYNAMIC );
			SetNextReloadActivity(ACT_VM_RELOAD_FULL_DYNAMIC);
			m_bFullNewShells = true;
		} 
		else if (  m_NextReloadActivity == ACT_VM_RELOAD_FULL_DYNAMIC && m_bFullNewShells && LastReloadActivityDone() ) 
		{
			// while open, keep running the same animation
			SendWeaponAnim( ACT_VM_RELOAD_FULL_DYNAMIC );  
			
			if ( ShouldCloseCylinder( pPlayer ))
			{
				SendWeaponAnim( ACT_VM_RELOAD_CLOSE_DYNAMIC );  
				SetNextReloadActivity( ACT_VM_RELOAD  );
			}
		}
		else if ( m_NextReloadActivity == ACT_VM_RELOAD && LastReloadActivityDone() ) 
		{
			m_bCylinderLatched		= true;
			m_flTimeWeaponIdle		= gpGlobals->curtime;
			m_flNextPrimaryAttack	= gpGlobals->curtime;

			if ( m_bFullNewShells )
			{
				DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_IDLE );
			}
		}
	}

	BaseClass::ItemPostFrame();	
}

#define MOTION_CHECK_RATE .02

bool CWeapon357::GetLocalAcceleration( CBasePlayer* pPlayer, Vector& localAcceleration )
{
	Vector currentOrigin = pPlayer->EyeToWeaponOffset();

	// first reading of this stream should reinitialize
	if ( gpGlobals->curtime > m_fLastUpdate + 5*MOTION_CHECK_RATE ) {
		m_lastOrigin = currentOrigin;
		m_fLastUpdate = gpGlobals->curtime;
		return false;
	}
	else if ( gpGlobals->curtime < m_fLastUpdate + MOTION_CHECK_RATE )
	{
		return false; // we don't check every time
	}
			
	float elapsed = gpGlobals->curtime - m_fLastUpdate;
	Vector velocity = (currentOrigin - m_lastOrigin) / elapsed;
	Vector accel = velocity - m_lastVelocity;	
	
	// scaled down the effects of the gravity by half by feel
	float gravity = GetCurrentGravity() * elapsed*.5;  // 9.0
	accel += Vector(0,0,gravity); 
	
	m_lastVelocity = velocity;
	
	// translate to weapon space. 
	VMatrix worldFromWeapon;
	AngleMatrix(pPlayer->EyeAngles(), worldFromWeapon.As3x4());
	localAcceleration = worldFromWeapon.InverseTR() * accel;

	// store of values for next frame ( Abs on both )
	m_lastVelocity = velocity;
	m_lastOrigin = currentOrigin;
	m_fLastUpdate = gpGlobals->curtime;

	return true;
}


bool CWeapon357::ShouldOpenCylinder( CBasePlayer* pPlayer )
{
	Vector localAcceleration;
	if ( GetLocalAcceleration( pPlayer, localAcceleration) ) 
		return localAcceleration.y < -9.75f;

	return false;
}

bool CWeapon357::ShouldCloseCylinder( CBasePlayer* pPlayer )
{
	Vector localAcceleration;
	if ( GetLocalAcceleration( pPlayer, localAcceleration) ) 
		return localAcceleration.y > 7.5f;

	return false;
}

bool CWeapon357::ShouldEmptyShells( CBasePlayer* pPlayer )
{
	Vector aimDirection;
	pPlayer->EyeVectors(&aimDirection);
	Vector up(0,0,1);

	return up.Dot(aimDirection) > .75f;
}


void CWeapon357::EmptyShells( CBasePlayer* pPlayer )
{
	CEffectData data;
	data.m_nEntIndex = entindex();
	DispatchEffect( "MagnumShellEject", data );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon357::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		WeaponSound( EMPTY );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, true, GetClassname() );

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.75;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.75;

	m_iClip1--;

	Vector vecSrc		= pPlayer->Weapon_ShootPosition();
	Vector vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	pPlayer->FireBullets( 1, vecSrc, vecAiming, vec3_origin, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0 );

	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	//Disorient the player
	QAngle angles = pPlayer->GetLocalAngles();

	angles.x += random->RandomInt( -1, 1 );
	angles.y += random->RandomInt( -1, 1 );
	angles.z = 0;

	pPlayer->SnapEyeAngles( angles );

	pPlayer->ViewPunch( QAngle( -8, random->RandomFloat( -2, 2 ), 0 ) );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, GetOwner() );

	if ( m_iLastShotTime > gpGlobals->curtime - 1 )
		m_iShotsFired++;
	else
		m_iShotsFired = 1;

	if ( m_iShotsFired == 3 )
	{
		m_iShotsFired = 0;
		DispatchParticleEffect("weapon_muzzle_smoke", PATTACH_POINT_FOLLOW, pPlayer->GetViewModel(), "muzzle", true );
	}
	m_iLastShotTime = gpGlobals->curtime;

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 ); 
	}
}
