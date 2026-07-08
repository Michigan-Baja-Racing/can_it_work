@echo off
set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "DBC_FILE=%SCRIPT_DIR%\dbc\MBR_DBC.dbc"
set "SRC_DIR=%SCRIPT_DIR%\src"
set "INCLUDE_DIR=%SCRIPT_DIR%\include"

if not exist "%SRC_DIR%" mkdir "%SRC_DIR%"
if not exist "%INCLUDE_DIR%" mkdir "%INCLUDE_DIR%"

cantools generate_c_source "%DBC_FILE%" --database-name mbr_dbc -o "%SCRIPT_DIR%"

move /Y "%SCRIPT_DIR%\mbr_dbc.c" "%SRC_DIR%\mbr_dbc.c"
move /Y "%SCRIPT_DIR%\mbr_dbc.h" "%INCLUDE_DIR%\mbr_dbc.h"

echo Done.
