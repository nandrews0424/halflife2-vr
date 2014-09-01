rm ../mod_hl2/bin/*.pdb
rm ../mod_episodic/bin/*.pdb
rm ../mod_ep2/bin/*.pdb

cp -r ../mod_episodic/bin ../mod_ep2/

/c/Program\ Files\ \(x86\)/NSIS/makensis.exe installer.nsi