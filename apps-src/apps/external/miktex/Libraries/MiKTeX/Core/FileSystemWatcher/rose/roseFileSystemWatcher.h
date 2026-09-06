/**
 * @file unxFileSystemWatcher.h
 * @author Christian Schenk
 * @brief File system watcher (Linux)
 *
 * @copyright Copyright © 2021-2023 Christian Schenk
 *
 * This file is part of the MiKTeX Core Library.
 *
 * The MiKTeX Core Library is licensed under GNU General Public License version
 * 2 or any later version.
 */

#ifndef MIKTEX_ROSEFILESYSTEMWATCHER_H
#define MIKTEX_ROSEFILESYSTEMWATCHER_H

#include <unordered_map>
#include <vector>

#include "../FileSystemWatcherBase.h"

CORE_INTERNAL_BEGIN_NAMESPACE;

// Don't monitoring changes in the TEXMF directory tree, such as the creation, modification, and deletion of files. 
// However, I do not want to remove the code related to FileSystemWatcherBase in other modules, so I wrote a class that does nothing.
class roseFileSystemWatcher :
  public FileSystemWatcherBase
{

public:

    roseFileSystemWatcher();

    virtual MIKTEXTHISCALL ~roseFileSystemWatcher();

private:

    void MIKTEXTHISCALL AddDirectories(const std::vector<MiKTeX::Util::PathName>& directories) override;

    bool MIKTEXTHISCALL Start() override;

    bool MIKTEXTHISCALL Stop() override;

    void MIKTEXTHISCALL WatchDirectories() override;

    std::unordered_map<int, MiKTeX::Util::PathName> directories;
};

CORE_INTERNAL_END_NAMESPACE;

#endif
