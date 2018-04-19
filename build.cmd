@echo off
setlocal
set MAKE=make.exe
set GCC=g++.exe
set MINGW=C:\MinGW\bin
where %GCC% 2>nul
IF %errorlevel% neq 0 (
  if not exist %MINGW%\%GCC% (
    echo Cygwin/g++ nor MinGW/g++ not found! Giving up!
    goto end
  )
  set MAKE=%MINGW%\mingw32-make.exe
  set PATH=%MINGW%
)
cd %~dp0
for /f "tokens=3" %%F in ('%MAKE% --version') do echo Using make %%F & goto n2
:n2
for /f "tokens=4" %%F in ('%GCC% --version') do echo Using gcc %%F & goto n1
:n1
%MAKE% %*
:end
endlocal
