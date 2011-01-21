PATH "C:\Program Files\NSIS";%PATH%

makensis plugin-updater.nsi
move plugin-updater.exe Sylpheed
makensis update-manager.nsi
move update-manager.exe Sylpheed
makensis sylpheed.nsi

rem makensis /DSYLPHEED_PRO plugin-updater.nsi
rem move plugin-updater.exe Sylpheed
rem makensis /DSYLPHEED_PRO update-manager.nsi
rem move update-manager.exe Sylpheed
rem makensis /DSYLPHEED_PRO sylpheed.nsi

pause
