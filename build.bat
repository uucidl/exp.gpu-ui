@echo off

REM User configuration
if not defined BuildSlowDeps set BuildSlowDeps=1
if not defined ClCommonFlags set ClCommonFlags=/nologo /W3 /EHsc /Od /Z7
set Deps=third-party-deps
if not defined OutputDir set OutputDir=output\win32
if not defined IntermediateOutputDir set IntermediateOutputDir=%OutputDir%\Obj

if not defined CL_EXE set CL_EXE=cl.exe
if not defined BGFX_SHADERC_EXE set BGFX_SHADERC_EXE=%OutputDir%\shaderc.exe
if not defined BX_BIN2C set BX_BIN2C=%Deps%\bx\tools\bin\windows\bin2c.exe

REM # Create output directories:
REM #
if not exist %OutputDir% md %OutputDir%
if %errorlevel% neq 0 exit /b 1
if not exist %IntermediateOutputDir% md %IntermediateOutputDir%
if %errorlevel% neq 0 exit /b 1

REM Needed by renderdoc (copy the renderdoc.dll inside the working dir)
if not exist %OutputDir%\temp md %OutputDir%\temp

REM # Test prerequisites can be called
REM #
%CL_EXE% 1>NUL 2>NUL
if %errorlevel% neq 0 (
	echo %CL_EXE% not found
	exit /b 1
)
if EXIST %BGFX_SHADERC_EXE% (%BGFX_SHADERC_EXE% -v) ELSE (
	echo "%BGFX_SHADERC_EXE% not found, build it by hand (see bgfx documentation)"
	exit /b 1
)
if NOT EXIST %BX_BIN2C% (
	echo "%BX_BIN2C% not found"
	exit /b 1
)

REM # Build dependencies
REM #

if %BuildSlowDeps% neq 0 %CL_EXE% /Fo%IntermediateOutputDir%\bx.obj /c ^
  unit_bx.cpp ^
  /D_CRT_SECURE_NO_WARNINGS ^
  /D__STDC_FORMAT_MACROS ^
  /I%Deps%\bx\include\compat\msvc ^
  /I%Deps%\bx\include ^
  %ClCommonFlags%

if %BuildSlowDeps% neq 0 %CL_EXE% /Fo%IntermediateOutputDir%\bimg.obj /c ^
  unit_bimg.cpp ^
  /I%Deps%\bimg\include ^
  /I%Deps%\bx\include\compat\msvc ^
  /I%Deps%\bx\include ^
  %ClCommonFlags%

if %BuildSlowDeps% neq 0 %CL_EXE% /Fo%IntermediateOutputDir%\bgfx.obj /c ^
  unit_bgfx.cpp ^
  /D_CRT_SECURE_NO_WARNINGS ^
  /DBGFX_SHARED_LIB_BUILD ^
  /DBGFX_CONFIG_DEBUG_PIX ^
  /I%Deps%\bgfx\include ^
  /I%Deps%\bgfx\3rdparty ^
  /I%Deps%\bgfx\3rdparty\khronos ^
  /I%Deps%\bgfx\3rdparty\dxsdk\include ^
  /I%Deps%\bimg\include ^
  /I%Deps%\bx\include\compat\msvc ^
  /I%Deps%\bx\include ^
  %ClCommonFlags%

REM we use an import library (/imblib) to allow
REM ui modules to import symbols from the host

%CL_EXE% /c /Fo%IntermediateOutputDir%\ui_api.obj ^
  unit_ui_api.c ^
  %ClCommonFlags%

REM # Build the host
%CL_EXE% /Fe%OutputDir%\test.exe ^
  win32_unit_host.cpp ^
  %IntermediateOutputDir%\bgfx.obj ^
  %IntermediateOutputDir%\bx.obj ^
  %IntermediateOutputDir%\bimg.obj ^
  /D_CRT_SECURE_NO_WARNINGS ^
  /Fo%IntermediateOutputDir%\ ^
  /I%Deps%\bgfx\3rdparty\dxsdk\include ^
  /I%Deps%\bgfx\include ^
  /I%Deps%\bx\include ^
  %ClCommonFlags% ^
  /Fm:%OutputDir%\win32_test_host.map ^
  /link /SUBSYSTEM:WINDOWS ^
  /implib:%OutputDir%\win32_test_host.lib


REM # Build reloadable UI module
REM # ui

REM # ui: shaders
REM #

REM # UI BOX
set VaryingDefFile=ui_box_varying.def.sc
set ShaderType=fragment
set ShaderName=ui_box_fs
set ShaderFileName=ui_box.fs
call :build_shader_c %ShaderType% %ShaderName% %ShaderFileName% %VaryingDefFile%

set ShaderType=vertex
set ShaderName=ui_box_vs
set ShaderFileName=ui_box.vs
call :build_shader_c %ShaderType% %ShaderName% %ShaderFileName% %VaryingDefFile%

REM # UI BUTTON
set VaryingDefFile=ui_button_varying.def.sc
set ShaderType=fragment
set ShaderName=ui_button_fs
set ShaderFileName=ui_button.fs
call :build_shader_c %ShaderType% %ShaderName% %ShaderFileName% %VaryingDefFile%

set ShaderType=vertex
set ShaderName=ui_button_vs
set ShaderFileName=ui_button.vs
call :build_shader_c %ShaderType% %ShaderName% %ShaderFileName% %VaryingDefFile%

REM # UI CHARS
set VaryingDefFile=ui_chars_varying.def.sc
set ShaderType=fragment
set ShaderName=ui_chars_fs
set ShaderFileName=ui_chars.fs
call :build_shader_c %ShaderType% %ShaderName% %ShaderFileName% %VaryingDefFile%

set ShaderType=vertex
set ShaderName=ui_chars_vs
set ShaderFileName=ui_chars.vs
call :build_shader_c %ShaderType% %ShaderName% %ShaderFileName% %VaryingDefFile%

REM # ui: DLL module
REM #
set DllPrefixPath=%OutputDir%\ui
set DllPdbPath=%DllPrefixPath%_dll_%random%.pdb
echo --- WAITING FOR PDB > %DllPrefixPath%_dll.lock
del %DllPrefixPath%_dll_*.pdb
%CL_EXE% /Fe%DllPrefixPath%.dll unit_ui.cpp ^
   %OutputDir%\win32_test_host.lib ^
   /I%IntermediateOutputDir% ^
   /D_CRT_SECURE_NO_WARNINGS ^
   %CLCommonFlags% ^
   /I%Deps%\bgfx\include ^
   /I%Deps%\bx\include\compat\msvc ^
   /I%Deps%\bx\include ^
   /Fo%IntermediateOutputDir%\ ^
   /Fm:%DllPrefixPath%.map ^
   /LD ^
   /link /PDB:%DllPdbPath% ^
   /EXPORT:ui_get_vtable_0



@if errorlevel 1 exit /b 1
@REM only remove lock file on success, to indicate we have a working dll
@attrib +R %DllPdbPath%
@del %DllPrefixPath%_dll_*.pdb > NUL 2> NUL
@attrib -R %DllPdbPath%
@del %DllPrefixPath%_dll.lock

if %errorlevel% neq 0 exit /b 1

exit /b 0

REM procedure to create a shader C file for inclusion
:build_shader_c
set ShaderType=%1
set ShaderName=%2
set ShaderFileName=%3
set VaryingDefFile=%4

echo %ShaderFileName%
%BGFX_SHADERC_EXE% -o %IntermediateOutputDir%\%ShaderFileName%.bin ^
 --type %ShaderType% -f %ShaderFileName% ^
 --varyingdef %VaryingDefFile% ^
 -i %Deps%
%BX_BIN2C% -o %IntermediateOutputDir%\%ShaderFileName%.c ^
  -n %ShaderName%_bin ^
  -f %IntermediateOutputDir%\%ShaderFileName%.bin
exit /b 0
