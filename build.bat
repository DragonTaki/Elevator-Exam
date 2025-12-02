@echo off
:: ===============================
::  build.bat - Build executable
::  Author: DragonTaki
:: ===============================
cd /d %~dp0

:: Setup VS environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

:: Create folders
if not exist build mkdir build
if not exist build\obj mkdir build\obj

:: Clean old files
del /q build\obj\*.obj 2>nul
del /q build\main.exe 2>nul

echo.
echo ==============================
echo  Compiling src/core/*.c ...
echo ==============================

for /R src\core %%F in (*.c) do (
    echo Compiling %%F
    cl ^
     /TC ^
     /W4 ^
     /Od ^
     /Zi ^
     /utf-8 ^
     /I"." ^
     /I"src\core" ^
     /I"src\network" ^
     /c "%%F" ^
     /Fo"build\obj\%%~nF.obj" ^
     /Fd"build\elevator.pdb" >nul

    if errorlevel 1 (
        echo Compile failed on %%F
        exit /b 1
    )
)

echo.
echo ==============================
echo  Compiling src/network/*.c ...
echo ==============================

for /R src\network %%F in (*.c) do (
    echo Compiling %%F
    cl ^
     /TC ^
     /W4 ^
     /Od ^
     /Zi ^
     /utf-8 ^
     /I"." ^
     /I"src\core" ^
     /I"src\network" ^
     /c "%%F" ^
     /Fo"build\obj\%%~nF.obj" ^
     /Fd"build\elevator.pdb" >nul

    if errorlevel 1 (
        echo Compile failed on %%F
        exit /b 1
    )
)

echo.
echo ==============================
echo  Compiling main.c ...
echo ==============================

cl ^
 /TC ^
 /W4 ^
 /Od ^
 /Zi ^
 /utf-8 ^
 /I"." ^
 /I"src\core" ^
 /I"src\network" ^
 /c main.c ^
 /Fo"build\obj\main.obj" ^
 /Fd"build\elevator.pdb"

if errorlevel 1 (
    echo Compile failed on main.c
    exit /b 1
)

echo.
echo Linking main.exe ...

link /DEBUG /PDB:"build\main.pdb" ^
 /OUT:"build\main.exe" ^
 build\obj\*.obj ^
 ws2_32.lib

if errorlevel 1 (
    echo Link main.exe failed.
    exit /b 1
)

echo.
echo Build successful: build\main.exe
