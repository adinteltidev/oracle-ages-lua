@echo off
setlocal
if not defined APPDATA exit /b 2
set "ORACLES_LUA_PROFILE=%APPDATA%\oracles-decomp\oracles-lua-phase7"
if not exist "%ORACLES_LUA_PROFILE%" mkdir "%ORACLES_LUA_PROFILE%"
"%~dp0Oracles-Lua-Fase7.exe" --cache "%ORACLES_LUA_PROFILE%" > "%ORACLES_LUA_PROFILE%\runtime.log" 2>&1
if errorlevel 1 pause
