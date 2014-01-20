!include "MUI2.nsh"
!define VERSION '1.2.0'

Name "Half-Life VR"

OutFile ".\output\halflife-vr-${VERSION}.exe"

Var SDK_INSTALLED
Var STEAM_EXE

; !define MUI_ICON ".\images\nsis.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP ".\images\header.bmp" ; optional
  
;Welcome Page Settings
!define MUI_WELCOMEFINISHPAGE_BITMAP ".\images\welcome2.bmp"; 164x314 
!define MUI_WELCOMEPAGE_TITLE 'Welcome to the installation for Half-Life 2 VR v${VERSION}'
!define MUI_WELCOMEPAGE_TEXT 'Thanks for installing Half-Life VR, this should only take a few seconds.  \
If you have any issues or ideas please send feedback so we can improve the mod.'
!insertmacro MUI_PAGE_WELCOME

; Directory Page
!define MUI_PAGE_HEADER_TEXT "  Installation Directory"
!define MUI_PAGE_HEADER_SUBTEXT "Please select an installation path for Half-Life VR"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Half-Life VR needs to be installed in your local Steam installation.  Please verify the the following install path before continuing."
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Steam sourcemods folder"
!insertmacro MUI_PAGE_DIRECTORY


!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_TITLE 'Installation Completed Successfully.'
!define MUI_FINISHPAGE_TEXT "Half-life VR is now installed.  You'll need to restart steam to see the mod in your games list on Steam."
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "" 

	SetOutPath $INSTDIR\halflife-vr
	File /r ..\mod_hl2\*
	
	SetOutPath $INSTDIR\halflife-vr-ep1
	File /r ..\mod_episodic\*
	
SectionEnd

Function .onInit
	
	ReadRegDWORD $SDK_INSTALLED HKCU "Software\Valve\Steam\Apps\243730" "Installed"
	; Check for Source SDK 2013
	ReadRegStr $R1 HKCU "Software\Valve\Steam" "SourceModInstallPath"
	ReadRegStr $STEAM_EXE HKCU "Software\Valve\Steam" "SteamExe"
	
	StrCmp $SDK_INSTALLED "1" SDK_INSTALLED
		

		MessageBox MB_YESNO|MB_ICONQUESTION \
		    "The Source SDK 2013 is required to play this mod but wasn't \ 
		    found on your computer. Do you want cancel this installation and install it now?" \
		    IDNO SKIP_SDK_INTALL

		    execshell open "steam://install/243730"
		    
		    Abort

		SKIP_SDK_INTALL:
			

	SDK_INSTALLED:

	StrCpy $INSTDIR "$R1"
	
FunctionEnd