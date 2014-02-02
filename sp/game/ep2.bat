mkdir "%cd%\mod_ep2\bin"
copy "%cd%\mod_episodic\bin\*.dll" "%cd%\mod_ep2\bin\"
"%STEAMDIR%\steamapps\common\Source SDK Base 2013 Singleplayer\hl2.exe" -w 1280 -h 800 -novid -game "%cd%\mod_ep2"