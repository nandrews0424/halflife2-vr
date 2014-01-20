 
//=============================================================================//
//
// Purpose: Screen used to render player health information 
//
//=============================================================================//
#include "cbase.h"

#include "C_VGuiScreen.h"
#include <vgui/IVGUI.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include "clientmode_hlnormal.h"
#include "c_basehlplayer.h"
#include "usermessages.h"
#include "vr\vr_controller.h"
#include "client_virtualreality.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
// In-game vgui panel which shows the RPG's ammo count
//
//-----------------------------------------------------------------------------
class CHealthScreen : public CVGuiScreenPanel
{
    DECLARE_CLASS( CHealthScreen, CVGuiScreenPanel );

public:
    CHealthScreen( vgui::Panel *parent, const char *panelName );

    virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
    virtual bool ShouldPaint();
	virtual void Paint();
	void		 MsgFunc_Battery(bf_read &msg );

private:
    int				m_iSuitPower;
	int				m_iHealth;
	
	vgui::Label		*m_pHealth;
	vgui::Label		*m_pHealthLabel;
	vgui::Label		*m_pSuit;
	vgui::Label		*m_pSuitLabel;

	vgui::HFont		m_hNumberFont;
	vgui::HFont		m_hTextFont;
	Color			m_clrText;
};


DECLARE_VGUI_SCREEN_FACTORY( CHealthScreen, "health_screen" );

CHealthScreen::CHealthScreen( vgui::Panel *parent, const char *panelName )
    : BaseClass( parent, panelName, g_hVGuiCombineScheme ) 
{
 	// SetScheme(vgui::scheme()->GetScheme( "ClientScheme" ));
}


void CHealthScreen::ApplySchemeSettings( IScheme *scheme ) 
{
	BaseClass::ApplySchemeSettings(scheme);
	// SetBgColor(Color(0,0,0,100));
	SetProportional(false);
}

bool CHealthScreen::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
    if ( !BaseClass::Init(pKeyValues, pInitData) )
        return false;

    m_pHealth		=  dynamic_cast<vgui::Label*>(FindChildByName( "HealthReadout" ));
	m_pHealthLabel	=  dynamic_cast<vgui::Label*>(FindChildByName( "HealthLabel" ));
	m_pSuit			=  dynamic_cast<vgui::Label*>(FindChildByName( "SuitReadout" ));
	m_pSuitLabel	=  dynamic_cast<vgui::Label*>(FindChildByName( "SuitLabel" ));
	
	vgui::IScheme* scheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );

	m_hNumberFont = scheme->GetFont("VrHudNumbers");
	m_hTextFont = scheme->GetFont( "DefaultVerySmall" );
	m_clrText = scheme->GetColor( "FgColor", Color(255,220,0, 180) );
	
    return true;
}

bool CHealthScreen::ShouldPaint()
{
	C_BaseHLPlayer *pPlayer = (C_BaseHLPlayer *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	return pPlayer->GetHealth() != m_iHealth || pPlayer->GetSuitArmor() != m_iSuitPower;
}

void CHealthScreen::Paint()
{
	m_pHealth->SetFont(m_hNumberFont);
	m_pSuit->SetFont(m_hNumberFont);
	m_pHealthLabel->SetFont(m_hTextFont);
	m_pSuitLabel->SetFont(m_hTextFont);
	m_pHealthLabel->SetFgColor(m_clrText);
	m_pSuitLabel->SetFgColor(m_clrText);	

	// Get our player
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer*>( C_BasePlayer::GetLocalPlayer() );
    
	if ( !pPlayer )
        return;
	
	char buf[32];
	m_iHealth = pPlayer->GetHealth();
	m_iSuitPower = pPlayer->GetSuitArmor();
		
    if ( m_pHealth )
    {
	    Q_snprintf( buf, sizeof( buf ), "%i", m_iHealth );
		m_pHealth->SetFgColor(m_clrText);
		m_pHealth->SetText( buf );

    }

	if ( m_pSuit )
	{
	    Q_snprintf( buf, sizeof( buf ), "%i", m_iSuitPower ); 
		m_pSuit->SetFgColor(m_clrText);
		m_pSuit->SetText( buf );
	} 

	// VM attached panels should fade at off-angles
	VMatrix m(g_ClientVirtualReality.GetWorldFromMidEye());
	g_MotionTracker()->overrideWeaponMatrix(m);
	Vector weapRight	= m.GetLeft() * -1;
	Vector eyesForward	= g_ClientVirtualReality.GetWorldFromMidEye().GetForward();
		
	float alpha = g_MotionTracker()->getHudPanelAlpha(weapRight, eyesForward, 3.5);
	surface()->DrawSetAlphaMultiplier(alpha); 
	SetAlpha(255*alpha);
}