SET MOD_FOLDER=%1
IF "%MOD_FOLDER%" == "" SET MOD_FOLDER="mod_hl2"

SET TARGET_FOLDER="halflife-vr"
SET STEAM_APP_ID="11383574143892174866"

IF "%MOD_FOLDER%" == "mod_episodic" SET TARGET_FOLDER="halflife-vr-ep1"
IF "%MOD_FOLDER%" == "mod_ep2" SET TARGET_FOLDER="halflife-vr-ep2"

IF "%MOD_FOLDER%" == "mod_episodic" SET STEAM_APP_ID="11383574143892174866"
IF "%MOD_FOLDER%" == "mod_ep2" SET STEAM_APP_ID="15236064928281638930"



echo "copying binaries to sourcemods folder"
xcopy "%cd%\%MOD_FOLDER%\bin" "%STEAMDIR%\steamapps\sourcemods\%TARGET_FOLDER%\bin" /E /Y

echo "copying cfg to sourcemods folder"
xcopy "%cd%\%MOD_FOLDER%\cfg" "%STEAMDIR%\steamapps\sourcemods\%TARGET_FOLDER%\cfg" /E /Y

start "" "steam://rungameid/%STEAM_APP_ID%"