@echo off
setlocal
if not defined APPDATA (
  echo APPDATA is not defined.
  pause
  exit /b 2
)
set "ORACLES_LUA_DATA=%APPDATA%\oracles-decomp\oracles-lua-phase1"
if not exist "%ORACLES_LUA_DATA%" mkdir "%ORACLES_LUA_DATA%"
if not exist "%ORACLES_LUA_DATA%" exit /b 2
"%~dp0Oracles-Lua-Fase1.exe" --cache "%ORACLES_LUA_DATA%" > "%ORACLES_LUA_DATA%\runtime.log" 2>&1
set "ORACLES_LUA_EXIT=%ERRORLEVEL%"
if not "%ORACLES_LUA_EXIT%"=="0" (
  echo Application error. See "%ORACLES_LUA_DATA%\runtime.log"
  pause
)
exit /b %ORACLES_LUA_EXIT%
