@echo off
setlocal
rem These are common installation locations (edit as required)
set MINGW=C:\MinGW\bin
set CYGWIN=C:\Cygwin64\bin
rem This value may need tweaking (currently 75% of processors are used)
set /a MAKE_JOBS=(%NUMBER_OF_PROCESSORS% * 3 + 3) / 4
set MAKE_OPTIONS=--jobs=%MAKE_JOBS%
rem Look for make and g++
cd %~dp0
set USING=Using
set MAKE_EXE=make.exe
set GCC_EXE=g++.exe
where %GCC_EXE% 2>nul
IF %errorlevel% neq 0 (
  if exist %MINGW%\%GCC_EXE% (
    set USING=Using MinGW
    if not exist %MINGW%\%MAKE_EXE% set MAKE_EXE=mingw32-make.exe
    set PATH=%MINGW%
  ) else if exist %CYGWIN%\%GCC_EXE% (
    set USING=Using Cygwin
    set MAKE_OPTIONS=%MAKE_OPTIONS%
    set PATH=%CYGWIN%
  ) else (
    echo %~nx0: Cygwin/MinGW not found: Giving up!
    exit /b 1
  )
)
rem Extract the version of make
set MAKE_VERSION=(unknown)
for /f "tokens=3" %%F in ('%MAKE_EXE% --version') do set MAKE_VERSION=%%F & goto n1
:n1
set MAKE_VERSION=%MAKE_VERSION: =%
echo %USING% make %MAKE_VERSION%
rem Extract the version of g++
set GCC_VERSION=(unknown)
for /f "tokens=2 delims=)" %%F in ('%GCC_EXE% --version') do set GCC_VERSION=%%F & goto n2
:n2
set GCC_VERSION=%GCC_VERSION: =%
echo %USING% gcc %GCC_VERSION%
rem Call make with the appropriate flags
if "%MAKE_VERSION%" geq "4.0" set MAKE_OPTIONS=%MAKE_OPTIONS% --output-sync=line
%MAKE_EXE% %MAKE_OPTIONS% %*
:end
endlocal
