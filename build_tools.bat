set Deps=third-party-deps

cd %Deps%\bgfx
..\bx\tools\bin\windows\genie.exe --with-tools vs2015

REM now build shaderc.exe


