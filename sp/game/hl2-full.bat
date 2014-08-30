

echo "copying to sourcemods folder"
xcopy "%cd%\mod_hl2" "%STEAMDIR%\steamapps\sourcemods\halflife-vr" /E /Y


rem "%STEAMDIR%\steamapps\common\Source SDK Base 2013 Singleplayer\hl2.exe"  -vrforce -refresh 75 -freq 75 -novid -game "%STEAMDIR%\steamapps\sourcemods\halflife-vr"