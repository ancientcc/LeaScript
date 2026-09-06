/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_peripheral.h"
#include "SDL_log.h"
#include "SDL_timer.h"

#import <UIKit/UIKit.h>
#import <Photos/Photos.h>
// #import <ReplayKit/ReplayKit.h>


#ifndef posix_mku32
	#define posix_mku32(l, h)		((uint32_t)(((uint16_t)(l)) | ((uint32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mku16
	#define posix_mku16(l, h)		((uint16_t)(((uint8_t)(l)) | ((uint16_t)((uint8_t)(h))) << 8))
#endif

SDL_bool SDL_ReadCard(SDL_IDCardInfo* info)
{
	SDL_memset(info, 0, sizeof(SDL_IDCardInfo));
	info->type = SDL_CardIDCard;
	SDL_strlcpy(info->name, "ancientcc", sizeof(info->name));
	info->gender = SDL_GenderMale;
	info->folk = 1;
    SDL_strlcpy(info->birthday, "19791204", sizeof(info->birthday));
	SDL_strlcpy(info->number, "33992219791204401X", sizeof(info->number));

	return SDL_TRUE;
}

SDL_handle SDL_OpenSerialPort(const char* path, int baudrate)
{
    return SDL_INVALID_HANDLE_VALUE;
}

void SDL_CloseSerialPort(SDL_handle h)
{
}

size_t SDL_ReadSerialPort(SDL_handle h, void* ptr, size_t size)
{
    return 0;
}

size_t SDL_WriteSerialPort(SDL_handle h, const void* ptr, size_t size)
{
    return 0;
}

int SDL_GetTtyUSB(const char fix_dev_nodes[][48], int fix_count, SDL_ttyUSB** ppttyUSB)
{
	SDL_ttyUSB* ttyUSBs = NULL;

	int usb_count = 0;
	int total_count = fix_count + usb_count;
	if (usb_count == 0 && total_count != 0) {
		ttyUSBs = (SDL_ttyUSB*)SDL_malloc(sizeof(SDL_ttyUSB) * total_count);
		SDL_memset(ttyUSBs, 0, sizeof(SDL_ttyUSB) * total_count);
	}
	
	for (int n = 0; n < fix_count; n ++) {
		SDL_ttyUSB* tty = ttyUSBs + n;
        SDL_strlcpy(tty->dev_node, fix_dev_nodes[n], sizeof(tty->dev_node));
		SDL_strlcpy(tty->path, fix_dev_nodes[n], sizeof(tty->dev_node));
    }

	*ppttyUSB = ttyUSBs;
	return total_count;
}

SDL_bool SDL_SetTime(int64_t utctime)
{
    return SDL_FALSE;
}

void SDL_UpdateApp(const char* package)
{
}

// extern "C" {
    bool iOS_SavePNGToAlbum(const char* filePath) {
        __block bool success = false;
        __block NSString *errorMsg = nil;
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        
        @autoreleasepool {
            NSString *path = [NSString stringWithUTF8String:filePath];
            NSData *pngData = [NSData dataWithContentsOfFile:path];
            UIImage *image = [UIImage imageWithData:pngData];
            
            if (!image) {
                SDL_Log("%u {iOS_SavePNGToAlbum}fail, image == nullptr", SDL_GetTicks());
                return false;
            }
            
            // 检查权限状态
            PHAuthorizationStatus status = [PHPhotoLibrary authorizationStatus];
            if (status == PHAuthorizationStatusDenied || 
                status == PHAuthorizationStatusRestricted) {
                SDL_Log("%u {iOS_SavePNGToAlbum}fail, PHAuthorizationStatusDenied", SDL_GetTicks());
                return false;
            }
            
            // 标记是否需要请求权限
            __block bool isRequestingPermission = false;
            
            void (^saveBlock)(void) = ^{
                [[PHPhotoLibrary sharedPhotoLibrary] performChanges:^{
                    [PHAssetChangeRequest creationRequestForAssetFromImage:image];
                } completionHandler:^(BOOL result, NSError *error) {
                    success = result;
                    if (error) {
                        errorMsg = error.localizedDescription;
                    }
                    dispatch_semaphore_signal(semaphore);
                }];
            };
            
            if (status == PHAuthorizationStatusNotDetermined) {
                // 请求权限
                isRequestingPermission = true;
                SDL_Log("%u {iOS_SavePNGToAlbum}PHAuthorizationStatusNotDetermined, request...", SDL_GetTicks());
                [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus newStatus) {
                    if (newStatus == PHAuthorizationStatusAuthorized || 
                        newStatus == PHAuthorizationStatusLimited) {
                        saveBlock();
                    } else {
                        dispatch_semaphore_signal(semaphore);
                    }
                }];
            } else {
                saveBlock();
            }
            
            // Determine the waiting time based on whether permission is being requested.
            // (10 seconds or 5 seconds)
            long timeoutSeconds = isRequestingPermission ? 10 : 5;
            dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, timeoutSeconds * NSEC_PER_SEC);
            dispatch_semaphore_wait(semaphore, timeout);
        }
        
        return success;
    }
// }

SDL_bool SDL_NotifyMediaFileAdded(const char* filename)
{
    return iOS_SavePNGToAlbum(filename)? SDL_TRUE: SDL_FALSE;
}

// extern "C" {
    void iOS_OpenURL(const char* urlString) {
        @autoreleasepool {
            NSString *urlStr = [NSString stringWithUTF8String:urlString];
            NSURL *url = [NSURL URLWithString:urlStr];
            
            if (url && [[UIApplication sharedApplication] canOpenURL:url]) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
                });
            }
        }
    }
// }

void SDL_OpenUrl(const char* url)
{
    iOS_OpenURL(url);
}

int SDL_GetPublicDirectory(SDL_PublicDir type, char* name, int maxlen)
{
    name[0] = '\0';
    return 0;
}

//
// screen record
//
void SDL_StartScreenRecording(const char* filename)
{
}

// Stop iOS screen recording and save to the photo library.
SDL_bool SDL_StopScreenRecording(char* filename, int maxlen)
{
    filename[0] = '\0';
    return SDL_TRUE;
}

SDL_bool SDL_IsScreenRecording()
{
    return SDL_FALSE;
}

//
// os info
//
void SDL_GetOsInfo(SDL_OsInfo* info)
{
    SDL_strlcpy(info->manufacturer, "Apple", sizeof(info->manufacturer));
    SDL_strlcpy(info->model, "iOS", sizeof(info->model));
    SDL_snprintf(info->display, sizeof(info->display), "%s", "iOS");

    info->cpuid[0] = '\0';
    info->serialnumber[0] = '\0';
}

SDL_bool SDL_PrintText(const uint8_t* bytes, int size)
{
    return SDL_TRUE;
}

void SDL_EnableRelay(SDL_bool enable)
{
}

void SDL_TurnOnFlashlight(SDL_LightColor color)
{
}

void SDL_SetBrightness(int brightness)
{
}

void SDL_WifiSetEnable(SDL_bool enable)
{
}

uint32_t SDL_WifiGetFlags(void)
{
	return 0;
}

int SDL_WifiGetScanResults(SDL_WifiScanResult** results)
{
	*results = NULL;
	return 0;
}

SDL_bool SDL_WifiConnect(const char* ssid, const char* password)
{
	return SDL_TRUE;
}

SDL_bool SDL_WifiRemove(const char* ssid)
{
	return SDL_TRUE;
}

/* vi: set ts=4 sw=4 expandtab: */
