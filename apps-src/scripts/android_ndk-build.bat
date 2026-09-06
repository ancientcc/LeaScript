@echo off
if '%1'=='' goto help
if '%1'=='help' goto help

@echo on

cd %lib3rdparty%
%NDK%/ndk-build
cd %scripts%
android_2_ndk.bat
cd %1
%NDK%/ndk-build

@echo off
goto exit

:help
echo Missing parameter, you must set app. for example: android_ndk-build %%studio%%

:exit