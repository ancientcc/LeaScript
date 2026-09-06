/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include "../SDL_tray_utils.h"

SDL_Tray* SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    SDL_SetError("Platform does not support the tray.");
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    SDL_SetError("Platform does not support the tray.");
}

SDL_TrayMenu* SDL_CreateTrayMenu(SDL_Tray *tray)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

SDL_TrayMenu* SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

SDL_TrayMenu* SDL_GetTrayMenu(SDL_Tray *tray)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

SDL_TrayMenu* SDL_GetTraySubmenu(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

SDL_TrayEntry** SDL_GetTrayEntries(SDL_TrayMenu *menu, int *count)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

void SDL_RemoveTrayEntry(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
}

SDL_TrayEntry * SDL_InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

void SDL_SetTrayEntryLabel(SDL_TrayEntry *entry, const char *label)
{
    SDL_SetError("Platform does not support the tray.");
}

const char * SDL_GetTrayEntryLabel(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, SDL_bool checked)
{
    SDL_SetError("Platform does not support the tray.");
}

SDL_bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
    return SDL_FALSE;
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, SDL_bool enabled)
{
    SDL_SetError("Platform does not support the tray.");
}

SDL_bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
    return SDL_FALSE;
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    SDL_SetError("Platform does not support the tray.");
}

void SDL_ClickTrayEntry(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    SDL_SetError("Platform does not support the tray.");
}

SDL_TrayMenu * SDL_GetTrayEntryParent(SDL_TrayEntry *entry)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

SDL_TrayEntry * SDL_GetTrayMenuParentEntry(SDL_TrayMenu *menu)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

SDL_Tray * SDL_GetTrayMenuParentTray(SDL_TrayMenu *menu)
{
    SDL_SetError("Platform does not support the tray.");
    return NULL;
}

void SDL_UpdateTrays(void)
{
    SDL_SetError("Platform does not support the tray.");
}


void SDL_TrayInit(void)
{
}

void SDL_TrayQuit(void)
{
}