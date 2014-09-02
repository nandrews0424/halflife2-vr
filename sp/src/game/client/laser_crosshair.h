#pragma once

#ifndef C_VR_SPRITES_H
#define C_VR_SPRITES_H

#include "particles_simple.h"
#include "tempent.h"
#include "glow_overlay.h"
#include "view.h"
#include "particle_litsmokeemitter.h"
#include "beam_shared.h"



class C_LaserSprite
{
	
	// TODO: define the laser crosshair here...



	



public:
	C_Sprite *m_laserDot;
	C_Beam	 *m_laserBeam;
	C_Sprite *m_laserMuzzleGlow;
	
	C_LaserSprite();
	
	void		Update();

	void		TurnOff();
	void		TurnOn();

private:
	bool		m_laserBeamIsOn;
	

};


#endif
