@echo off
rem Fabrique la version jouable du jeu dans le dossier Build\
rem Modifiez la ligne ci-dessous si Unreal est installe ailleurs ou dans une autre version.
set UE=C:\Program Files\Epic Games\UE_5.6

if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (
	echo Unreal Engine introuvable dans "%UE%".
	echo Ouvrez Package.bat avec le Bloc-notes et corrigez la ligne "set UE=".
	pause
	exit /b 1
)

call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%~dp0LaRuche.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="%~dp0Build"
echo.
echo Termine. Le jeu se trouve dans le dossier Build\ : compressez-le en .zip pour le partager.
pause
