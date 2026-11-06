@echo off
setlocal
cd /d "%~dp0..\..\.."
if errorlevel 1 exit /b 1
call build.bat build %* PROJECT=NT26FxDx_OpenKit APP_VARIANT=pro BUILD_MODE=tirtc TIRTC_ASSET_INSTALLER=y MODEM=NT26F6D0 MODEMPKG=F6D_D
exit /b %errorlevel%
