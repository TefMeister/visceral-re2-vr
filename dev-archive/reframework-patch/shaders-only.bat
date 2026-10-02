@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
echo PATHEXT=%PATHEXT%
set CompileShadersOutput=D:\ref-a24\build2\_deps\directxtk-build\Shaders\Compiled
if not exist "%CompileShadersOutput%" mkdir "%CompileShadersOutput%"
cd /d "D:\ref-a24\build2\_deps\directxtk-src\Src\Shaders"
echo CWD=%CD%
dir /b CompileShaders.cmd
call "%CD%\CompileShaders.cmd"
echo DXTK-SHADERS-EXIT %ERRORLEVEL%
dir /b "%CompileShadersOutput%" | find /c ".inc"
set CompileShadersOutput=D:\ref-a24\build2\_deps\directxtk12-build\Shaders\Compiled
if not exist "%CompileShadersOutput%" mkdir "%CompileShadersOutput%"
cd /d "D:\ref-a24\build2\_deps\directxtk12-src\Src\Shaders"
echo CWD=%CD%
call "%CD%\CompileShaders.cmd" dxil
echo DXTK12-SHADERS-EXIT %ERRORLEVEL%
dir /b "%CompileShadersOutput%" | find /c ".inc"
