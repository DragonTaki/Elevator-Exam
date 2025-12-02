@echo off
cd /d %~dp0
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist build mkdir build

cl ^
 /TC ^
 /W4 ^
 /Od ^
 /Zi ^
 /utf-8 ^
 interactive_client.c ^
 /Fe:build\interactive_client.exe ^
 /Fd:build\interactive_client.pdb
