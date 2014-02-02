### Half life 2 VR Mod

The intent of this mod is to provide an extended VR experience built on the baseline HL2 VR mode provided by Valve. This currently primarily focuses on integrating razer hydra for 1 to 1 weapon and player positional tracking.


### Building the mod for the first time

You'll need to have [visual c++ 2010 express](http://www.visualstudio.com/en-us/downloads#d-2010-express) installed to build the mod.

1. Clone the repository
2. Browse to sp/src
3. Run the createallprojects.bat file
4. Open the newly created everything.sln
5. Build the solution `ctrl+shift+b`.  You may see some errors in one or two of the projects but that shouldn't matter
6. Close the solution, run the creategameprojects.bat file, open the new games.sln file and build it as well

As you pull updates you'll only really ever need to do #6 moving forward, but if the vpc files are updated (adding new files to the project etc) the build will fail telling you you need to rerun the creategameprojects.bat to regenerate the updated projects.


###Running the game

You'll need the Source SDK 2013 SinglePlayer installed in steam to properly start the mod (currently it needs the 'upcoming' beta) and you'll need to have built the project (see above).

1. Create an environment variable `STEAMDIR` that points to your steam installation folder (ex. C:\Program Files (x86)\Steam)
2. Browse to sp/game and run any of the .bat files to start the game


###Packaging an installer

If you've built a version of the game that you want to create an installer you'll need to have [NSIS](http://nsis.sourceforge.net/Download) installed. 

1. Browse to sp/game/installer
2. Open up the installer.nsi file and update the version
3. Run make-installer.bat
4. Your installer should be in the output folder in the same directory
