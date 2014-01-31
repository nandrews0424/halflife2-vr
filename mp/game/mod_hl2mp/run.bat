rmdir /s /q "C:\Program Files (x86)\Steam\SteamApps\sourcemods\halflife-vr-mp"
xcopy "%CD%" "C:\Program Files (x86)\Steam\SteamApps\sourcemods\halflife-vr-mp" /S /E
"C:\Program Files (x86)\Steam\steamapps\common\Source SDK Base 2013 Multiplayer\hl2.exe" -w 1280 -h 800 -novid -game "C:\Program Files (x86)\Steam\SteamApps\sourcemods\halflife-vr-mp"