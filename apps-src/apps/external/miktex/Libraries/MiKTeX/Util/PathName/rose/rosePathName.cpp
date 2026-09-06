/**
 * @file winPathName.cpp
 * @author Christian Schenk
 * @brief PathName class (Windows)
 *
 * @copyright Copyright © 1996-2024 Christian Schenk
 *
 * This file is part of the MiKTeX Util Library.
 *
 * The MiKTeX Util Library is licensed under GNU General Public License version
 * 2 or any later version.
 */

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <fmt/format.h>
#include <fmt/ostream.h>

#define A7C88F5FBE5C45EB970B3796F331CD89
#include "miktex/Util/config.h"

#if defined(MIKTEX_UTIL_SHARED)
#define MIKTEXUTILEXPORT MIKTEXDLLEXPORT
#else
#define MIKTEXUTILEXPORT
#endif

#include "miktex/Util/PathName.h"

#include "../../internal.h"

using namespace std;

using namespace MiKTeX::Util;

PathName& PathName::SetToCurrentDirectory()
{
#ifdef _WIN32
    wchar_t* buf = _wgetcwd(nullptr, MIKTEX_UTIL_PATHNAME_SIZE);
    if (buf == nullptr)
    {
        throw CRuntimeError("_wgetcwd");
    }
    *this = buf;
    free(buf);
#else
    while (getcwd(GetData(), GetCapacity()) == nullptr)
    {
        if (errno == ERANGE)
        {
            Reserve(GetCapacity() * 2);
        }
        else
        {
            throw CRuntimeError("getcwd");
        }
    }
#endif
    return *this;
}

#ifdef _WIN32
PathName& PathName::SetToTempDirectory()
{
    for (const string& env : vector<string>{ "TMP", "TEMP", "USERPROFILE" })
    {
        if (Helpers::GetEnvironmentString(env, *this) && this->IsAbsolute())
        {
            return *this;
        }
    }
    wchar_t szTemp[MIKTEX_UTIL_PATHNAME_SIZE];
    unsigned long n = GetWindowsDirectoryW(szTemp, static_cast<DWORD>(MIKTEX_UTIL_PATHNAME_SIZE));
    if (n == 0)
    {
        throw WindowsError("GetWindowsDirectoryW");
    }
    if (n >= GetCapacity())
    {
        throw Unexpected("buf too small");
    }
    *this = szTemp;
    return *this;
}

PathName& PathName::SetToTempFile(const PathName& directory)
{
    wchar_t szTemp[MAX_PATH];
    UINT n = GetTempFileNameW(directory.ToWideCharString().c_str(), L"mik", 0, szTemp);
    if (n == 0)
    {
        throw WindowsError("GetTempFileNameW");
    }
    *this = szTemp;
    return *this;
}

PathName PathName::GetMountPoint() const
{
    wchar_t szDir[MIKTEX_UTIL_PATHNAME_SIZE];
    if (!GetVolumePathNameW(this->ToWideCharString().c_str(), szDir, MIKTEX_UTIL_PATHNAME_SIZE))
    {
        throw WindowsError("GetVolumePathNameW");
    }
    return PathName(szDir);
}

PathName& PathName::AppendAltDirectoryDelimiter()
{
    size_t l = GetLength();
    if (l == 0 || !PathNameUtil::IsDirectoryDelimiter(CharBuffer::operator[](l - 1)))
    {
        CharBuffer::Append(PathNameUtil::AltDirectoryDelimiter);
    }
    return *this;
}
#else
PathName& PathName::SetToTempDirectory()
{
    if (Helpers::GetEnvironmentString("TMPDIR", *this) && this->IsAbsolute())
    {
        return *this;
    }
#if defined(P_tmpdir)
    *this = P_tmpdir;
#else
    * this = "/tmp";
#endif
    return *this;
}

PathName& PathName::SetToTempFile(const PathName& directory)
{
    *this = directory;
    AppendComponent("mikXXXXXX");
    int fd = mkstemp(GetData());
    if (fd < 0)
    {
        throw CRuntimeError("mkstemp");
    }
    close(fd);
    return *this;
}

PathName PathName::GetMountPoint() const
{
    throw Unexpected("not implemented");
}
#endif
