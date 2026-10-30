@echo off
setlocal
if "%~1"=="" (
  echo Usage: Validar-Lua.cmd "C:\path\Ages.gbc" [full]
  echo Without full: short replay. With full: all 4825 checkpoints.
  pause
  exit /b 2
)
if not defined APPDATA exit /b 2
set "ORACLES_LUA_REPORT=%APPDATA%\oracles-decomp\oracles-lua-phase1\validation"
if not exist "%ORACLES_LUA_REPORT%" mkdir "%ORACLES_LUA_REPORT%"
if not exist "%ORACLES_LUA_REPORT%" exit /b 2
set "ORACLES_LUA_FRAMES=20000"
if /i "%~2"=="full" set "ORACLES_LUA_FRAMES=289518"
echo Running Lua contract tests...
"%~dp0Tests\test_lua_gameplay.exe" > "%ORACLES_LUA_REPORT%\lua-contract.log" 2>&1
if errorlevel 1 goto failed
echo Running expanded block rule contracts...
"%~dp0Tests\test_lua_block_rules.exe" > "%ORACLES_LUA_REPORT%\block-rules-contract.log" 2>&1
if errorlevel 1 goto failed
echo Running directed puzzle scenarios and frame comparisons...
"%~dp0Tests\test_lua_block_rules_rom.exe" --rom "%~f1" --tas-dir "%~dp0Tests" > "%ORACLES_LUA_REPORT%\block-rules-scenes.log" 2>&1
if errorlevel 1 goto failed
echo Running normal and reload replays. This may take several minutes.
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
