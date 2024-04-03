@echo off
setlocal EnableDelayedExpansion
set ARGC=0
for %%A in (%*) do set /a ARGC+=1
if %ARGC% neq 4 (
  echo usage: %~nx0 ^<stub-file^> ^<zip-file^> ^<zip-box-directory^> ^<target-file^>
  exit /b 2
)
cd %~dp0
set ARG2=%~f2
set ARG2=!ARG2:%CD%\=!
set ZIPFILE=!ARG2:\=/!
set ARG3=%~f3
set ARG3=!ARG3:%CD%\=!
set ZIPDIR=!ARG3:\=/!
set ARG4=%~f4
set ARG4=!ARG4:%CD%\=!
set TARGET=!ARG4:\=/!
echo Zipping %ZIPDIR% to %ZIPFILE%...
%~f1 zip make --target=%ZIPFILE% --directory=%ZIPDIR%
if %errorlevel% neq 0 exit /b %errorlevel%
echo Sandwiching %ZIPFILE% into %TARGET%...
%~f1 sandwich make --target=%TARGET% --zip=%ZIPFILE%
if %errorlevel% neq 0 exit /b %errorlevel%
echo Smoke testing %TARGET%...
%~f4 smoke-test
if %errorlevel% neq 0 exit /b %errorlevel%
endlocal
