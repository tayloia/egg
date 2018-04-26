@echo off
setlocal
if "%1" == "" goto usage
if not "%2" == "" goto usage
set EXE=%1
set EXE=%EXE:/=\%
set LOG=%EXE%.log
%EXE% 1>%LOG% 2>&1
set RETVAL=%errorlevel%
if %RETVAL% == 0 goto success
type %LOG%
echo FAILURE: %EXE% returned %RETVAL%
exit /b %RETVAL%
:usage
echo usage: %~n0 test-executable
exit /b 1
:success
for /f "tokens=1,2,*" %%A in (%LOG%) do if "%%A" == "[==========]" if not "%%B" == "Running" echo SUCCESS: %%B %%C
:end
endlocal
