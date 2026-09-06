/* makefndb.cpp: creating the file name database

   Copyright (C) 1996-2024 Christian Schenk

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

#include "../miktex_Core_config.h"

#include <fstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <miktex/Core/AutoResource>
#include <miktex/Core/Directory>
#include <miktex/Core/FileStream>
#include <miktex/Core/Paths>
#include <miktex/Core/TemporaryFile>

#include <miktex/Trace/Trace>

#include <miktex/Util/PathName>

#include "../miktex_Core_internal.h"

#include "../Session/SessionImpl.h"
#include "fndbmem.h"

#include <SDL_timer.h>
#include "filesystem.hpp"

using namespace std::placeholders;

using namespace std;

using namespace MiKTeX::Core;
using namespace MiKTeX::Trace;
using namespace MiKTeX::Util;

extern std::string miktex_sandbox_dir;
const uint8_t null_byte = 0;

#define FN_MIKTEXIGNORE ".miktexignore"

struct FILENAMEINFO
{
  string FileName;
  const string* Directory = nullptr;
  const string* Info = nullptr;
};

class FndbManager
{
public:
  FndbManager() :
    trace_fndb(TraceStream::Open(MIKTEX_TRACE_FNDB)),
    trace_error(TraceStream::Open(MIKTEX_TRACE_ERROR))
  {
  }

public:
  bool Create(const PathName& fndbPath, const PathName& rootPath, ICreateFndbCallback* callback, bool enableStringPooling, bool storeFileNameInfo, bool nofile);

public:
  void* GetMemPointer()
  {
    return byteArray.data();
  }

public:
  FndbByteOffset GetMemTop() const
  {
    return static_cast<FndbByteOffset>(byteArray.size());
  }

private:
  void SetMem(FndbByteOffset fo, const void* data, size_t size)
  {
    MIKTEX_ASSERT(fo + size <= GetMemTop());
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(data);
    const uint8_t* end = begin + size;
    std::copy(begin, end, byteArray.begin() + fo);
  }

private:
  void SetMem(FndbByteOffset fo, FndbByteOffset data)
  {
    SetMem(fo, &data, sizeof(data));
  }

private:
  FndbByteOffset ReserveMem(size_t size);

private:
  void FastPushBack(uint8_t data)
  {
    byteArray.push_back(data);
  }

private:
  FndbByteOffset PushBack(uint8_t data)
  {
    FndbByteOffset ret = GetMemTop();
    FastPushBack(data);
    return ret;
  }

private:
  FndbByteOffset PushBack(FndbWord data);

private:
  FndbByteOffset PushBack(const void* data, size_t size);

private:
  FndbByteOffset PushBack(const char* data);

private:
  void AlignMem(size_t align = 8);

private:
  static void GetIgnorableFiles(const PathName& dirPath, vector<string>& filesToBeIgnored);

public:
  void ReadDirectory(const PathName& dirPath, vector<string>& subDirectoryNames, vector<FILENAMEINFO>& fileNames, bool doCleanUp);

private:
  void CollectFiles(const PathName& parentPath, const PathName& folderName, vector<FILENAMEINFO>& fileNames);
  void CollectFiles2(const PathName& parentPath, vector<FILENAMEINFO>& fileNames, size_t& num_directories);
  bool did_walk_fndb_path(const std::string& dir, const SDL_dirent2* dirent, std::vector<FILENAMEINFO>& files, size_t& num_directories, const std::string& root);

private:
  PathName rootPath;

private:
  vector<uint8_t> byteArray;

private:
  size_t deepestLevel;

private:
  size_t currentLevel;

private:
  size_t numDirectories;

private:
  size_t numFiles;

private:
  ICreateFndbCallback* callback;

private:
  unordered_set<string> stringPool;
  unordered_set<string> stringPool2;
  
private:
  typedef unordered_map<string, FndbByteOffset> StringMap;

private:
  StringMap stringMap;

private:
  bool enableStringPooling;

private:
  bool storeFileNameInfo;

private:
  unique_ptr<TraceStream> trace_fndb;

private:
  unique_ptr<TraceStream> trace_error;
};

FndbByteOffset FndbManager::ReserveMem(size_t size)
{
  FndbByteOffset ret = GetMemTop();

  const bool use_memset = true;
  // uint32_t start = SDL_GetTicks();
  if (!use_memset) {      
      byteArray.reserve(byteArray.size() + size);
      for (size_t i = 0; i < size; ++i)
      {
        byteArray.push_back(null_byte);
      }
  } else {
      size_t oldSize = byteArray.size();
      byteArray.resize(oldSize + size);
      uint8_t* ptr = byteArray.data() + oldSize;
      memset(ptr, null_byte, size);
  }

  // SDL_Log("{use_memset: %s}ReserveMem(%i), byteArray.size: %i, cost %u ms", 
  //    use_memset? "true": "flase", (int)size, (int)byteArray.size(), SDL_GetTicks() - start);
  return ret;
}

FndbByteOffset FndbManager::PushBack(FndbWord data)
{
  AlignMem(sizeof(FndbWord));
  const uint8_t* byteArray = reinterpret_cast<const uint8_t*>(&data);
  if (sizeof(FndbWord) == 4)
  {
    FndbByteOffset ret = GetMemTop();
    FastPushBack(byteArray[0]);
    FastPushBack(byteArray[1]);
    FastPushBack(byteArray[2]);
    FastPushBack(byteArray[3]);
    return ret;
  }
  else
  {
    return PushBack(byteArray, sizeof(FndbWord));
  }
}

FndbByteOffset FndbManager::PushBack(const void* data, size_t size)
{
  FndbByteOffset ret = GetMemTop();
  const uint8_t* byteArray = reinterpret_cast<const uint8_t*>(data);
  for (size_t i = 0; i < size; ++i)
  {
    FastPushBack(byteArray[i]);
  }
  return ret;
}

FndbByteOffset FndbManager::PushBack(const char* data)
{
  if (enableStringPooling)
  {
    StringMap::const_iterator it = stringMap.find(data);
    if (it != stringMap.end())
    {
      return it->second;
    }
  }
  FndbByteOffset ret = GetMemTop();
  MIKTEX_ASSERT(data != nullptr);
  PushBack(data, strlen(data));
  FastPushBack(null_byte);
  if (enableStringPooling)
  {
    stringMap[data] = ret;
  }
  return ret;
}

void FndbManager::AlignMem(size_t align)
{
  FndbByteOffset foTop = GetMemTop();
  while (((foTop++) % align) > 0)
  {
    FastPushBack(null_byte);
  }
}

void FndbManager::GetIgnorableFiles(const PathName& dirPath, vector<string>& filesToBeIgnored)
{
  PathName ignoreFile(dirPath / FN_MIKTEXIGNORE);
  if (!File::Exists(ignoreFile))
  {
    return;
  }
  ifstream reader = File::CreateInputStream(ignoreFile);
  filesToBeIgnored.reserve(10);
  for (string line; std::getline(reader, line); )
  {
    filesToBeIgnored.push_back(line);
  }
  sort(filesToBeIgnored.begin(), filesToBeIgnored.end(), StringComparerIgnoringCase());
}

void FndbManager::ReadDirectory(const PathName& dirPath, vector<string>& subDirectoryNames, vector<FILENAMEINFO>& fileNames, bool doCleanUp)
{
  if (!Directory::Exists(dirPath))
  {
    trace_fndb->WriteLine("core", fmt::format(T_("the directory {0} does not exist"), Q_(dirPath)));
    return;
  }
  vector<string> filesToBeIgnored;
  GetIgnorableFiles(dirPath, filesToBeIgnored);
  unique_ptr<DirectoryLister> lister = DirectoryLister::Open(dirPath);
  DirectoryEntry entry;
  vector<DirectoryEntry> toBeDeleted;
  PathName directory(Utils::GetRelativizedPath(dirPath.GetData(), rootPath.GetData()));
  directory = directory.ToUnix();
  while (lister->GetNext(entry))
  {
    if (binary_search(filesToBeIgnored.begin(), filesToBeIgnored.end(), entry.name, StringComparerIgnoringCase()))
    {
      continue;
    }
    if (doCleanUp && PathName(entry.name).HasExtension(MIKTEX_TO_BE_DELETED_FILE_SUFFIX))
    {
      toBeDeleted.push_back(entry);
    }
    else if (entry.isDirectory)
    {
      subDirectoryNames.push_back(entry.name);
    }
    else
    {
      FILENAMEINFO filenameinfo;
      filenameinfo.FileName = entry.name;
      filenameinfo.Directory = &*stringPool.insert(directory.ToString()).first;
      fileNames.push_back(filenameinfo);
    }
  }
  lister->Close();

  // silent clean-up
  for (const DirectoryEntry& e : toBeDeleted)
  {
    try
    {
      PathName path(dirPath / e.name);
      if (e.isDirectory)
      {
        Directory::Delete(path, true);
      }
      else
      {
        File::Delete(path);
      }
    }
    catch (const exception&)
    {
    }
  }
}

void FndbManager::CollectFiles(const PathName& parentPath, const PathName& folderName, vector<FILENAMEINFO>& fileNames)
{
  if (currentLevel > deepestLevel)
  {
    deepestLevel = currentLevel;
  }

  vector<string> subDirectoryNames;
  subDirectoryNames.reserve(40);

  bool done = false;

  PathName path(parentPath / folderName.ToString());
  path.MakeFullyQualified();

  PathName directory(Utils::GetRelativizedPath(path.GetData(), rootPath.GetData()));
  directory = directory.ToUnix();

  if (callback != nullptr)
  {
    if (!callback->OnProgress(static_cast<unsigned>(currentLevel), path))
    {
      throw OperationCancelledException();
    }
    vector<string> subDirs;
    vector<string> files;
    vector<string> infos;
    done = callback->ReadDirectory(path, subDirs, files, infos);
    if (done)
    {
      subDirectoryNames = subDirs;
      MIKTEX_ASSERT(files.size() == infos.size());
      for (int i = 0; i < files.size(); ++i)
      {
        FILENAMEINFO filenameinfo;
        filenameinfo.FileName = files[i];
        filenameinfo.Directory = &*stringPool.insert(directory.ToString()).first;
        filenameinfo.Info = &*stringPool.insert(infos[i]).first;
        fileNames.push_back(filenameinfo);
      }
    }
  }

  if (!done)
  {
    ReadDirectory(path, subDirectoryNames, fileNames, true);
  }

  numDirectories += subDirectoryNames.size();

  // recurse into sub-directories
  PathName pathFolder(parentPath / folderName.ToString());
  size_t i = 0;
  ++currentLevel;
  for (const string& s : subDirectoryNames)
  {
    // RECURSION
    CollectFiles(pathFolder, PathName(s), fileNames);
    ++i;
  }
  --currentLevel;
}

bool FndbManager::did_walk_fndb_path(const std::string& dir, const SDL_dirent2* dirent, std::vector<FILENAMEINFO>& fileNames, size_t& num_directories, const std::string& root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
        // at this point, 'walk_dir' has already removed '.' and '..'.
        // perfectly, the fileNames needs to remove these two directories.
        num_directories ++;

	} else {
        // SDL_Log("#%i, dir: %s, dirent->name: %s", (int)fileNames.size(), dir.c_str(), dirent->name);
        FILENAMEINFO filenameinfo;
        filenameinfo.FileName = dirent->name;
        if (dir.size() != root.size()) {
            filenameinfo.Directory = &*stringPool2.insert(dir.substr(root.size() + 1)).first;
        } else {
            filenameinfo.Directory = &*stringPool2.insert(null_str).first;
        }
        fileNames.push_back(filenameinfo);
    }
	return true;
}

void FndbManager::CollectFiles2(const PathName& parentPath, std::vector<FILENAMEINFO>& fileNames, size_t& num_directories)
{
    const std::string root = parentPath.ToString();
    walk_dir(root, true, std::bind(&FndbManager::did_walk_fndb_path, this, _1, _2, std::ref(fileNames), std::ref(num_directories), std::ref(root)));
}

void compare_filenames(const std::vector<FILENAMEINFO>& fileNames, const std::vector<FILENAMEINFO>& fileNames2)
{
    std::set<std::string> files;
    for (std::vector<FILENAMEINFO>::const_iterator it = fileNames.begin(); it != fileNames.end(); ++ it) {
        const FILENAMEINFO& info = *it;
        const std::string file = *info.Directory + "/" + info.FileName;
        VALIDATE(info.Info == nullptr, null_str);
        VALIDATE(files.count(file) == 0, null_str);
        files.insert(file);
    }

    std::set<std::string> files2;
    for (std::vector<FILENAMEINFO>::const_iterator it = fileNames2.begin(); it != fileNames2.end(); ++ it) {
        const FILENAMEINFO& info = *it;
        const std::string file = *info.Directory + "/" + info.FileName;
        VALIDATE(info.Info == nullptr, null_str);
        VALIDATE(files2.count(file) == 0, null_str);
        files2.insert(file);
    }

    VALIDATE(files.size() == files2.size(), null_str);
    VALIDATE(files == files2, null_str);
}

bool FndbManager::Create(const PathName& fndbPath, const PathName& rootPath, ICreateFndbCallback* callback, bool enableStringPooling, bool storeFileNameInfo, bool nofile)
{
  trace_fndb->WriteLine("core", fmt::format(T_("creating fndb file {0}..."), Q_(fndbPath)));
  const uint32_t start_ticks = SDL_GetTicks();
  SDL_Log("%u (FndbManager::Create)creating fndb file. fndbPath: %s, rootPath: %s, nofile: %s", 
      start_ticks, fndbPath.ToString().c_str(), rootPath.ToString().c_str(), nofile? "true": "false");
  unsigned rootIdx = SESSION_IMPL()->DeriveTEXMFRoot(rootPath);
  this->rootPath = rootPath;
  this->enableStringPooling = enableStringPooling;
  this->storeFileNameInfo = storeFileNameInfo;
  byteArray.reserve(2 * 1024 * 1024);
  try
  {
    ReserveMem(sizeof(FileNameDatabaseHeader));
    FileNameDatabaseHeader fndb;
    fndb.Init();
    numDirectories = 0;
    numFiles = 0;
    deepestLevel = 0;
    currentLevel = 0;
    this->callback = callback;

    vector<FILENAMEINFO> fileNames;
    if (false) {
        CollectFiles(rootPath, PathName(CURRENT_DIRECTORY), fileNames);

    } else {
        CollectFiles2(rootPath, fileNames, numDirectories);

        // vector<FILENAMEINFO> fileNames2;
        // size_t numDirectories2 = 0;
        // CollectFiles2(rootPath, fileNames2, numDirectories2);
        // compare_filenames(fileNames, fileNames2, numDirectories2);
        // VALIDATE(numDirectories == numDirectories2, null_str);
    }

    uint32_t now = SDL_GetTicks();
    // SDL_Log("%u (FndbManager::Create)post CollectFiles. fileNames.size: %i, cost %u ms", now, (int)fileNames.size(), now - start_ticks);
    SDL_Log("%u (FndbManager::Create)post CollectFiles. fileNames.size: %i, cost %u ms", now, (int)fileNames.size(), now - start_ticks);
    numFiles = fileNames.size();
    AlignMem();
    fndb.foTable = ReserveMem(fileNames.size() * sizeof(FileNameDatabaseRecord));
    AlignMem();
    fndb.foStrings = GetMemTop();
    for (size_t idx = 0; idx < fileNames.size(); ++idx)
    {
      FileNameDatabaseRecord rec;
      rec.foFileName = PushBack(fileNames[idx].FileName.c_str());
      rec.foDirectory = PushBack(fileNames[idx].Directory->c_str());
      rec.foInfo = PushBack(fileNames[idx].Info == nullptr ? "" : fileNames[idx].Info->c_str());
      SetMem(static_cast<unsigned>(fndb.foTable + idx * sizeof(rec)), &rec, sizeof(rec));
    }
    fndb.numDirs = static_cast<unsigned>(numDirectories);
    fndb.numFiles = static_cast<unsigned>(numFiles);
    fndb.depth = static_cast<unsigned>(deepestLevel);
    fndb.size = GetMemTop();
    AlignMem(FNDB_PAGESIZE);
    SetMem(0, &fndb, sizeof(fndb));

    if (nofile) {
        now = SDL_GetTicks();
        SDL_Log("%u (FndbManager::Create)fndb creation completed(nofile=true), cost %u ms", now, now - start_ticks);
        return true;
    }

    VALIDATE(false, null_str);

    // <fixme>
    bool unloaded = false;
    for (size_t i = 0; !unloaded && i < 100; ++i)
    {
      unloaded = SESSION_IMPL()->UnloadFilenameDatabaseInternal(rootIdx, chrono::seconds(0));
      if (!unloaded)
      {
        trace_fndb->WriteLine("core", "sleep for 1ms");
        this_thread::sleep_for(chrono::milliseconds(1));
      }
    }
    if (!unloaded)
    {
      MIKTEX_FATAL_ERROR(T_("fndb cannot be unloaded"));
    }
    // </fixme>
    
    PathName tmpFndbPath(fndbPath);
    tmpFndbPath.AppendExtension(".tmp");
    unique_ptr<TemporaryFile> tmpFndbFile = TemporaryFile::Create(tmpFndbPath);
    // FileStream streamFndb;
    // streamFndb.Attach(File::Open(tmpFndbPath, FileMode::Create, FileAccess::Write, false));
    FileStream streamFndb(File::Open(tmpFndbPath, FileMode::Create, FileAccess::Write, false));
    streamFndb.Write(reinterpret_cast<const char*>(GetMemPointer()), GetMemTop());
    streamFndb.Close();
    if (File::Exists(fndbPath))
    {
      File::Delete(fndbPath, { FileDeleteOption::TryHard });
    }
    File::Move(tmpFndbPath, fndbPath);
    tmpFndbFile->Keep();

    PathName changeFile = fndbPath;
    changeFile.SetExtension(MIKTEX_FNDB_CHANGE_FILE_SUFFIX);
    if (File::Exists(changeFile))
    {
      File::Delete(changeFile);
    }
    now = SDL_GetTicks();
    SDL_Log("%u (FndbManager::Create)fndb creation completed(nofile=false), cost %u ms", now, now - start_ticks);
    trace_fndb->WriteLine("core", T_("fndb creation completed"));
    SESSION_IMPL()->RecordMaintenance();
    return true;
  }
  catch (const OperationCancelledException&)
  {
    trace_fndb->WriteLine("core", T_("fndb creation cancelled"));
    return false;
  }
}

bool Fndb::Create(const PathName& fndbPath, const PathName& rootPath, ICreateFndbCallback* callback)
{
  return Fndb::Create(fndbPath, rootPath, callback, true, false);
}

bool Fndb::Create(const PathName& fndbPath, const PathName& rootPath, ICreateFndbCallback* callback, bool enableStringPooling, bool storeFileNameInfo)
{
  FndbManager fndbmngr;

  if (!fndbmngr.Create(fndbPath, rootPath, callback, enableStringPooling, storeFileNameInfo, false))
  {
    return false;
  }

#if defined(MIKTEX_WINDOWS) && REPORT_EVENTS
  ReportMiKTeXEvent(EVENTLOG_INFORMATION_TYPE, MIKTEX_EVENT_FNDB_CREATED, fndbPath, rootPath, 0);
#endif

  return true;
}

uint8_t* Fndb::Create_nofile(const PathName& fndbPath, const PathName& rootPath, int& len)
{
    FndbManager fndbmngr;

    if (!fndbmngr.Create(fndbPath, rootPath, nullptr, true, false, true)) {
        return nullptr;
    }

    len = fndbmngr.GetMemTop();
    uint8_t* result = (uint8_t*)malloc(len);
    memcpy(result, fndbmngr.GetMemPointer(), len);
    return result;
}

bool Fndb::Refresh(const PathName& path, ICreateFndbCallback* callback)
{
  unsigned root = SESSION_IMPL()->DeriveTEXMFRoot(path);
  PathName pathFndbPath = SESSION_IMPL()->GetFilenameDatabasePathName(root);
  return Fndb::Create(pathFndbPath, SESSION_IMPL()->GetRootDirectoryPath(root), callback);
}

bool Fndb::Refresh(ICreateFndbCallback* callback)
{
  shared_ptr<SessionImpl> session = SESSION_IMPL();
  unsigned n = session->GetNumberOfTEXMFRoots();
  for (unsigned ord = 0; ord < n; ++ord)
  {
    if (session->IsAdminMode() && !session->IsCommonRootDirectory(ord))
    {
      // skipping user root directory
      continue;
    }
    if (!session->IsAdminMode() && session->IsCommonRootDirectory(ord) && !session->IsMiKTeXPortable())
    {
      // skipping common root directory
      continue;
    }
    PathName rootDirectory = session->GetRootDirectoryPath(ord);
    PathName pathFndbPath = session->GetFilenameDatabasePathName(ord);
    if (!Fndb::Create(pathFndbPath, rootDirectory, callback))
    {
      return false;
    }
  }
  return true;
}

void Fndb::ensure_file_exists()
{
    static bool called = false;
    if (called) {
        return;
    }

    MiKTeX::Util::PathName pathOut(miktex_sandbox_dir + "/miktex/config/pdflatex.ini");
    Fndb::Add({ {pathOut} });

/*
    std::vector<Fndb::Record> paths;
    paths.push_back({MiKTeX::Util::PathName(miktex_sandbox_dir + "/miktex/data/le/20251031101315.png")});
    paths.push_back({MiKTeX::Util::PathName(miktex_sandbox_dir + "/miktex/data/le/Info.plist")});
    paths.push_back({MiKTeX::Util::PathName(miktex_sandbox_dir + "/miktex/data/le/LaunchImage-1242x2208.png")});
    Fndb::Add(paths);
*/
    called = true;
}
