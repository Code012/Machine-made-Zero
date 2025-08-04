:: don't display all the lines as they are gettinf executed. @ symbol means that this line is included (so that your script won't print "echo off", either
@echo off

:: call command required to call a batch file from another batch file
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" x64
set path=D:\Coding\handmade-hero\misc;%path%
cls
