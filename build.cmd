@echo off
rem Compila WRAM en modo MinSizeRel y ejecuta las pruebas del nucleo.
rem Requiere Visual Studio 2022 o Build Tools con la carga de trabajo de C++.
setlocal
set "ROOT=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
  echo No se encontro Visual Studio con herramientas de C++.
  exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake -S "%ROOT%." -B "%ROOT%build" -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel || exit /b 1
cmake --build "%ROOT%build" || exit /b 1
"%ROOT%build\wram_tests.exe" --junit "%ROOT%build\junit-core.xml" || exit /b 1
for %%F in ("%ROOT%build\WRAM.exe") do echo WRAM.exe: %%~zF bytes
endlocal
