/**
 * @file winStartupConfig.cpp
 * @author Christian Schenk
 * @brief Startup configuration (Windows specials)
 *
 * @copyright Copyright © 1996-2024 Christian Schenk
 *
 * This file is part of the MiKTeX Core Library.
 *
 * The MiKTeX Core Library is licensed under GNU General Public License version
 * 2 or any later version.
 */

#include "../../miktex_Core_config.h"

// #include <Windows.h>
// #include <shlobj.h>

#include <miktex/Configuration/ConfigNames>
#include <miktex/Core/Paths>

#include "../../miktex_Core_internal.h"

#include "../../Session/SessionImpl.h"

#include "rose_config.hpp"
#include "wml_exception.hpp"

using namespace MiKTeX::Configuration;
using namespace MiKTeX::Core;
using namespace MiKTeX::Util;

extern std::string miktex_sandbox_dir;

/*
 * UserConfig:    %USERPROFILE%\AppData\Roaming\MiKTeX
 * UserData:      %USERPROFILE%\AppData\Local\MiKTeX
 * UserInstall:   %USERPROFILE%\AppData\Local\Programs\MiKTeX
 * CommonConfig:  C:\ProgramData\MiKTeX
 * CommonData:    C:\ProgramData\MiKTeX
 * CommonInstall: C:\Program Files\MiKTeX
 */
InternalStartupConfig SessionImpl::DefaultConfig(MiKTeXConfiguration config, VersionNumber setupVersion, const PathName& commonPrefixArg, const PathName& userPrefixArg)
{
    InternalStartupConfig ret;
    if (config == MiKTeXConfiguration::None) {
        config = MiKTeXConfiguration::Regular;
    }
    ret.config = config;
    ret.setupVersion = setupVersion;

    if (game_config::os == os_windows) {
        miktex_sandbox_dir = game_config::preferences_dir + "/miktex/sandbox";
    } else {
        // miktex_sandbox_dir = "/sdcard/apk/miktex/sandbox";
        miktex_sandbox_dir = game_config::preferences_dir + "/miktex/sandbox";
    }
    ret.userConfigRoot = miktex_sandbox_dir;
    ret.userDataRoot = ret.userConfigRoot;
    ret.userInstallRoot = ret.userConfigRoot;

    SDL_Log("{android}SessionImpl::DefaultConfig, userConfigRoot: %s", ret.userConfigRoot.ToString().c_str());
    return ret;
}

#ifdef _WIN32
InternalStartupConfig SessionImpl::ReadRegistry(ConfigurationScope scope)
{
    VALIDATE(false, null_str);

    InternalStartupConfig ret;
    return ret;
}

void SessionImpl::WriteRegistry(ConfigurationScope scope, const InternalStartupConfig & startupConfig)
{
    VALIDATE(false, null_str);
}
#endif
