#ifndef LASER_DOT_H
#define LASER_DOT_H

#include "Sprite.h"

//-----------------------------------------------------------------------------
// Laser Dot
//-----------------------------------------------------------------------------
class CLaserCrosshair : public CSprite 
{
	DECLARE_CLASS( CLaserCrosshair, CSprite );
public:

	CLaserCrosshair( void );
	~CLaserCrosshair( void );

	static CLaserCrosshair *Create( const Vector &origin, CBaseEntity *pOwner = NULL, bool bVisibleDot = true );

	void	TurnOn( void );
	void	TurnOff( void );
	bool	IsOn() const { return m_bIsOn; }

	void	Toggle( void );

	void	LaserThink( void );
	void	SetLaserPosition( const Vector& pos, const Vector& normal);

	int		ObjectCaps() { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }

	void	MakeInvisible( void );

	void	Precache( void );

protected:
	Vector				m_vecSurfaceNormal;
	bool				m_bVisibleLaserDot;
	bool				m_bIsOn;

	DECLARE_DATADESC();
};

#endif