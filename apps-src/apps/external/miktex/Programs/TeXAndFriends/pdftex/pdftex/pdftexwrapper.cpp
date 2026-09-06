/**
 * @file alias.cpp
 * @author Christian Schenk
 * @brief Function wrapper
 *
 * @copyright Copyright © 1991-2023 Christian Schenk
 *
 * This file is free software.
 *
 * This file is licensed under GNU General Public License version 2 or any later
 * version.
 */

#define FUNC MiKTeX_PDFTEX

#include <iostream>
#include <memory>

// #if defined(MIKTEX_WINDOWS)
#ifdef _WIN32
#include <Windows.h>
#include <VersionHelpers.h>
#endif

#include <miktex/Core/Exceptions>
#include <miktex/Core/FileType>
#include <miktex/Core/Session>
#include <miktex/Definitions>
#include <miktex/Util/StringUtil>

#if defined(MIKTEX_WINDOWS)
#include <miktex/Core/win/ConsoleCodePageSwitcher>
#endif

#if !defined(FUNC)
#define FUNC c4pmain
#endif

#define T_(x) MIKTEXTEXT(x)

extern "C" int MIKTEXCEECALL FUNC(int argc, char* argv[]);

#if defined(_UNICODE)
#define WRAPPER_MAIN wmain
#define WRAPPER_CHAR wchar_t
#else
#define WRAPPER_MAIN main
#define WRAPPER_CHAR char
#endif

bool IsWindows10OrGreater2()
{
    // Very strange, even though the system is Windows 11, 'IsWindows10OrGreater' still returns false. 
    // However, on the same computer, in the MiKTeX project, this function returns true.
    // return IsWindows10OrGreater();

    return true;
}

// int MIKTEXCEECALL WRAPPER_MAIN(int argc, WRAPPER_CHAR* argv[])
int miktex_pdftex_main(const std::vector<std::string>& _args)
{
#if defined(MIKTEX_WINDOWS)
    if (!IsWindows10OrGreater2())
    {
        std::cerr << T_("MiKTeX requires Windows 10 (or greater): https://miktex.org/announcement/legacy-windows-deprecation") << std::endl;
        return 1;
    }
    MiKTeX::Core::ConsoleCodePageSwitcher cpSwitcher;
#endif
    int argc = _args.size();
    // std::vector<std::string> utf8args;
    // utf8args.reserve(argc);

    std::vector<char*> args;
    args.reserve(argc + 1);
    for (std::vector<std::string>::const_iterator it = _args.begin(); it != _args.end(); ++ it) {
        const std::string& arg = *it;
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);
    int exitCode = FUNC(argc, &args[0]);
#if defined(MIKTEX_WINDOWS)
    if (exitCode == 0 && !IsWindows10OrGreater2())
    {
        std::cerr
            << "\n"
            << "\n"
            << T_("MiKTeX requires Windows 10 (or greater): https://miktex.org/announcement/legacy-windows-deprecation") << std::endl;
    }
#endif
    return exitCode;
}
