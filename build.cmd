@echo off
setlocal
rem These are common installation locations (edit as required)
set WSL32=%SystemRoot%\SYSNATIVE\wsl.exe
set WSL64=%SystemRoot%\System32\wsl.exe
set MINGW32=C:\MinGW\bin
set MINGW64=C:\MinGW64\bin
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
if %errorlevel% neq 0 (
  if exist %WSL64% (
    set USING=Using WSL64
    set MAKE_EXE=%WSL64% make
    set MAKE_OPTIONS=%MAKE_OPTIONS% PLATFORM=wsl
    set GCC_EXE=%WSL64% g++
  ) else if exist %WSL32% (
    set USING=Using WSL32
    set MAKE_EXE=%WSL32% make
    set MAKE_OPTIONS=%MAKE_OPTIONS% PLATFORM=wsl
    set GCC_EXE=%WSL32% g++
  ) else if exist %MINGW64%\%GCC_EXE% (
    set USING=Using MinGW64
    set MAKE_OPTIONS=%MAKE_OPTIONS% PLATFORM=mingw
    if not exist %MINGW64%\%MAKE_EXE% set MAKE_EXE=mingw32-make.exe
    set PATH=%MINGW64%
  ) else if exist %MINGW32%\%GCC_EXE% (
    set USING=Using MinGW32
    set MAKE_OPTIONS=%MAKE_OPTIONS% PLATFORM=mingw
    if not exist %MINGW32%\%MAKE_EXE% set MAKE_EXE=mingw32-make.exe
    set PATH=%MINGW32%
  ) else if exist %CYGWIN%\%GCC_EXE% (
    set USING=Using Cygwin
    set MAKE_OPTIONS=%MAKE_OPTIONS% PLATFORM=cygwin
    set PATH=%CYGWIN%
  ) else (
    echo %~nx0: GCC/make not found: Giving up!
    exit /b 1
  )
)
:n0
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
rem if "%MAKE_VERSION%" geq "4.0" set MAKE_OPTIONS=%MAKE_OPTIONS% --output-sync=line
%MAKE_EXE% %MAKE_OPTIONS% %*
:end
endlocal
