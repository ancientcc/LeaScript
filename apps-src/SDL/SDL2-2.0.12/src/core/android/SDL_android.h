/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"
#include "SDL_system.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#include <EGL/eglplatform.h>
#include <android/native_window_jni.h>

#include "SDL_audio.h"
#include "SDL_rect.h"
#include "SDL_video.h"
#include "SDL_filesystem.h"
#include "SDL_peripheral.h"

/* Interface from the SDL library into the Android Java activity */
extern void Android_JNI_SetActivityTitle(const char *title);
extern void Android_JNI_SetWindowStyle(SDL_bool fullscreen);
extern void Android_JNI_SetOrientation(int w, int h, int resizable, const char *hint);
extern void Android_JNI_MinizeWindow(void);
extern SDL_bool Android_JNI_ShouldMinimizeOnFocusLoss(void);

extern SDL_bool Android_JNI_GetAccelerometerValues(float values[3]);
extern void Android_JNI_ShowTextInput(SDL_Rect *inputRect);
extern void Android_JNI_HideTextInput(void);
extern SDL_bool Android_JNI_IsScreenKeyboardShown(void);
extern SDL_bool Android_JNI_CanScreenKeyboardShown(void);
extern ANativeWindow* Android_JNI_GetNativeWindow(void);
extern void Android_JNI_SetSurfaceViewFormat(int format);

extern SDL_DisplayOrientation Android_JNI_GetDisplayOrientation(void);
extern int Android_JNI_GetDisplayDPI(float *ddpi, float *xdpi, float *ydpi);

/* Audio support */
extern int Android_JNI_OpenAudioDevice(int iscapture, SDL_AudioSpec *spec);
extern void* Android_JNI_GetAudioBuffer(void);
extern void Android_JNI_WriteAudioBuffer(void);
extern int Android_JNI_CaptureAudioBuffer(void *buffer, int buflen);
extern void Android_JNI_FlushCapturedAudio(void);
extern void Android_JNI_CloseAudioDevice(const int iscapture);
extern void Android_JNI_AudioSetThreadPriority(int iscapture, int device_id);

/* Detecting device type */
extern SDL_bool Android_IsDeXMode(void);
extern SDL_bool Android_IsChromebook(void);

#include "SDL_rwops.h"

int Android_JNI_FileOpen(SDL_RWops* ctx, const char* fileName, const char* mode);
Sint64 Android_JNI_FileSize(SDL_RWops* ctx);
Sint64 Android_JNI_FileSeek(SDL_RWops* ctx, Sint64 offset, int whence);
size_t Android_JNI_FileRead(SDL_RWops* ctx, void* buffer, size_t size, size_t maxnum);
size_t Android_JNI_FileWrite(SDL_RWops* ctx, const void* buffer, size_t size, size_t num);
int Android_JNI_FileClose(SDL_RWops* ctx);

/* Environment support */
void Android_JNI_GetManifestEnvironmentVariables(void);

/* Clipboard support */
int Android_JNI_SetClipboardText(const char* text);
char* Android_JNI_GetClipboardText(void);
SDL_bool Android_JNI_HasClipboardText(void);

/* Power support */
int Android_JNI_GetPowerInfo(int* plugged, int* charged, int* battery, int* seconds, int* percent);

/* Joystick support */
void Android_JNI_PollInputDevices(void);

/* Haptic support */
void Android_JNI_PollHapticDevices(void);
void Android_JNI_HapticRun(int device_id, float intensity, int length);
void Android_JNI_HapticStop(int device_id);

/* Video */
void Android_JNI_SuspendScreenSaver(SDL_bool suspend);

/* Touch support */
void Android_JNI_InitTouch(void);

/* Threads */
#include <jni.h>
JNIEnv *Android_JNI_GetEnv(void);
int Android_JNI_SetupThread(void);

/* Generic messages */
int Android_JNI_SendMessage(int command, int param);

/* Init */
JNIEXPORT void JNICALL SDL_Android_Init(JNIEnv* mEnv, jclass cls);

/* MessageBox */
#include "SDL_messagebox.h"
int Android_JNI_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid);

/* Cursor support */
int Android_JNI_CreateCustomCursor(SDL_Surface *surface, int hot_x, int hot_y);
SDL_bool Android_JNI_SetCustomCursor(int cursorID);
SDL_bool Android_JNI_SetSystemCursor(int cursorID);

/* Relative mouse support */
SDL_bool Android_JNI_SupportsRelativeMouse(void);
SDL_bool Android_JNI_SetRelativeMouseEnabled(SDL_bool enabled);

/* Request permission */
SDL_bool Android_JNI_RequestPermission(const char *permission);

int SDL_GetAndroidSDKVersion(void);

SDL_bool SDL_IsAndroidTablet(void);
SDL_bool SDL_IsAndroidTV(void);
SDL_bool SDL_IsChromebook(void);
SDL_bool SDL_IsDeXMode(void);

void Android_ActivityMutex_Lock(void);
void Android_ActivityMutex_Unlock(void);
void Android_ActivityMutex_Lock_Running(void);

jclass Android_JNI_GetActivityClass(void);
JavaVM* Android_JNI_GetJavaVM(void);
jobject Android_JNI_GetActivityObject(void);

/* file system support */
SDL_bool Android_JNI_IsDirectory(const char* dir);
SDL_bool Android_JNI_IsFile(const char* file);
int64_t Android_JNI_getLength64(char const* file);

SDL_DIR* Android_JNI_OpenDir(char const* dir);
SDL_dirent2* Android_JNI_ReadDir(SDL_DIR* dir);
int Android_JNI_CloseDir(SDL_DIR* dir);
SDL_bool Android_JNI_GetStat(const char* name, SDL_dirent* stat);

SDL_Photo* Android_JNI_OpenPhoto(char const* dir);
void* Android_JNI_ReadPhoto(SDL_Photo* photos, const int index, const int width, const int height);
int Android_JNI_ClosePhoto(SDL_Photo* photos);

SDL_bool Android_JNI_ReadCard(SDL_IDCardInfo* info);
void Android_JNI_SetProperty(const char* key, const char* value);
void Android_JNI_Reboot(Uint32 ms);
void Android_JNI_Shutdown(Uint32 ms);
void Android_JNI_SetAppType(int type);
SDL_bool Android_JNI_SetTime(int64_t utctime);
void Android_JNI_UpdateApp(const char* package);
void Android_JNI_NotifyMediaFileAdded(const char* filename);
void Android_JNI_OpenUrl(const char* url);
int Android_GetPublicDirectory(SDL_PublicDir type, char* name, int maxlen);
void Android_StartScreenRecording(const char* filename);
void Android_StopScreenRecording(char* filename, int maxlen);
SDL_bool Android_IsScreenRecording(void);
void Android_JNI_GetOsInfo(SDL_OsInfo* info);
SDL_bool Android_JNI_PrintText(const uint8_t* bytes, int size);
void Android_EnableRelay(SDL_bool enable);
void Android_TurnOnFlashlight(SDL_LightColor color);
void Android_TurnOnScreen(SDL_bool on);
void Android_SetBrightness(int brightness);
SDL_bool Android_JNI_OrbbecCreateContext(void);
void Android_JNI_OrbbecDestroyContext(void);
char* Android_JNI_CreateGUID(SDL_bool line);
void Android_JNI_WifiSetEnable(SDL_bool enable);
uint32_t Android_JNI_WifiGetFlags(void);
int Android_JNI_WifiGetScanResults(SDL_WifiScanResult** results);
SDL_bool Android_JNI_WifiConnect(const char* ssid, const char* password);
SDL_bool Android_JNI_WifiRemove(const char* ssid);

SDL_bool Android_JNI_BleInitialize(void);
void Android_JNI_BleScanPeripherals(const char* uuid);
void Android_JNI_BleStopScanPeripherals(void);
void Android_JNI_BleConnectPeripheral(const char* address);
void Android_JNI_BleDisconnectPeripheral(const char* address);
void Android_JNI_BleDiscoverServices(const char* address);
void Android_JNI_BleClearCache(const char* address);
void Android_JNI_BleNotifyCharacteristic(const char* address, const char* service, const char* characteristic, SDL_bool enable);
void Android_JNI_BleWriteCharacteristic(const char* address, const char* service, const char* characteristic, const unsigned char* data, int len);
void Android_JNI_BleReadCharacteristic(const char* address, const char* service, const char* characteristic);

void Android_JNI_PBleStartAdvertising(const char* service_uuid16, const char* name, int manufacturer_id, const uint8_t* manufacturer_data, int size);
void Android_JNI_PBleStopAdvertising(void);
SDL_bool Android_JNI_PBleIsAdvertising(void);
void Android_JNI_PBleWriteCharacteristic(const char* uuid, const uint8_t* data, int size);
void Android_JNI_PBleCancelConnection(void);

double Android_GetDpiScale(void);
int Android_GetStatusBarHeight(void);
int Android_GetNavigationHeight(void);
void Android_EnableStatusBar(SDL_bool enable);
void Android_SetStatusBarStyle(SDL_bool white_font);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

/* vi: set ts=4 sw=4 expandtab: */
