echo "copying binaries to sourcemods folder"
xcopy "%cd%\mod_hl2\bin" "%STEAMDIR%\steamapps\sourcemods\halflife-vr\bin" /E /Y

echo "copying cfg to sourcemods folder"
xcopy "%cd%\mod_hl2\cfg" "%STEAMDIR%\steamapps\sourcemods\halflife-vr\cfg" /E /Y

start "" "steam://rungameid/11383574143892174866"