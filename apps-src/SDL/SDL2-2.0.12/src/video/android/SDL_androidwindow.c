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

#if SDL_VIDEO_DRIVER_ANDROID

#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"
#include "../../core/android/SDL_android.h"

#include "SDL_androidvideo.h"
#include "SDL_androidwindow.h"
#include "SDL_hints.h"
#include "SDL_timer.h"
#include "SDL_log.h"

/* Currently only one window */
SDL_Window *Android_Window = NULL;

void Android_SetFullscreen(_THIS, SDL_bool fullscreen)
{
    SDL_Log("Android_SetFullscreen--- surface(%i x %i), fullscreen: %s", Android_SurfaceWidth, Android_SurfaceHeight, fullscreen? "true": "false");
    if (Android_FullScreenAlways) {
        SDL_Log("---Android_SetFullscreen X, Android_FullScreenAlways is true, do nothing, fullscreen: %s, surface(%i x %i)", fullscreen? "true": "false", Android_SurfaceWidth, Android_SurfaceHeight);
        return;
    }
    waiting_SDL_WINDOWEVENT_RESIZED = SDL_TRUE;
    Android_JNI_SetWindowStyle(fullscreen);

    // now StatusBar is immersed always, so if no NavigationBar(for example phone), set fullscreen will cann't change surfaceSize.
    // On those occasions, do not use waiting_SDL_WINDOWEVENT_RESIZED mechanism.
    SDL_Log("Android_SetFullscreen, 1, waiting_SDL_WINDOWEVENT_RESIZED: %s", waiting_SDL_WINDOWEVENT_RESIZED? "true": "false");
    int remaining_wait_ms = 10;
    // in normal, while spend 4-5 ms.
    while (waiting_SDL_WINDOWEVENT_RESIZED && remaining_wait_ms >= 0) {
        SDL_Delay(3);
        remaining_wait_ms -= 3;
    }

    SDL_Log("---Android_SetFullscreen X, fullscreen: %s, surface(%i x %i)", fullscreen? "true": "false", Android_SurfaceWidth, Android_SurfaceHeight);
}
/*
void Android_SetOrientation(_THIS, SDL_bool landscape)
{
    SDL_Log("Android_SetOrientation--- surface(%i x %i), landscape: %s", Android_SurfaceWidth, Android_SurfaceHeight, landscape? "true": "false");

    if (Android_SurfaceWidth == 0 || Android_SurfaceHeight == 0) {
        return;
    }
    if (Android_SurfaceWidth == Android_SurfaceHeight) {
        return;
    }

    // because statue_bar, width and height are not flip-equal after and before change.
    if (landscape) {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeRight,LandscapeLeft");
        if (Android_SurfaceWidth > Android_SurfaceHeight) {
            return;
        }

    } else {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
        if (Android_SurfaceWidth < Android_SurfaceHeight) {
            return;
        }
    }

    // require rotate, flip width and height.
    int width = Android_SurfaceHeight;
    int height = Android_SurfaceWidth;

    waiting_SDL_WINDOWEVENT_RESIZED = SDL_TRUE;
    Android_JNI_SetOrientation(width, height, SDL_WINDOW_RESIZABLE, SDL_GetHint(SDL_HINT_ORIENTATIONS));

    SDL_Log("Android_SetOrientation, 1, waiting_SDL_WINDOWEVENT_RESIZED: %s", waiting_SDL_WINDOWEVENT_RESIZED? "true": "false");
    while (waiting_SDL_WINDOWEVENT_RESIZED) {
        SDL_Delay(10);
    }

    SDL_Log("---Android_SetOrientation X landscape: %s, surface(%i x %i)", landscape? "true": "false", Android_SurfaceWidth, Android_SurfaceHeight);

    if (landscape && Android_SurfaceWidth < Android_SurfaceHeight) {
        
    }
}
*/

void Android_SetOrientation(_THIS, SDL_bool landscape)
{
    SDL_Log("Android_SetOrientation--- surface(%i x %i), landscape: %s", Android_SurfaceWidth, Android_SurfaceHeight, landscape? "true": "false");

    if (Android_SurfaceWidth == 0 || Android_SurfaceHeight == 0) {
        return;
    }
    if (Android_SurfaceWidth == Android_SurfaceHeight) {
        return;
    }

    // because statue_bar, width and height are not flip-equal after and before change.
    if (landscape) {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeRight,LandscapeLeft");
        if (Android_SurfaceWidth > Android_SurfaceHeight) {
            return;
        }

    } else {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
        if (Android_SurfaceWidth < Android_SurfaceHeight) {
            return;
        }
    }

    // require rotate, flip width and height.
    int width = Android_SurfaceHeight;
    int height = Android_SurfaceWidth;
    if (landscape) {
        // If orientation needs to be changed, it usually occurs in portrait-mode apps that occasionally need landscape mode. 
        // Issues typically arise when switching from portrait to landscape, such as on a Honor 70c.
        int times = 0;
        while (Android_SurfaceWidth < Android_SurfaceHeight) {
            waiting_SDL_WINDOWEVENT_RESIZED = SDL_TRUE;
            Android_JNI_SetOrientation(width, height, SDL_WINDOW_RESIZABLE, SDL_GetHint(SDL_HINT_ORIENTATIONS));

            SDL_Log("time#%i, Android_SetOrientation, 1, waiting_SDL_WINDOWEVENT_RESIZED: %s", times, waiting_SDL_WINDOWEVENT_RESIZED? "true": "false");
            while (waiting_SDL_WINDOWEVENT_RESIZED) {
                SDL_Delay(10);
            }

            if (Android_SurfaceWidth < Android_SurfaceHeight) {
                SDL_Log("time#%i, require landscape, but, Android_SurfaceWidth(%i) < Android_SurfaceHeight(%i), rotage again", 
                    times, Android_SurfaceWidth, Android_SurfaceHeight);
            }
            times ++;
        }
    } else {
        waiting_SDL_WINDOWEVENT_RESIZED = SDL_TRUE;
        Android_JNI_SetOrientation(width, height, SDL_WINDOW_RESIZABLE, SDL_GetHint(SDL_HINT_ORIENTATIONS));

        SDL_Log("Android_SetOrientation, 1, waiting_SDL_WINDOWEVENT_RESIZED: %s", waiting_SDL_WINDOWEVENT_RESIZED? "true": "false");
        while (waiting_SDL_WINDOWEVENT_RESIZED) {
            SDL_Delay(10);
        }
    }
    SDL_Log("---Android_SetOrientation X landscape: %s, surface(%i x %i)", landscape? "true": "false", Android_SurfaceWidth, Android_SurfaceHeight);
}

int
Android_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *data;
    int retval = 0;

    Android_ActivityMutex_Lock_Running();

    if (Android_Window) {
        retval = SDL_SetError("Android only supports one window");
        goto endfunction;
    }

	SDL_Log("Android_CreateWindow--- window(%i, %i, %i, %i), Surface(%i x %i)", window->x, window->y, window->w, window->h, Android_SurfaceWidth, Android_SurfaceHeight);

    /* Set orientation */
    Android_JNI_SetOrientation(window->w, window->h, window->flags & SDL_WINDOW_RESIZABLE, SDL_GetHint(SDL_HINT_ORIENTATIONS));

    /* Adjust the window data to match the screen */
    window->x = 0;
    window->y = 0;
    window->w = Android_SurfaceWidth;
    window->h = Android_SurfaceHeight;

    window->flags &= ~SDL_WINDOW_HIDDEN;
    window->flags |= SDL_WINDOW_SHOWN;          /* only one window on Android */

    /* One window, it always has focus */
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);

    data = (SDL_WindowData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        retval = SDL_OutOfMemory();
        goto endfunction;
    }

    SDL_Log("Android_CreateWindow, post Android_JNI_SetOrientation, window(%i, %i, %i, %i), Surface(%i x %i)", window->x, window->y, window->w, window->h, Android_SurfaceWidth, Android_SurfaceHeight);
    data->native_window = Android_JNI_GetNativeWindow();

    if (!data->native_window) {
        SDL_free(data);
        retval = SDL_SetError("Could not fetch native window");
        goto endfunction;
    }

    /* Do not create EGLSurface for Vulkan window since it will then make the window
       incompatible with vkCreateAndroidSurfaceKHR */
    if ((window->flags & SDL_WINDOW_OPENGL) != 0) {
        data->egl_surface = SDL_EGL_CreateSurface(_this, (NativeWindowType) data->native_window);

        if (data->egl_surface == EGL_NO_SURFACE) {
            ANativeWindow_release(data->native_window);
            SDL_free(data);
            retval = -1;
            goto endfunction;
        }
    }

    window->driverdata = data;
    Android_Window = window;

    SDL_Log("---Android_CreateWindow, X, window->flags: 0x%08x", Android_Window->flags);
endfunction:

    SDL_UnlockMutex(Android_ActivityMutex);

    return retval;
}

void
Android_SetWindowTitle(_THIS, SDL_Window *window)
{
    Android_JNI_SetActivityTitle(window->title);
}

void
Android_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    SDL_LockMutex(Android_ActivityMutex);

    if (window == Android_Window) {

        /* If the window is being destroyed don't change visible state */
        if (!window->is_destroying) {
            Android_JNI_SetWindowStyle(fullscreen);
        }

        /* Ensure our size matches reality after we've executed the window style change.
         *
         * It is possible that we've set width and height to the full-size display, but on
         * Samsung DeX or Chromebooks or other windowed Android environemtns, our window may
         * still not be the full display size.
         */
        if (!SDL_IsDeXMode() && !SDL_IsChromebook()) {
            goto endfunction;
        }

        SDL_WindowData *data = (SDL_WindowData *)window->driverdata;

        if (!data || !data->native_window) {
            if (data && !data->native_window) {
                SDL_SetError("Missing native window");
            }
            goto endfunction;
        }

        int old_w = window->w;
        int old_h = window->h;

        int new_w = ANativeWindow_getWidth(data->native_window);
        int new_h = ANativeWindow_getHeight(data->native_window);

        if (new_w < 0 || new_h < 0) {
            SDL_SetError("ANativeWindow_getWidth/Height() fails");
        }

        if (old_w != new_w || old_h != new_h) {
            SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, new_w, new_h);
        }
    }

endfunction:

    SDL_UnlockMutex(Android_ActivityMutex);
}

void
Android_MinimizeWindow(_THIS, SDL_Window *window)
{
    Android_JNI_MinizeWindow();
}

void
Android_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_LockMutex(Android_ActivityMutex);

    if (window == Android_Window) {
        Android_Window = NULL;

        if (window->driverdata) {
            SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
            if (data->egl_surface != EGL_NO_SURFACE) {
                SDL_EGL_DestroySurface(_this, data->egl_surface);
            }
            if (data->native_window) {
                ANativeWindow_release(data->native_window);
            }
            SDL_free(window->driverdata);
            window->driverdata = NULL;
        }
    }

    SDL_UnlockMutex(Android_ActivityMutex);
}

SDL_bool
Android_GetWindowWMInfo(_THIS, SDL_Window *window, SDL_SysWMinfo *info)
{
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;

    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        info->subsystem = SDL_SYSWM_ANDROID;
        info->info.android.window = data->native_window;
        info->info.android.surface = data->egl_surface;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

#endif /* SDL_VIDEO_DRIVER_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
