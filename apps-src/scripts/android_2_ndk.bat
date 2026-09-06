@echo off
if not exist "%SDL_sdl%\libs\armeabi-v7a\libSDL2.so" goto ndk_64

@echo on

set DST=%SCRIPTS%\..\linker\android\lib\armeabi-v7a\.
copy %SDL_sdl%\libs\armeabi-v7a\libSDL2.so %SDL_sdl_so_path_32%\.
copy %SDL_sdl%\libs\armeabi-v7a\libhidapi.so %SDL_sdl_so_path_32%\.
copy %SDL_image%\libs\armeabi-v7a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\armeabi-v7a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi-v7a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\armeabi-v7a\libvpx.so %DST%
copy %lib3rdparty%\libs\armeabi-v7a\lib3rdparty.so %DST%

:ndk_64
if not exist "%SDL_sdl%\libs\arm64-v8a\libSDL2.so" goto exit

@echo on

set DST=%SCRIPTS%\..\linker\android\lib\arm64-v8a\.
copy %SDL_sdl%\libs\arm64-v8a\libSDL2.so %SDL_sdl_so_path_64%\.
copy %SDL_sdl%\libs\arm64-v8a\libhidapi.so %SDL_sdl_so_path_64%\.
copy %SDL_image%\libs\arm64-v8a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\arm64-v8a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\arm64-v8a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\arm64-v8a\libvpx.so %DST%
copy %lib3rdparty%\libs\arm64-v8a\lib3rdparty.so %DST%

@echo off

:exit

rem ABI_LEVEL: 21 hasn't arch-arm64