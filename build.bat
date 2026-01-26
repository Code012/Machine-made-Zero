@echo off
if not exist build mkdir build
:: this remembers the original directory it switched from, so that popd can return to it once we've done.
pushd build
set code_path=../code/
:: GENERAL COMPILER FLAGS
set compiler=               -nologo &:: Suppress Startup Banner
set compiler=%compiler%     -Oi     &:: Use assembly intrinsics where possible
set compiler=%compiler%     -MT     &:: Include CRT library in the executable (static link), Don't rely on the user having the correct CRT dll version
set compiler=%compiler%     -Gm-    &:: Disable minimal rebuild
set compiler=%compiler%     -GR-    &:: Disable runtime type info (C++)
set compiler=%compiler%     -EHa-   &:: Disable exception handling (C++)
set compiler=%compiler% 	-W4	    &:: So windows warnings go away
set compiler=%compiler% 	-WX	    &:: Treat all warnings as errors
:: IGNORE WARNINGS
set compiler=%compiler%     -wd4201 &:: Nameless struct/union
set compiler=%compiler%     -wd4100 &:: Unused function parameter
set compiler=%compiler%     -wd4189 &:: Local variable not referenced
set compiler=%compiler%     -wd4701 &:: Potentially uninitialized local variable 'name' used
set compiler=%compiler%		
:: DEBUG VARIABLES
set debug=		  			-FC &:: Produce the full path of the source code file
set debug=%debug% 			-Zi &:: Produce debug information (seperate PDB file, Use Z7 for embedding debug info into .obj file)
:: COMMON LINKER SWITCHES
set link=					-opt:ref				&:: Remove unused functions
set link=%win32_link% 		-incremental:no			&:: Perform full link each time
:: DLL LINKER SWITCHES
set dll_link=				/EXPORT:GameUpdateAndRender
set dll_link=%dll_link%		/EXPORT:GameGetSoundSamples
:: WIN32 PLATFORM LIBRARIES
set win32_libs=				user32.lib
set win32_libs=%win32_libs% Gdi32.lib
set win32_libs=%win32_libs% Winmm.lib
set win32_lins=%win32_libs% Xinput.lib 
:: CROSS_PLATFORM DEFINES
set defines=	      		-DHANDMADE_INTERNAL=1
set defines=%defines% 		-DHANDMADE_SLOW=1

:: No optimisations (slow)L -Od; all optimisations (fast): -O2
cl -Od %compiler% %defines% %debug% -Fmhandmade.map %code_path%handmade.cpp -LD /link %link% %dll_link%															&:: Cross-platform game code (handmade.dll)
cl -Od %compiler% -DHANDMADE_WIN32=1 %defines% %debug% -Fmwin32_handmade.map %code_path%win32_handmade.cpp %win32_libs% /link %link% -subsystem:windows,5.2 	&:: Windows platform code (win32_handmade.exe)
popd
