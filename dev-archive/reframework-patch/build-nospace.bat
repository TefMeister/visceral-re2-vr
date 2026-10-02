@echo off
rem DirectXTK's CompileShaders.cmd cannot cope with a space in the path, so build through a junction without one.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist D:\ref-a24 mklink /J D:\ref-a24 "D:\RE2 REFramework builds\tools\REFramework-a24c3459"
cd /d D:\ref-a24
if not exist build2 mkdir build2
cd build2
cmake .. -G "Visual Studio 17 2022" -A x64 -DDEVELOPER_MODE=ON
echo CONFIGURE-EXIT %ERRORLEVEL%
cmake --build . --config Release --target REFramework
echo BUILD-EXIT %ERRORLEVEL%
