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
del /q build\test.exe 2>nul

echo.
echo ==============================
echo  Compiling core/*.c ...
echo ==============================

:: Recursively compile all .c files in the core folder
for /R core %%F in (*.c) do (
    rem skip "sim_time.c"
    echo %%~nxF | findstr /I "sim_time.c" >nul
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
echo  Compiling test.c ...
echo ==============================

cl ^
/TC ^
/W4 ^
/Od ^
/Zi ^
/utf-8 ^
/I"." ^
/I"core" ^
/c test.c ^
/Fo"build\obj\test.obj" ^
/Fd"build\test.pdb"
if errorlevel 1 (
    echo Compile failed on test.c
    exit /b 1
)

echo.
echo Linking test.exe ...

link /DEBUG /PDB:"build\test.pdb" ^
/OUT:"build\test.exe" ^
build\obj\*.obj

if errorlevel 1 (
    echo Link test.exe failed.
    exit /b 1
)

echo.
echo Build success.
echo   main: build\main.exe
echo   test: build\test.exe
echo.
