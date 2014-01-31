//The following include files are necessary to allow your MyPanel.cpp to compile.

#include "cbase.h"
#include "vgui_vr_panel.h"
#include <vgui/IVGui.h>
#include <vgui/IVGUI.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Frame.h>
#include "vr/vr_controller.h"

#include "tier0/memdbgon.h"

ConVar cl_showvrpanel("cl_showvrpanel", "1", FCVAR_CLIENTDLL, "Sets the state of VR Panel <state>");


 //CMyPanel class: Tutorial example class
 class CVRPanel : public vgui::Panel
 {
 	DECLARE_CLASS_SIMPLE(CVRPanel, vgui::Panel); 
 	//CMyPanel : This Class / vgui::Frame : BaseClass
 
 	CVRPanel(vgui::VPANEL parent); 	// Constructor
 	~CVRPanel(){};				// Destructor
 
 protected:
 	//VGUI overrides:
 	virtual void OnTick();
 
 private:
 	//Other used VGUI control Elements:
	vgui::Label		*m_pStatus;
	vgui::HFont		m_hTextFont;
	vgui::ImagePanel *m_pImage;

 };


 // Constuctor: Initializes the Panel
CVRPanel::CVRPanel(vgui::VPANEL parent)
: BaseClass(NULL, "VRPanel")
{

	int w, h;
	w = ScreenWidth();
	h = ScreenHeight();

	SetParent( parent );
 
	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );
 
	SetProportional( false );
	SetVisible( true );
	
	SetSize(150, 150);
	SetPos(350, 280);
	
	vgui::ivgui()->AddTickSignal( GetVPanel(), 100 );
	
	// Label
	vgui::IScheme* scheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );
	vgui::HFont font = scheme->GetFont( "MenuLarge" );
	m_pStatus = new vgui::Label(this, "lbl", "Dock your hydras!");
	m_pStatus->SetPos(10, 0);
	m_pStatus->SetSize(180,40);
	m_pStatus->SetFont(font);
	m_pStatus->SetFgColor(Color(255, 0, 0, 200));
	
	// Image
	m_pImage = new vgui::ImagePanel(this, "imgCalibrate");
	m_pImage->SetImage("hydra_calibrate");
	m_pImage->SetPos(27, 38);
	m_pImage->SetSize(150, 150);
	

	DevMsg("VRPanel has been constructed\n");
}


void CVRPanel::OnTick()
{
	BaseClass::OnTick();
	SetVisible(g_MotionTracker()->showMenuPanel()); 
}


//Class: CMyPanelInterface Class. Used for construction.

class CVRPanelInterface : public IVRPanel
{
private:
	CVRPanel *MyPanel;
public:
	CVRPanelInterface()
	{
		MyPanel = NULL;
	}
	void Create(vgui::VPANEL parent)
	{
		MyPanel = new CVRPanel(parent);
	}
	void Destroy()
	{
		if (MyPanel)
		{
			MyPanel->SetParent( (vgui::Panel *)NULL);
			delete MyPanel;
		}
	}
};

static CVRPanelInterface g_VRPanel;
IVRPanel *vrPanel = (IVRPanel*) &g_VRPanel;