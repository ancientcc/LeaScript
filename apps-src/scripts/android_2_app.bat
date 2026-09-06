@echo off
if '%1'=='' goto help
if '%1'=='help' goto help

if not exist "%SDL_sdl%\libs\armeabi-v7a\libSDL2.so" goto copy_from_linker_32

@echo on

set DST=%1\..\..\..\..\linker\android\lib\armeabi-v7a\.
copy %SDL_sdl%\libs\armeabi-v7a\libSDL2.so %SDL_sdl_so_path_32%\.
copy %SDL_sdl%\libs\armeabi-v7a\libhidapi.so %SDL_sdl_so_path_32%\.
copy %SDL_image%\libs\armeabi-v7a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\armeabi-v7a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi-v7a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\armeabi-v7a\libvpx.so %DST%
copy %lib3rdparty%\libs\armeabi-v7a\lib3rdparty.so %DST%



:copy_from_linker_32

@echo on

set SRC=%1\..\..\..\..\linker\android\lib\armeabi-v7a
set DST=%1\libs\armeabi-v7a\.
copy %SDL_sdl_so_path_32%\libSDL2.so %DST%
copy %SDL_sdl_so_path_32%\libhidapi.so %DST%
copy %SRC%\libSDL2_image.so %DST%
copy %SRC%\libSDL2_mixer.so %DST%
copy %SRC%\libSDL2_ttf.so %DST%
copy %SRC%\libvpx.so %DST%
copy %SRC%\lib3rdparty.so %DST%

if not exist "%SDL_sdl%\libs\arm64-v8a\libSDL2.so" goto copy_from_linker_64

@echo on

set DST=%1\..\..\..\..\linker\android\lib\arm64-v8a\.
copy %SDL_sdl%\libs\arm64-v8a\libSDL2.so %SDL_sdl_so_path_64%\.
copy %SDL_sdl%\libs\arm64-v8a\libhidapi.so %SDL_sdl_so_path_64%\.
copy %SDL_image%\libs\arm64-v8a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\arm64-v8a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\arm64-v8a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\arm64-v8a\libvpx.so %DST%
copy %lib3rdparty%\libs\arm64-v8a\lib3rdparty.so %DST%



:copy_from_linker_64

@echo on

set SRC=%1\..\..\..\..\linker\android\lib\arm64-v8a
set DST=%1\libs\arm64-v8a\.
copy %SDL_sdl_so_path_64%\libSDL2.so %DST%
copy %SDL_sdl_so_path_64%\libhidapi.so %DST%
copy %SRC%\libSDL2_image.so %DST%
copy %SRC%\libSDL2_mixer.so %DST%
copy %SRC%\libSDL2_ttf.so %DST%
copy %SRC%\libvpx.so %DST%
copy %SRC%\lib3rdparty.so %DST%

@echo off
goto exit

:help
echo Missing parameter, you must set app. for example: android_2_app %%studio%%

:exit