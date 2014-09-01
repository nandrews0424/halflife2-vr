SET MOD_FOLDER=%1
IF "%MOD_FOLDER%" == "" SET MOD_FOLDER="mod_hl2"

SET TARGET_FOLDER="halflife-vr"
SET STEAM_APP_ID="11383574143892174866"

IF "%MOD_FOLDER%" == "mod_episodic" SET TARGET_FOLDER="halflife-vr-ep1"
IF "%MOD_FOLDER%" == "mod_ep2" SET TARGET_FOLDER="halflife-vr-ep2"

echo "copying to sourcemods folder"
xcopy "%cd%\%MOD_FOLDER%" "%STEAMDIR%\steamapps\sourcemods\%TARGET_FOLDER%" /E /Y