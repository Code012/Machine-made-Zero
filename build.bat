@echo off
if not exist build mkdir build
:: this remembers the original directory it switched from, so that popd can return to it once we've done.
pushd build 
:: important to specify drive, or it can't find file
cl -Zi D:/Coding/handmade-hero/code/win32_handmade.cpp user32.lib


popd
