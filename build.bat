@echo off
rem Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
rem wechat:angaio/13707128443
rem ---------------------------------------------------------------------------
rem Build the IrIsp application against the prebuilt libraries in bin\.
rem Edit the two paths below to match your machine, then run:  build.bat
rem ---------------------------------------------------------------------------
setlocal
set QTDIR=C:\Qt\5.15.2\msvc2019_64
set OPENCV_DIR=C:\opencv\build
rem ---------------------------------------------------------------------------
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
set NINJA=%QTDIR%\..\..\Tools\Ninja\ninja.exe
call "%VCVARS%" >nul 2>&1
cd /d "%~dp0"
cmake -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QTDIR%" -DOpenCV_DIR="%OPENCV_DIR%"
if errorlevel 1 (echo CONFIGURE FAILED & exit /b 1)
cmake --build build --config Release
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%QTDIR%\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler ^
  --no-opengl-sw --compiler-runtime bin\IrProcessDemo.exe >nul 2>&1
echo.
echo BUILD OK  -^>  bin\IrProcessDemo.exe
endlocal
