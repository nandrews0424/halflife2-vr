#if !defined( IVRPANEL_H )
#define IVRPANEL_H 
#ifdef _WIN32
#pragma once
#endif

class IVRPanel
{
public:
	virtual void	Create( vgui::VPANEL parent ) = 0;
	virtual void	Destroy( void ) = 0;
};

extern IVRPanel* vrPanel;


#endif // IVRPANEL_H 