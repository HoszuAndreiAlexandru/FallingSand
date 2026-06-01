@echo off
setlocal

REM --- Paths ---
set CONFIG=Debug
set SRC_DIR=Source
set BUILD_DIR=build
set BIN_DIR=%BUILD_DIR%\bin\%CONFIG%

REM --- Step 1: Configure CMake ---
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -T v143 -A x64

REM --- Step 2: Build project ---
cmake --build %BUILD_DIR% --config Debug

REM --- Step 3: Run executable ---
cd %BIN_DIR%
FallingSand.exe
pause

endlocal