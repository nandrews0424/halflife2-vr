!include "MUI2.nsh"
!define VERSION '1.2.3'
!define DEPENDENCY_VERSION '1.2.0'

Name "Half-Life VR"

OutFile ".\output\halflife-vr-${VERSION}-sourcemod-updater.exe"

Var SDK_INSTALLED
Var STEAM_EXE

; !define MUI_ICON ".\images\nsis.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP ".\images\header.bmp" ; optional
  
;Welcome Page Settings
!define MUI_WELCOMEFINISHPAGE_BITMAP ".\images\welcome2.bmp"; 164x314 
!define MUI_WELCOMEPAGE_TITLE 'Welcome to the Source Mod VR updater'
!define MUI_WELCOMEPAGE_TEXT "This installer will attempt to upgrade a mod with VR support.  There are no guarantees that everything will function correctly with all mods, and this is a try at your own risk operation.  You should probably back up the mod folder before running this installer."

!insertmacro MUI_PAGE_WELCOME

; Directory Page
!define MUI_PAGE_HEADER_TEXT "  Select the mod to upgrade"
!define MUI_PAGE_HEADER_SUBTEXT "Please select an installation folder for the mod you want to add VR support for."
!define MUI_DIRECTORYPAGE_TEXT_TOP "This installer will overwrite files in the selected mod folder to add VR support.  Select the mod folder you'd like to upgrade"
!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Select your mod"
!insertmacro MUI_PAGE_DIRECTORY


!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_TITLE 'Installation Completed Successfully.'
!define MUI_FINISHPAGE_TEXT "The selected has been VR-ified (hopefully).  Don't forget to add the -vr options in the steam startup.  Depending on the mod it may still require some editing of the gameinfo.txt in the mod's installation folder."
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "" 
	
	SetOutPath $INSTDIR\bin
	File /r ..\mod_episodic\bin\*.dll
		
	SetOutPath $INSTDIR\scripts
	File ..\mod_episodic\scripts\vgui_screens.txt
	File ..\mod_episodic\scripts\weapon_*
	SetOutPath $INSTDIR\scripts\screens
	File /r ..\mod_episodic\scripts\screens\*

	SetOutPath $INSTDIR\models
	File /r ..\mod_episodic\models\*
	
	SetOutPath $INSTDIR\materials
	File /r ..\mod_episodic\materials\*

	SetOutPath $INSTDIR\particles
	File /r ..\mod_episodic\particles\*

	SetOutPath $INSTDIR\cfg
	File ..\mod_episodic\cfg\autoexec.cfg
	File ..\mod_episodic\cfg\config.cfg
	File ..\mod_episodic\cfg\skill*

	SetOutPath $INSTDIR\resource
	File /r ..\mod_episodic\resource\*

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