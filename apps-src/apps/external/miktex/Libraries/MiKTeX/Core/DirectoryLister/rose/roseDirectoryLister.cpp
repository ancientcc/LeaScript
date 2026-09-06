/**
 * @file roseDirectoryLister.cpp
 * @author Christian Schenk
 * @brief Directory lister implementation (Windows)
 *
 * @copyright Copyright © 1996-2023 Christian Schenk
 *
 * This file is part of the MiKTeX Core Library.
 *
 * The MiKTeX Core Library is licensed under GNU General Public License version
 * 2 or any later version.
 */

#include "../../miktex_Core_config.h"

// #include <Windows.h>
// #include <VersionHelpers.h>

#include <miktex/Core/DirectoryLister>
// #include <miktex/Core/win/WindowsVersion>

#include "../../miktex_Core_internal.h"

#include "roseDirectoryLister.h"

using namespace std;

using namespace MiKTeX::Core;
using namespace MiKTeX::Util;

unique_ptr<DirectoryLister> DirectoryLister::Open(const PathName& directory)
{
    return make_unique<roseDirectoryLister>(directory, nullptr, (int)Options::None);
}

unique_ptr<DirectoryLister> DirectoryLister::Open(const PathName& directory, const char* lpszPattern)
{
    return make_unique<roseDirectoryLister>(directory, lpszPattern, (int)Options::None);
}

unique_ptr<DirectoryLister> DirectoryLister::Open(const PathName& directory, const char* lpszPattern, int options)
{
    return make_unique<roseDirectoryLister>(directory, lpszPattern, options);
}

roseDirectoryLister::roseDirectoryLister(const PathName& directory, const char* lpszPattern, int options)
    : directory(directory)
    , pattern(lpszPattern == nullptr ? "" : lpszPattern)
    , options(options)
    , dir_(nullptr)
{
    dir_ = SDL_OpenDir(directory.ToString().c_str());
	if (dir_ == nullptr) {
		int ii = 0;
	}
}

roseDirectoryLister::~roseDirectoryLister()
{
    Close();
}

void roseDirectoryLister::Close()
{
    if (dir_ != nullptr) {
        SDL_CloseDir(dir_);
        dir_ = nullptr;
    }
}

bool roseDirectoryLister::GetNext(DirectoryEntry& direntry)
{
    DirectoryEntry2 direntry2;
    if (!GetNext(direntry2))
    {
        return false;
    }
    direntry.name = direntry2.name;
    // direntry.wname1 = direntry2.wname1;
    direntry.isDirectory = direntry2.isDirectory;
    return true;
}

inline bool IsDotDirectory(const char* entry)
{
  if (entry[0] != '.')
  {
    return false;
  }
  if (entry[1] == 0)
  {
    return true;
  }
  return entry[1] == '.' && entry[2] == 0;
}

bool roseDirectoryLister::GetNext(DirectoryEntry2& direntry2)
{
    if (dir_ == nullptr) {
        return false;
    }

    SDL_dirent2* dirent = nullptr;
    
    bool isDirectory = false;
    do {
        dirent = SDL_ReadDir(dir_);
        if (dirent == nullptr) {
            return false;
        }

        if (SDL_DIRENT_DIR(dirent->mode)) {
            isDirectory = true;
        } else {
            isDirectory = false;
        }
    } while ((IsDotDirectory(dirent->name) && ((options & (int)Options::IncludeDotAndDotDot) == 0))
           || (!pattern.empty() && !PathName::Match(pattern, PathName(dirent->name)))
           || ((options & (int)Options::DirectoriesOnly) != 0 && !isDirectory)
           || ((options & (int)Options::FilesOnly) != 0 && isDirectory));

    direntry2.name = dirent->name;
    direntry2.isDirectory = isDirectory;
    direntry2.size = dirent->size;
    return true;
}
