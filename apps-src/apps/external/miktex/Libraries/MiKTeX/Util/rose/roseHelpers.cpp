/**
 * @file winHelpers.cpp
 * @author Christian Schenk
 * @brief Helpers class (Windows)
 *
 * @copyright Copyright © 2021-2024 Christian Schenk
 *
 * This file is part of the MiKTeX Util Library.
 *
 * The MiKTeX Util Library is licensed under GNU General Public License version
 * 2 or any later version.
 */

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

#define A7C88F5FBE5C45EB970B3796F331CD89
#include "miktex/Util/config.h"

#include "../internal.h"
#include "rose_filesystem_dll.hpp"
#include "rose_exception.hpp"

using namespace std;

using namespace MiKTeX::Util;

BEGIN_INTERNAL_NAMESPACE;

#ifdef _WIN32
void Helpers::CanonicalizePathName(PathName& path)
{
    CharBuffer<wchar_t, MIKTEX_UTIL_PATHNAME_SIZE> fullPath;
    bool done = false;
    int rounds = 0;
    do
    {
        DWORD n = GetFullPathNameW(path.ToWideCharString().c_str(), fullPath.GetCapacity(), fullPath.GetData(), nullptr);
        if (n == 0)
        {
            throw WindowsError("GetFullPathNameW");
        }
        done = n < fullPath.GetCapacity();
        if (!done)
        {
            if (rounds > 0)
            {
                throw Unexpected("buf too small");
            }
            fullPath.Reserve(n);
        }
        rounds++;
    } while (!done);
    path = fullPath.GetData();
}

bool Helpers::GetEnvironmentString(const string& name, string& value)
{
    wchar_t* lpszValue = _wgetenv(StringUtil::UTF8ToWideChar(name).c_str());
    if (lpszValue == nullptr)
    {
        return false;
    }
    else
    {
        value = StringUtil::WideCharToUTF8(lpszValue);
        return true;
    }
}

static unsigned long GetFileAttributes_harmlessErrors[] = {
  ERROR_FILE_NOT_FOUND, // 2
  ERROR_PATH_NOT_FOUND, // 3
  ERROR_ACCESS_DENIED, // 5
  ERROR_NOT_READY, // 21
  ERROR_BAD_NETPATH, // 53
  ERROR_BAD_NET_NAME, // 67
  ERROR_INVALID_NAME, // 123
  ERROR_BAD_PATHNAME, // 161
};

bool miktex_DirectoryExists(const PathName& path)
{
    unsigned long attributes = GetFileAttributesW(path.ToExtendedLengthPathName().ToWideCharString().c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return false;
        }
        return true;
    }
    unsigned long error = ::GetLastError();
    // TODO: range-based for loop
    for (int idx = 0; idx < sizeof(GetFileAttributes_harmlessErrors) / sizeof(GetFileAttributes_harmlessErrors[0]); ++idx)
    {
        if (error == GetFileAttributes_harmlessErrors[idx])
        {
            error = ERROR_SUCCESS;
            break;
        }
    }
    if (error != ERROR_SUCCESS)
    {
        throw WindowsError("GetFileAttributesW");
    }
    return false;
}

#else
void Helpers::CanonicalizePathName(PathName& path)
{
    char* resolved = realpath(path.GetData(), nullptr);
    if (resolved == nullptr)
    {
        if (errno == ENOENT)
        {
            return;
        }
        throw CRuntimeError("realpath");
    }
    path = resolved;
    free(resolved);
}

bool Helpers::GetEnvironmentString(const string& name, string& value)
{
    const char* path = getenv(name.c_str());
    if (path == nullptr)
    {
        return false;
    }
    else
    {
        value = path;
        return true;
    }
}
#endif

bool Helpers::DirectoryExists(const PathName& path)
{
    bool existed = SDL_IsDirectory(path.ToString().c_str());

#ifdef _WIN32
    bool existed2 = miktex_DirectoryExists(path);
    VALIDATE(existed == existed2, null_str);
#endif

    return existed;
}

END_INTERNAL_NAMESPACE;
