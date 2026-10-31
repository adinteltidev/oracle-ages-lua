@echo off
setlocal
if "%~1"=="" (
  echo Usage: Validar-Lua.cmd "C:\Jogos\Ages.gbc" [full]
  pause
  exit /b 2
)
if not defined APPDATA exit /b 2
set "ORACLES_LUA_REPORT=%APPDATA%\oracles-decomp\oracles-lua-phase5\validation"
if not exist "%ORACLES_LUA_REPORT%" mkdir "%ORACLES_LUA_REPORT%"
if not exist "%ORACLES_LUA_REPORT%" exit /b 2
set "ORACLES_LUA_FRAMES=20000"
if /i "%~2"=="full" set "ORACLES_LUA_FRAMES=289518"
"%~dp0Tests\test_lua_gameplay.exe" > "%ORACLES_LUA_REPORT%\lua-contract.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_block_rules.exe" > "%ORACLES_LUA_REPORT%\block-contract.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_colored_cube.exe" > "%ORACLES_LUA_REPORT%\colored-cube-contract.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_levers_lava.exe" > "%ORACLES_LUA_REPORT%\levers-lava-contract.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_levers_lava_rom.exe" --rom "%~f1" --tas-dir "%~dp0Tests" > "%ORACLES_LUA_REPORT%\levers-lava-scenes.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_doors.exe" > "%ORACLES_LUA_REPORT%\doors-contract.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_doors_rom.exe" --rom "%~f1" --tas-dir "%~dp0Tests" > "%ORACLES_LUA_REPORT%\doors-scenes.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_items_shops.exe" > "%ORACLES_LUA_REPORT%\items-shops-contract.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_lua_items_shops_rom.exe" --rom "%~f1" --tas-dir "%~dp0Tests" > "%ORACLES_LUA_REPORT%\items-shops-scenes.log" 2>&1
if errorlevel 1 goto failed
"%~dp0Tests\test_native_ages.exe" --rom "%~f1" --tas-dir "%~dp0Tests" --frames %ORACLES_LUA_FRAMES% --run both > "%ORACLES_LUA_REPORT%\replay.log" 2>&1
if errorlevel 1 goto failed
echo PASS. Logs: "%ORACLES_LUA_REPORT%"
pause
exit /b 0
:failed
set "ORACLES_LUA_EXIT=%ERRORLEVEL%"
echo Failed or skipped. Exit: %ORACLES_LUA_EXIT%. Logs: "%ORACLES_LUA_REPORT%"
pause
exit /b %ORACLES_LUA_EXIT%
