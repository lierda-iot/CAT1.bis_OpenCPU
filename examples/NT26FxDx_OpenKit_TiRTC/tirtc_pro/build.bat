@echo off
setlocal
if not "%~1"=="" goto usage
cd /d "%~dp0..\..\.."
if errorlevel 1 exit /b 1
if not defined TIRTC_PYTHON set "TIRTC_PYTHON=python"
"%TIRTC_PYTHON%" -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)" >nul 2>nul
if errorlevel 1 (
    echo Python 3.10 or newer is required for pro factory packaging.
    echo Install Python or set TIRTC_PYTHON to the full path of python.exe.
    exit /b 1
)
call "%~dp0build_app.bat"
if errorlevel 1 exit /b 1
"%TIRTC_PYTHON%" "%~dp0resource_store\package_factory.py"
exit /b %errorlevel%

:usage
echo Usage: build.bat
echo Use build_app.bat for optional make arguments.
exit /b 2
