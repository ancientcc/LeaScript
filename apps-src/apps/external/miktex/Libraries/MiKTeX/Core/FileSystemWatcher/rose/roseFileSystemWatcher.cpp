/**
 * @file roseFileSystemWatcher.cpp
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

#include "../../miktex_Core_config.h"

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "../../miktex_Core_internal.h"

#include "roseFileSystemWatcher.h"
#include <SDL_log.h>

using namespace std;

using namespace MiKTeX::Core;
using namespace MiKTeX::Util;

unique_ptr<FileSystemWatcher> FileSystemWatcher::Create()
{
    return make_unique<roseFileSystemWatcher>();
}

roseFileSystemWatcher::roseFileSystemWatcher()
{
    SDL_Log("roseFileSystemWatcher::roseFileSystemWatcher");
}

roseFileSystemWatcher::~roseFileSystemWatcher()
{
    SDL_Log("roseFileSystemWatcher::~roseFileSystemWatcher");
}

void roseFileSystemWatcher::AddDirectories(const vector<PathName>& directories)
{
    unique_lock<shared_mutex> l(mutex);
    for (const auto& dir : directories) {
    }
}

bool roseFileSystemWatcher::Start()
{
    return true;
}

bool roseFileSystemWatcher::Stop()
{
    return true;
}

void roseFileSystemWatcher::WatchDirectories()
{
}
