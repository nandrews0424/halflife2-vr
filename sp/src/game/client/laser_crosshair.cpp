#include "cbase.h"
#include "laser_crosshair.h"

ConVar crosshair_laser("crosshair_laser", "0", FCVAR_REPLICATED, "Enables crosshair laser");

C_LaserSprite::C_LaserSprite()
{
	m_laserDot = new C_Sprite();
	bool success = m_laserDot->InitializeAsClientEntity(NULL, RENDER_GROUP_OTHER);
	if (success)
	{
		m_laserDot->SetMoveType(MOVETYPE_NONE);
		m_laserDot->AddSolidFlags(FSOLID_NOT_SOLID);
		m_laserDot->AddEffects(EF_NOSHADOW);
		m_laserDot->SpriteInit("sprites/laserdot.vmt", Vector(0, 0, 0));
		m_laserDot->SetTransparency(kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation);
		m_laserDot->SetScale(.5f);
		m_laserDot->SetOwnerEntity(C_BasePlayer::GetLocalPlayer());
		m_laserDot->SetSimulatedEveryTick(true);
	}

	m_laserBeam = new C_Beam();
	success = m_laserBeam->InitializeAsClientEntity(NULL, RENDER_GROUP_OTHER);
	if (success)
	{
		m_laserBeam->BeamInit("effects/laser1_noz.vmt", 10.f);
		m_laserBeam->SetNoise(0);
		m_laserBeam->SetColor(255, 0, 0);
		m_laserBeam->SetScrollRate(0);
		m_laserBeam->SetWidth(0.5f);
		m_laserBeam->SetEndWidth(0.5f);
		m_laserBeam->SetBrightness(172);
		m_laserBeam->SetBeamFlags(SF_BEAM_SHADEIN);
	}

	m_laserMuzzleGlow = new C_Sprite();
	success = m_laserMuzzleGlow->InitializeAsClientEntity(NULL, RENDER_GROUP_OTHER);
	if (success)
	{
		m_laserMuzzleGlow->SetMoveType(MOVETYPE_NONE);
		m_laserMuzzleGlow->AddSolidFlags(FSOLID_NOT_SOLID);
		m_laserMuzzleGlow->AddEffects(EF_NOSHADOW);
		m_laserMuzzleGlow->SpriteInit("sprites/redglow1.vmt", Vector(0,0,0));

		// TODO: m_hLaserMuzzleSprite->SetAttachment(pOwner->GetViewModel(), LookupAttachment("laser"));
		m_laserMuzzleGlow->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation);
		m_laserMuzzleGlow->SetBrightness(255, 0.5f);
		m_laserMuzzleGlow->SetScale(0.025f, 0.5f);
		m_laserMuzzleGlow->TurnOn();
	}
}


void C_LaserSprite::Update()
{
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
		
	if (!pPlayer)
		return;

	C_BaseViewModel* pViewModel = pPlayer->GetViewModel();
	if (!pViewModel)
		return;
	
	C_BaseCombatWeapon* pWeapon = pPlayer->GetActiveWeapon();

	
	// Should the laser just be off?
	if (!pWeapon || pWeapon->GetWpnData().laserCrosshairScale <= 0 ) // TODO: need crosshair_laser check here as well!!!!
	{
		m_laserDot->TurnOff();
		m_laserMuzzleGlow->TurnOff();
		m_laserBeam->SetBrightness(0);
		return;
	}


	Vector muzzlePos, forward, right, up, traceEnd;
	QAngle angles;

	int idxAttachment = pViewModel->LookupAttachment("muzzle");
	pViewModel->GetAttachment(idxAttachment, muzzlePos, angles);
	AngleVectors(angles, &forward, &right, &up);
	
	float laserLength = 80;

	if ( m_laserDot != NULL )
	{
		if ( !m_laserDot->IsOn() )
			m_laserDot->TurnOn();


		// Update the position
		traceEnd = muzzlePos + (forward * MAX_TRACE_LENGTH);
		trace_t tr;
		UTIL_TraceLine(muzzlePos, traceEnd, (MASK_SHOT & ~CONTENTS_WINDOW), m_laserDot, COLLISION_GROUP_NONE, &tr);
		m_laserDot->SetAbsOrigin(tr.endpos);

		// laser length is applied to the beam, this is helpful here...
		Vector	viewDir = tr.endpos - muzzlePos;
		if (viewDir.Length() < 80)
			laserLength = viewDir.Length();
		
		// Modulate the scale as well
		float	scale = RemapVal(viewDir.Length(), 32, 1024, 0.01f, 0.5f);
		float	scaleOffs = random->RandomFloat(-scale * 0.25f, scale * 0.25f);

		scale = clamp(scale + scaleOffs, 0.1f, 32.0f);
		m_laserDot->SetScale(scale);

		m_laserDot->SetBrightness(220 + random->RandomInt(-24, 24));
	}
		
	
	if ( m_laserBeam != NULL )
	{
		
		// update the beam positioning
		m_laserBeam->SetParent(pViewModel);
		m_laserBeam->SetStartAttachment(idxAttachment);

		
		// it's now relative to viewmodel
		m_laserBeam->SetEndPos( Vector(laserLength, 0, 0));

		// modulate it's brightness as well
		m_laserBeam->SetBrightness(170 + random->RandomInt(-24, 24));
	}

	if (m_laserMuzzleGlow != NULL)
	{
		if (!m_laserMuzzleGlow->IsOn())
			m_laserMuzzleGlow->TurnOn();

		m_laserMuzzleGlow->SetAttachment(pViewModel, idxAttachment);
		m_laserMuzzleGlow->SetScale(0.025f + random->RandomFloat(-0.01f, 0.01f));
	}
}

void C_LaserSprite::TurnOn()
{

}

void C_LaserSprite::TurnOff()
{

}
