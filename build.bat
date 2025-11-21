@echo off
:: ===============================
::  build.bat - Build executable and run
::  Author: DragonTaki
:: ===============================
cd /d %~dp0

:: Set up VS compilation environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

:: Create "build" and "obj" folders
if not exist build mkdir build
if not exist build\obj mkdir build\obj

:: Clean up old .obj files
del /q build\obj\*.obj 2>nul
del /q build\main.exe 2>nul

echo.
echo ==============================
echo  Compiling core/*.c ...
echo ==============================

:: Recursively compile all .c files in the core folder
for /R core %%F in (*.c) do (
    rem skip "sim_time_test.c"
    echo %%~nxF | findstr /I "sim_time_test.c" >nul
    if errorlevel 1 (
        echo Compiling %%F
        cl ^
         /TC ^
         /W4 ^
         /Od ^
         /Zi ^
         /utf-8 ^
         /I"." ^
         /I"core" ^
         /c "%%F" ^
         /Fo"build\obj\%%~nF.obj" ^
         /Fd"build\elevator.pdb" >nul
        if errorlevel 1 (
            echo Compile failed on %%F
            exit /b 1
        )
    ) else (
        echo Skipping %%F
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
/I"core" ^
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
build\obj\*.obj

if errorlevel 1 (
    echo Link main.exe failed.
    exit /b 1
)
