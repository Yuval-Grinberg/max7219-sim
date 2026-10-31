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
rem Find MSYS2's compiler (add your own install folder here if it lives somewhere else).
for %%D in ("D:\Program Files\msys64" "C:\msys64" "C:\Program Files\msys64" "D:\msys64") do (
  if exist "%%~D\ucrt64\bin\gcc.exe" (
    set "PATH=%%~D\ucrt64\bin;%PATH%"
    goto :found_msys
  )
)
:found_msys

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
