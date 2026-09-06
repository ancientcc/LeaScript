/* File.cpp: file operations

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

#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#include <miktex/Core/win/winAutoResource>
#else
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#endif

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <thread>

#include <miktex/Core/Directory>
#include <miktex/Core/File>
#include <miktex/Core/FileStream>
#include <miktex/Core/Fndb>
#include <miktex/Trace/Trace>
#include <miktex/Trace/TraceStream>

#include "../../miktex_Core_internal.h"

#include "../../Session/SessionImpl.h"

#include <rose_config_3rdparty.hpp>
#include <rose_filesystem_dll.hpp>
#include <rose_exception.hpp>
#include <SDL_log.h>
#include <SDL_timer.h>

int tuenc_verbose = 0;

#ifdef __cplusplus
extern "C" {
#endif

int get_tuenc_verbose() { return tuenc_verbose; }

#ifdef __cplusplus
}
#endif

using namespace std;

using namespace MiKTeX::Core;
using namespace MiKTeX::Trace;
using namespace MiKTeX::Util;
/*
static std::unordered_map<DWORD, bool> expectedErrors = {
  { ERROR_FILE_NOT_FOUND, false }, // 2
  { ERROR_PATH_NOT_FOUND, false }, // 3
  { ERROR_ACCESS_DENIED, true }, // 5
  { ERROR_NOT_READY, false, }, // 21
  { ERROR_BAD_NETPATH, false }, // 53
  { ERROR_BAD_NET_NAME, false }, // 67
  { ERROR_INVALID_NAME, false }, // 123
  { ERROR_BAD_PATHNAME, false }, // 161
  { ERROR_CANT_ACCESS_FILE, false }, // 1920
};

static bool miktex_Exists(const PathName& path, FileExistsOptionSet options)
{
  auto trace_access = TraceStream::Open(MIKTEX_TRACE_FILES);
  if (options[FileExistsOption::SymbolicLink])
  {
    UNIMPLEMENTED();
  }

  PathName extPath = path.ToExtendedLengthPathName();
  if (extPath == PathName("\\\\.\\nul"))
  {
    return true;
  }
  unsigned long attributes = GetFileAttributesW(extPath.ToWideCharString().c_str());
  bool exists = attributes != INVALID_FILE_ATTRIBUTES;
  if (exists)
  {
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
      exists = false;
      trace_access->WriteLine("core", fmt::format(T_("{0} is a directory"), Q_(path)));
    }
  }
  else
  {
    DWORD error = ::GetLastError();
    auto it = expectedErrors.find(error);
    if (it != expectedErrors.end())
    {
      exists = it->second;
    }
    else
    {
      MIKTEX_FATAL_WINDOWS_RESULT_3("GetFileAttributesW", error, T_("MiKTeX cannot retrieve attributes for the file '{path}'."), "path", path.ToDisplayString());
    }
  }
  trace_access->WriteLine("core", fmt::format(T_("accessing file {0}: {1}"), Q_(path), exists ? "OK" : "NOK"));
  return exists;
}
*/
bool File::Exists(const PathName& path, FileExistsOptionSet options)
{
  if (options[FileExistsOption::SymbolicLink])
  {
    UNIMPLEMENTED();
  }

  // bool ret = miktex_Exists(path, options);
  const std::string str = path.ToString();
  bool exists = SDL_IsFile(str.c_str());
  if (game_config::os == os_windows) {
      if (!exists && (str == ".\\nul:" || str == "./nul:")) {
          exists = true;
      }
  }
  // VALIDATE(ret == exists, null_str);
  return exists;
}

#ifdef _WIN32
FileAttributeSet File::GetAttributes(const PathName& path)
{
  unsigned long attributes = GetNativeAttributes(path);

  FileAttributeSet result;

  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
  {
    result += FileAttribute::Directory;
  }

  if ((attributes & FILE_ATTRIBUTE_READONLY) != 0)
  {
    result += FileAttribute::ReadOnly;
  }

  if ((attributes & FILE_ATTRIBUTE_HIDDEN) != 0)
  {
    result += FileAttribute::Hidden;
  }

  return result;
}
#else
FileAttributeSet File::GetAttributes(const PathName& path)
{
  mode_t attributes = static_cast<mode_t>(GetNativeAttributes(path));

  FileAttributeSet result;

  if (S_ISDIR(attributes) != 0)
  {
    result += FileAttribute::Directory;
  }

  if (((attributes & S_IWUSR) == 0)
    && ((attributes & S_IWGRP) == 0)
    && ((attributes & S_IWOTH) == 0))
  {
    result += FileAttribute::ReadOnly;
  }

  if (((attributes & S_IXUSR) != 0)
    || ((attributes & S_IXGRP) != 0)
    || ((attributes & S_IXOTH) != 0))
  {
    result += FileAttribute::Executable;
  }
  
  return result;
}
#endif

unsigned long File::GetNativeAttributes(const PathName& path)
{
    return SDL_GetAttributes(path.ToString().c_str());
}

#ifdef _WIN32
void File::SetAttributes(const PathName& path, FileAttributeSet attributes)
{
  unsigned long attributesOld = GetNativeAttributes(path);

  unsigned long attributesNew = attributesOld;

  if (attributes[FileAttribute::ReadOnly])
  {
    attributesNew |= FILE_ATTRIBUTE_READONLY;
  }
  else
  {
    attributesNew &= ~FILE_ATTRIBUTE_READONLY;
  }

  if (attributes[FileAttribute::Hidden])
  {
    attributesNew |= FILE_ATTRIBUTE_HIDDEN;
  }
  else
  {
    attributesNew &= ~FILE_ATTRIBUTE_HIDDEN;
  }

  if (attributesNew == attributesOld)
  {
    return;
  }

  SetNativeAttributes(path, attributesNew);
}
#else
mode_t GetFileCreationMask()
{
#if 0
  // not atomic
  mode_t cmask = umask(0);
  umask(cmask);
  return cmask;
#else
  return 022;
#endif
}

void File::SetAttributes(const PathName& path, FileAttributeSet attributes)
{
  mode_t newAttributes = S_IRUSR | S_IRGRP | S_IROTH;
  if (!attributes[FileAttribute::ReadOnly])
  {
    newAttributes |= S_IWUSR | S_IWGRP | S_IWOTH;
  }
  if (attributes[FileAttribute::Executable])
  {
    newAttributes |= S_IXUSR | S_IXGRP | S_IXOTH;
  }
  newAttributes &= ~GetFileCreationMask();
  if (newAttributes != static_cast<unsigned long>(GetNativeAttributes(path)))
  {
    SetNativeAttributes(path, static_cast<unsigned long>(newAttributes));
  }
}
#endif

void File::SetNativeAttributes(const PathName& path, unsigned long nativeAttributes)
{
    SDL_SetAttributes(path.ToString().c_str(), nativeAttributes);
}
/*
size_t miktex_GetSize(const PathName& path)
{
  HANDLE h = CreateFileW(path.ToExtendedLengthPathName().ToWideCharString().c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (h == INVALID_HANDLE_VALUE)
  {
    MIKTEX_FATAL_WINDOWS_ERROR_2("CreateFileW", "path", path.ToString());
  }

  AutoHANDLE autoClose(h);

  // TODO: large file support
  unsigned long fileSize = GetFileSize(h, nullptr);

  if (fileSize == INVALID_FILE_SIZE)
  {
    MIKTEX_FATAL_WINDOWS_ERROR_2("GetFileSize", "path", path.ToString());
  }

  return fileSize;
}
*/
size_t File::GetSize(const PathName& path)
{
    // size_t fsize1 = miktex_GetSize(path);
    size_t fsize2 = file_size(path.ToString().c_str());
    // VALIDATE(fsize1 == fsize2, null_str);

    return fsize2;
}

#ifdef _WIN32
// number of 100 nanosecond units from 1/1/1601 to 1/1/1970
const LONGLONG EPOCH_BIAS = 116444736000000000;


// 1601-01-01 00:00:00 as Unx time
const LONGLONG MIN_TIME_T = -11644473600;

// 30827-12-31 23:59:59 as Unx time
const LONGLONG MAX_TIME_T = 910670515199;

MIKTEXSTATICFUNC(FILETIME) UniversalCrtTimeToFileTime(time_t time)
{
  FILETIME fileTime;
  if (time == static_cast<time_t>(-1)
    || time < MIN_TIME_T
    || time > MAX_TIME_T)

  {
    fileTime.dwLowDateTime = 0;
    fileTime.dwHighDateTime = 0;
  }
  else
  {
    LONGLONG ll = static_cast<LONGLONG>(time) * 10000000 + EPOCH_BIAS;
    fileTime.dwLowDateTime = static_cast<DWORD>(ll);
    fileTime.dwHighDateTime = ll >> 32;
  }
  return fileTime;
}

MIKTEXSTATICFUNC(time_t) FileTimeToUniversalCrtTime(FILETIME fileTime)
{
  if (fileTime.dwLowDateTime == 0 && fileTime.dwHighDateTime == 0)
  {
    return static_cast<time_t>(-1);
  }
  ULARGE_INTEGER uli;
  uli.LowPart = fileTime.dwLowDateTime;
  uli.HighPart = fileTime.dwHighDateTime;
  return (uli.QuadPart / 10000000) - (EPOCH_BIAS / 10000000);
}

MIKTEXINTERNALFUNC(void) SetTimesInternal(HANDLE handle, time_t creationTime, time_t lastAccessTime, time_t lastWriteTime)
{
  FILETIME creationFileTime;
  FILETIME lastAccessFileTime;
  FILETIME lastWriteFileTime;
  if (creationTime != static_cast<time_t>(-1))
  {
    creationFileTime = UniversalCrtTimeToFileTime(creationTime);
  }
  if (lastAccessTime != static_cast<time_t>(-1))
  {
    lastAccessFileTime = UniversalCrtTimeToFileTime(lastAccessTime);
  }
  if (lastWriteTime != static_cast<time_t>(-1))
  {
    lastWriteFileTime = UniversalCrtTimeToFileTime(lastWriteTime);
  }
  if (!SetFileTime(handle,
    creationTime != static_cast<time_t>(-1) ? &creationFileTime : nullptr,
    lastAccessTime != static_cast<time_t>(-1) ? &lastAccessFileTime : nullptr,
    lastWriteTime != static_cast<time_t>(-1) ? &lastWriteFileTime : nullptr))
  {
    MIKTEX_FATAL_WINDOWS_ERROR("SetFileTime");
  }
}

#define GET_OSFHANDLE(hf) \
  reinterpret_cast<HANDLE>(_get_osfhandle(static_cast<int>(hf)))

void File::SetTimes(int fd, time_t creationTime, time_t lastAccessTime, time_t lastWriteTime)
{
  SetTimesInternal(GET_OSFHANDLE(fd), creationTime, lastAccessTime, lastWriteTime);
}

void File::SetTimes(FILE* stream, time_t creationTime, time_t lastAccessTime, time_t lastWriteTime)
{
  SetTimes(_fileno(stream), creationTime, lastAccessTime, lastWriteTime);
}
#else
void File::SetTimes(int fd, time_t creationTime, time_t lastAccessTime, time_t lastWriteTime)
{
  UNUSED_ALWAYS(creationTime);
  time_t now = time(nullptr);
  if (lastAccessTime == static_cast<time_t>(-1))
  {
    lastAccessTime = now;
  }
  if (lastWriteTime == static_cast<time_t>(-1))
  {
    lastWriteTime = now;
  }
#if defined(HAVE_FUTIMES)
  timeval times[2] = {
    { lastAccessTime, 0 },
    { lastWriteTime, 0 }
  };
  if (futimes(fd, times) < 0)
  {
    MIKTEX_FATAL_CRT_ERROR("futimes");
  }
#else
  UNUSED_ALWAYS(fd);
  UNIMPLEMENTED();
#endif
}

void File::SetTimes(FILE* stream, time_t creationTime, time_t lastAccessTime, time_t lastWriteTime)
{
  int fd = fileno(stream);
  if (fd < 0)
  {
    MIKTEX_FATAL_CRT_ERROR("fileno");
  }    
  SetTimes(fd, creationTime, lastAccessTime, lastWriteTime);
}
#endif

void File::SetTimes(const PathName& path, time_t creationTime, time_t lastAccessTime, time_t lastWriteTime)
{
    SDL_SetTimes(path.ToString().c_str(), creationTime, lastAccessTime, lastWriteTime);
}
/*
void miktex_GetTimes(const PathName& path, time_t& creationTime, time_t& lastAccessTime, time_t& lastWriteTime)
{
  WIN32_FIND_DATAW findData;
  HANDLE findHandle = FindFirstFileW(path.ToExtendedLengthPathName().ToWideCharString().c_str(), &findData);
  if (findHandle == INVALID_HANDLE_VALUE)
  {
    MIKTEX_FATAL_WINDOWS_ERROR_2("FindFirstFileW", "path", path.ToString());
  }
  if (!FindClose(findHandle))
  {
    MIKTEX_FATAL_WINDOWS_ERROR_2("FindClose", "path", path.ToString());
  }
  creationTime = FileTimeToUniversalCrtTime(findData.ftCreationTime);
  lastAccessTime = FileTimeToUniversalCrtTime(findData.ftLastAccessTime);
  lastWriteTime = FileTimeToUniversalCrtTime(findData.ftLastWriteTime);
}
*/
void File::GetTimes(const PathName& path, time_t& creationTime, time_t& lastAccessTime, time_t& lastWriteTime)
{
    // miktex_GetTimes(path, creationTime, lastAccessTime, lastWriteTime);

    lastWriteTime = file_3times(path.ToString(), &creationTime, &lastAccessTime);
  
    // VALIDATE(creationTime == creationTime2 && lastAccessTime == lastAccessTime2 && lastWriteTime == lastWriteTime2, null_str);
}

void File::Delete(const PathName& path)
{
    SDL_DeleteFiles(path.ToString().c_str());
}

void File::Move(const PathName& source, const PathName& dest, FileMoveOptionSet options)
{
    const std::string dest_str = dest.ToString();
    SDL_bool replace_existing = options[FileMoveOption::ReplaceExisting]? SDL_TRUE: SDL_FALSE;
    if (replace_existing || (!SDL_IsFile(dest_str.c_str()) && !SDL_IsDirectory(dest_str.c_str()))) {
        if (!SDL_MoveFile(source.ToString().c_str(), dest_str.c_str())) {
            return;
        }
    }

    if (options[FileMoveOption::UpdateFndb]) {
        auto session = SESSION_IMPL();
        if (session->IsTEXMFFile(source) && Fndb::FileExists(source)) {
            Fndb::Remove({ source });
        }
        if (session->IsTEXMFFile(dest) && !Fndb::FileExists(dest)) {
            Fndb::Add({ {dest} });
        }
    }
}

void File::Copy(const PathName& source, const PathName& dest, FileCopyOptionSet options)
{
    if (!SDL_CopyFiles(source.ToString().c_str(), dest.ToString().c_str())) {
        return;
    }

    if (options[FileCopyOption::UpdateFndb]) {
        if (SESSION_IMPL()->IsTEXMFFile(dest) && !Fndb::FileExists(dest)) {
            Fndb::Add({ {dest} });
        }
    }
}

#ifdef _WIN32
void File::CreateLink(const PathName& oldName, const PathName& newName, CreateLinkOptionSet options)
{
  if (options[CreateLinkOption::ReplaceExisting] && File::Exists(newName) )
  {
    FileDeleteOptionSet deleteOptions = { FileDeleteOption::TryHard };
    if (options[CreateLinkOption::UpdateFndb])
    {
      deleteOptions += FileDeleteOption::UpdateFndb;
    }
    File::Delete(newName, deleteOptions);
  }
  auto trace_files = TraceStream::Open(MIKTEX_TRACE_FILES);
  trace_files->WriteLine("core", fmt::format(T_("creating {0} link from {1} to {2}"), options[CreateLinkOption::Symbolic] ? "symbolic" : "hard", Q_(newName), Q_(oldName)));
  if (options[CreateLinkOption::Symbolic])
  {
    UNIMPLEMENTED();
  }
  else if (CreateHardLinkW(newName.ToExtendedLengthPathName().ToWideCharString().c_str(), oldName.ToExtendedLengthPathName().ToWideCharString().c_str(), nullptr) == 0)
  {
    MIKTEX_FATAL_WINDOWS_ERROR_2("CreateHardLinkW", "path", newName.ToString(), "existing", oldName.ToString());
  }
  if (options[CreateLinkOption::UpdateFndb])
  {
    if (SESSION_IMPL()->IsTEXMFFile(newName) && !Fndb::FileExists(newName))
    {
      Fndb::Add({ {newName} });
    }
  }
}

bool File::IsSymbolicLink(const PathName& path)
{
  UNIMPLEMENTED();
}

PathName File::ReadSymbolicLink(const PathName& path)
{
  UNIMPLEMENTED();
}

size_t File::SetMaxOpen(size_t newMax)
{
  newMax = min((int)newMax, 2048);
  int oldMax = _getmaxstdio();
  if (oldMax >= newMax)
  {
    newMax = oldMax;
  }
  else
  {
    auto trace_files = TraceStream::Open(MIKTEX_TRACE_FILES);
    trace_files->WriteLine("core", fmt::format(T_("increasing maximum number of simultaneously open files (oldmax={0}, newmax={1})"), oldMax, newMax));
    if (_setmaxstdio(static_cast<int>(newMax)) < 0)
    {
      MIKTEX_FATAL_CRT_ERROR_2("_setmaxstdio", "newmax", std::to_string(newMax));
    }
  }
  return newMax;
}

#ifdef __cplusplus
extern "C" {
#endif
int is_tmp_exam_tex = false;
#ifdef __cplusplus
}
#endif

FILE* File::Open(const PathName& path, FileMode mode, FileAccess access, bool isTextFile, FileOpenOptionSet options)
{
    SDL_Log("%u {file}[open]File::Open, file: %s", SDL_GetTicks(), path.ToString().c_str());
    if (path.ToString().find("tuenc.def") != std::string::npos) {
        tuenc_verbose = 0; // 1
        int ii = 0;

    } else if (path.ToString().find("tmp_exam.pdf") != std::string::npos) {
        int ii = 0;

    } else if (path.ToString().find("tmp_exam.tex") != std::string::npos) {
        int ii = 0;
        is_tmp_exam_tex = 1;

    } else if (path.ToString().find("texsys.aux") != std::string::npos) {
        int ii = 0;

    } else if (path.ToString().find("lualatex.fmt") != std::string::npos) {
        int ii = 0;

    }

/*
    uint32_t desired_access = 0;
    uint32_t create_disposition = OPEN_EXISTING;

    if (mode == FileMode::Create) {
        create_disposition = CREATE_ALWAYS;

    } else if (mode == FileMode::CreateNew) {
        create_disposition = CREATE_ALWAYS;

    } else if (mode == FileMode::Append) {
        int ii = 0;

    } else if(mode == FileMode::Open) {
        int ii = 0;

    } else {
        VALIDATE(false, null_str);
    }

    if (access == FileAccess::ReadWrite) {
        desired_access = GENERIC_READ | GENERIC_WRITE;

    } else if (access == FileAccess::Read) {
        desired_access = GENERIC_READ;

    } else if (access == FileAccess::Write) {
        desired_access = GENERIC_WRITE;

    } else {
        VALIDATE(false, null_str);
    }


    if (mode == FileMode::Create || mode == FileMode::CreateNew || mode == FileMode::Append) {
        PathName dir(path);
        dir.MakeFullyQualified();
        dir.RemoveFileSpec();

        if (!Directory::Exists(dir)) {
            Directory::Create(dir);
        }
    }

    posix_file_t fd;
    posix_fopen(path.ToString().c_str(), desired_access, create_disposition, fd);
    return fd;
*/
  auto trace_files = TraceStream::Open(MIKTEX_TRACE_FILES);
  
  trace_files->WriteLine("core", fmt::format(T_("opening file {0} ({1} 0x{2:x} {3})"), Q_(path), static_cast<int>(mode), static_cast<int>(access), isTextFile));

  int flags = 0;
  string strFlags;

  if (mode == FileMode::Create)
  {
    flags |= O_CREAT;
  }
  else if (mode == FileMode::CreateNew)
  {
    flags |= O_CREAT | O_EXCL;
  }
  else if (mode == FileMode::Append)
  {
    flags |= O_CREAT | O_APPEND;
  }

  if (access == FileAccess::ReadWrite)
  {
    flags |= O_RDWR;
    if (mode == FileMode::Append)
    {
      strFlags += "a+";
    }
    else
    {
      strFlags += "r+";
    }
  }
  else if (access == FileAccess::Read)
  {
    flags |= O_RDONLY;
    strFlags += "r";
  }
  else if (access == FileAccess::Write)
  {
    flags |= O_WRONLY;
    if (mode == FileMode::Append)
    {
      strFlags += "a";
    }
    else
    {
      flags |= O_TRUNC;
      strFlags += "w";
    }
  }

  if (options[FileOpenOption::DeleteOnClose])
  {
    flags |= O_TEMPORARY;
  }

#if defined(O_SEQUENTIAL)
  flags |= O_SEQUENTIAL;
#if 0
  strFlags += "S";
#endif
#endif

  if (isTextFile)
  {
    flags |= O_TEXT;
    strFlags += "t";
  }
  else
  {
    flags |= O_BINARY;
    strFlags += "b";
  }

  if (mode == FileMode::Create || mode == FileMode::CreateNew || mode == FileMode::Append)
  {
    PathName dir(path);
    dir.MakeFullyQualified();
    dir.RemoveFileSpec();
    if (!Directory::Exists(dir))
    {
      Directory::Create(dir);
    }
  }

  int fd = _wopen(path.ToExtendedLengthPathName().ToWideCharString().c_str(), flags, ((flags & O_CREAT) == 0) ? 0 : S_IREAD | S_IWRITE);
  if (fd < 0)
  {
    if (errno == EINVAL && ::GetLastError() == ERROR_USER_MAPPED_FILE)
    {
      MIKTEX_FATAL_WINDOWS_ERROR_2("CreateFileW", "path", path.ToString(), "modeString", strFlags);
    }
    else
    {
      MIKTEX_FATAL_CRT_ERROR_2("_wopen", "path", path.ToString(), "modeString", strFlags);
    }
  }

  return FdOpen(path, fd, strFlags.c_str());
}
#else
void File::CreateLink(const PathName& oldName, const PathName& newName, CreateLinkOptionSet options)
{
  if (options[CreateLinkOption::ReplaceExisting] && File::Exists(newName) )
  {
    FileDeleteOptionSet deleteOptions = { FileDeleteOption::TryHard };
    if (options[CreateLinkOption::UpdateFndb])
    {
      deleteOptions += FileDeleteOption::UpdateFndb;
    }
    File::Delete(newName, deleteOptions);
  }
  auto trace_files = TraceStream::Open(MIKTEX_TRACE_FILES);
  trace_files->WriteLine("core", fmt::format(T_("creating {0} link from {1} to {2}"), options[CreateLinkOption::Symbolic] ? "symbolic" : "hard",  Q_(newName), Q_(oldName)));
  if (options[CreateLinkOption::Symbolic])
  {
    if (symlink(oldName.GetData(), newName.GetData()) != 0)
    {
      MIKTEX_FATAL_CRT_ERROR_2("symlink", "oldName", oldName.ToString(), "newName", newName.ToString());
    }
  }
  else
  {
    if (link(oldName.GetData(), newName.GetData()) != 0)
    {
      MIKTEX_FATAL_CRT_ERROR_2("link", "oldName", oldName.ToString(), "newName", newName.ToString());
    }
  }
  if (options[CreateLinkOption::UpdateFndb])
  {
    shared_ptr<SessionImpl> session = SESSION_IMPL();
    if (session->IsTEXMFFile(newName) && !Fndb::FileExists(newName))
    {
      Fndb::Add({ { newName } });
    }
  }
}

bool File::IsSymbolicLink(const PathName& path)
{
  struct stat statbuf;
  if (lstat(path.GetData(), &statbuf) != 0)
  {
    MIKTEX_FATAL_CRT_ERROR_2("lstat", "path", path.ToString());
  }
  return S_ISLNK(statbuf.st_mode);
}

PathName File::ReadSymbolicLink(const PathName& path)
{
  PathName result;
  ssize_t len = readlink(path.GetData(), result.GetData(), result.GetCapacity());
  if (len < 0)
  {
    MIKTEX_FATAL_CRT_ERROR_2("readlink", "path", path.ToString());
  }
  if (len == result.GetCapacity())
  {
    BUF_TOO_SMALL();
  }
  result[len] = 0;
  return result;
}

size_t File::SetMaxOpen(size_t newMax)
{
  // FIXME: unimplemented
  return FOPEN_MAX;
}

FILE* File::Open(const PathName& path, FileMode mode, FileAccess access, bool isTextFile, FileOpenOptionSet options)
{
    SDL_Log("{file}[open]File::Open, file: %s", path.ToString().c_str());
  UNUSED_ALWAYS(isTextFile);

  auto trace_files = TraceStream::Open(MIKTEX_TRACE_FILES);

  trace_files->WriteLine("core", fmt::format(T_("opening file {0} ({1} {2} {3})"), Q_(path), static_cast<int>(mode), static_cast<int>(access), static_cast<int>(isTextFile)));

  int flags = 0;
  string strFlags;

  if (mode == FileMode::Create)
  {
    flags |= O_CREAT;
  }
  else if (mode == FileMode::CreateNew)
  {
    flags |= O_CREAT | O_EXCL;
  }
  else if (mode == FileMode::Append)
  {
    flags |= O_CREAT | O_APPEND;
  }

  if (access == FileAccess::ReadWrite)
  {
    flags |= O_RDWR;
    if (mode == FileMode::Append)
    {
      strFlags += "a+";
    }
    else
    {
      strFlags += "r+";
    }
  }
  else if (access == FileAccess::Read)
  {
    flags |= O_RDONLY;
    strFlags += "r";
  }
  else if (access == FileAccess::Write)
  {
    flags |= O_WRONLY;
    if (mode == FileMode::Append)
    {
      strFlags += "a";
    }
    else
    {
      flags |= O_TRUNC;
      strFlags += "w";
    }
  }

  if (mode == FileMode::Create || mode == FileMode::CreateNew || mode == FileMode::Append)
  {
    PathName dir(path);
    dir.MakeFullyQualified();
    dir.RemoveFileSpec();
    if (!Directory::Exists(dir))
    {
      Directory::Create(dir);
    }
  }

  int fd;

  fd = open(path.GetData(), flags, (((flags & O_CREAT) == 0) ? 0 : (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)));

  if (fd < 0)
  {
    MIKTEX_FATAL_CRT_ERROR_2("open", "path", path.ToString(), "mode", strFlags);
  }
  
  try
  {
    if (options[FileOpenOption::DeleteOnClose])
    {
      File::Delete(path);
    }
    return FdOpen(fd, strFlags.c_str());
  }
  catch (const exception&)
  {
    close(fd);
    throw;
  }
}
#endif
posix_file_t File::Open2(const PathName& path, FileMode mode, FileAccess access, bool isTextFile, FileOpenOptionSet options)
{
    SDL_Log("{file}[open2]File::Open, file: %s", path.ToString().c_str());
    if (path.ToString().find("test2.tex") != std::string::npos) {
        int ii = 0;

    } else if (path.ToString().find("test2.pdf") != std::string::npos) {
        int ii = 0;

    } else if (path.ToString().find("cmex7.tfm") != std::string::npos) {
        int ii = 0;

    } else if (path.ToString().find("test2.synctex.gz") != std::string::npos) {
        int ii = 0;

    } else if (path.ToString().find("pdflatex.fmt") != std::string::npos) {
        int ii = 0;

    } else if (path.ToString().find("xelatex.fmt") != std::string::npos) {
        int ii = 0;
    }

    uint32_t desired_access = 0;
    uint32_t create_disposition = OPEN_EXISTING;

    if (mode == FileMode::Create) {
        create_disposition = CREATE_ALWAYS;

    } else if (mode == FileMode::CreateNew) {
        create_disposition = CREATE_ALWAYS;

    } else if (mode == FileMode::Append) {
        int ii = 0;

    } else if(mode == FileMode::Open) {
        int ii = 0;

    } else {
        VALIDATE(false, null_str);
    }

    if (access == FileAccess::ReadWrite) {
        desired_access = GENERIC_READ | GENERIC_WRITE;

    } else if (access == FileAccess::Read) {
        desired_access = GENERIC_READ;

    } else if (access == FileAccess::Write) {
        desired_access = GENERIC_WRITE;

    } else {
        VALIDATE(false, null_str);
    }


    if (mode == FileMode::Create || mode == FileMode::CreateNew || mode == FileMode::Append) {
        PathName dir(path);
        dir.MakeFullyQualified();
        dir.RemoveFileSpec();

        if (!Directory::Exists(dir)) {
            Directory::Create(dir);
        }
    }

    posix_file_t fd;
    posix_fopen(path.ToString().c_str(), desired_access, create_disposition, fd);
    return fd;
}

#ifdef _WIN32
bool File::TryLock(HANDLE hFile, File::LockType lockType, chrono::milliseconds timeout)
{
    SDL_Log("{android}File::TryLock---");
  chrono::time_point<chrono::high_resolution_clock> tryUntil = chrono::high_resolution_clock::now() + timeout;
  bool locked;
  do
  {
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    locked = LockFileEx(hFile, (lockType == LockType::Exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0) | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &overlapped) ? true : false;
    if (!locked)
    {
      if (GetLastError() != ERROR_LOCK_VIOLATION)
      {
        MIKTEX_FATAL_WINDOWS_ERROR("LockFileEx");
      }
      this_thread::sleep_for(10ms);
    }
  } while (!locked && chrono::high_resolution_clock::now() < tryUntil);
  SDL_Log("{android}---File::TryLock, locked: %s", locked? "true": "false");
  return locked;
}

bool File::TryLock(int fd, File::LockType lockType, chrono::milliseconds timeout)
{
  return TryLock(reinterpret_cast<HANDLE>(_get_osfhandle(fd)), lockType, timeout);
}

void File::Unlock(HANDLE hFile)
{
  OVERLAPPED overlapped;
  memset(&overlapped, 0, sizeof(overlapped));
  if (!UnlockFileEx(hFile, 0, MAXDWORD, MAXDWORD, &overlapped))
  {
    MIKTEX_FATAL_WINDOWS_ERROR("UnlockFileEx");
  }
}

void File::Unlock(int fd)
{
  Unlock(reinterpret_cast<HANDLE>(_get_osfhandle(fd)));
}
#else
bool File::TryLock(int fd, File::LockType lockType, chrono::milliseconds timeout)
{
    SDL_Log("{android}File::TryLock---");
  chrono::time_point<chrono::high_resolution_clock> tryUntil = chrono::high_resolution_clock::now() + timeout;
  bool locked;
  do
  {
    locked = flock(fd, (lockType == LockType::Exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) == 0;
    if (!locked)
    {
      if (errno != EWOULDBLOCK)
      {
        MIKTEX_FATAL_CRT_ERROR("flock");
      }
      this_thread::sleep_for(10ms);
    }
  } while (!locked && chrono::high_resolution_clock::now() < tryUntil);
  SDL_Log("{android}---File::TryLock, locked: %s", locked? "true": "false");
  return locked;
}

void File::Unlock(int fd)
{
    SDL_Log("{android}File::Unlock---fd: %i", fd);
  if (flock(fd, LOCK_UN) != 0)
  {
    MIKTEX_FATAL_CRT_ERROR("flock");
  }
}
#endif