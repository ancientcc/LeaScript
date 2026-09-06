/* winDirectory.cpp:

   Copyright (C) 1996-2021 Christian Schenk

   This file is part of the MiKTeX Core Library.

   The MiKTeX Core Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   The MiKTeX Core Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the MiKTeX Core Library; if not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA. */

#include "../../miktex_Core_config.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <miktex/Core/Directory>
// #include <miktex/Core/win/winAutoResource>
#include <miktex/Trace/Trace>
#include <miktex/Trace/TraceStream>

#include "../../miktex_Core_internal.h"

#include <rose_filesystem.hpp>
#include <rose_exception.hpp>

using namespace std;

using namespace MiKTeX::Core;
using namespace MiKTeX::Trace;
using namespace MiKTeX::Util;

PathName Directory::GetCurrent()
{
  PathName cd;
  cd.SetToCurrentDirectory();
  return cd;
}

#ifdef _WIN32
void Directory::SetCurrent(const PathName& path)
{
  if (_wchdir(UW_(path.GetData())) != 0)
  {
    MIKTEX_FATAL_CRT_ERROR_2("_wchdir", "path", path.ToString());
  }
}
#else
#include <unistd.h>
#include <sys/stat.h>
void Directory::SetCurrent(const PathName& path)
{
  if (chdir(path.GetData()) != 0)
  {
    MIKTEX_FATAL_CRT_ERROR_2("chdir", "path", path.ToString());
  }
}
#endif
/*
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

static bool miktex_Exists(const PathName& path)
{
  auto trace_access = TraceStream::Open(MIKTEX_TRACE_ACCESS);
  unsigned long attributes = GetFileAttributesW(path.ToExtendedLengthPathName().ToWideCharString().c_str());
  if (attributes != INVALID_FILE_ATTRIBUTES)
  {
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
      trace_access->WriteLine("core", fmt::format(T_("{0} is not a directory"), Q_(path)));
      return false;
    }
    trace_access->WriteLine("core", fmt::format(T_("accessing directory {0}: OK"), Q_(path)));
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
    MIKTEX_FATAL_WINDOWS_ERROR_3("GetFileAttributesW",
      T_("MiKTeX cannot retrieve attributes for the directory '{path}'."),
      "path", path.ToDisplayString());
  }
  trace_access->WriteLine("core", fmt::format(T_("accessing directory {0}: NOK"), Q_(path)));
  return false;
}
*/
bool Directory::Exists(const PathName& path)
{
  // bool ret = miktex_Exists(path);
  const std::string str = path.ToString();
  bool exists = SDL_IsDirectory(str.c_str());
  // VALIDATE(ret == exists, null_str);
  return exists;
}

void Directory::Delete(const PathName& path)
{
    SDL_DeleteFiles(path.ToString().c_str());
}

void Directory::SetTimes(const PathName& path, time_t creationTime, time_t lastAccessTime, time_t lastWriteTime)
{
    File::SetTimes(path, creationTime, lastAccessTime, lastWriteTime);
}

void Directory::Move(const PathName& source, const PathName& dest)
{
  File::Move(source, dest);
}
