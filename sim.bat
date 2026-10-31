@echo off
rem  Build and run an Arduino sketch in the MAX7219 simulator (Windows).
rem    sim.bat C:\path\to\MySketch [simulator options]
rem  Needs MSYS2 with gcc, cmake, ninja and SDL2 (see README.md).
setlocal
if "%~1"=="" (
  echo usage: sim.bat SKETCH_FOLDER_OR_INO [options]
  echo        sim.bat examples\Demo
  echo        sim.bat C:\Users\me\Documents\Arduino\MyMatrix --modules 4x1 --rtl
  exit /b 1
)
@REM if exist "C:\msys64\ucrt64\bin\gcc.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if exist "D:\Program Files\msys64\ucrt64\bin\gcc.exe" set "PATH=D:\Program Files\msys64\ucrt64\bin;%PATH%"

set "HERE=%~dp0"
set "SKETCH=%~f1"
for %%I in ("%SKETCH%") do set "NAME=%%~nI"
set "BUILD=%HERE%build\%NAME%"

cmake -S "%HERE%." -B "%BUILD%" -G Ninja -DSKETCH="%SKETCH%" || goto :fail
cmake --build "%BUILD%" || goto :fail

shift
set "ARGS="
:collect
if "%~1"=="" goto run
set ARGS=%ARGS% %1
shift
goto collect
:run
for %%E in ("%BUILD%\*_sim.exe") do "%%~fE" %ARGS%
exit /b %errorlevel%

:fail
echo.
echo Build failed - see the messages above.
exit /b 1
