/**
 * @file wrapper.cpp
 * @author Christian Schenk
 * @brief Wrap a main function
 *
 * @copyright Copyright © 2004-2023 Christian Schenk
 *
 * This file is free software.
 *
 * This file is licensed under GNU General Public License version 2 or any later
 * version.
 */

#if defined(HAVE_STDAFX_H)
#include "StdAfx.h"
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <iostream>

#include <cstdlib>

#if defined(MIKTEX_WINDOWS)
#include <Windows.h>
#include <VersionHelpers.h>
#endif

#include <miktex/Core/Exceptions>
#include <miktex/Util/StringUtil>
#include <miktex/App/Application>

#if defined(MIKTEX_WINDOWS)
#include <miktex/Core/win/ConsoleCodePageSwitcher>
#endif

#include <vector>
#include <SDL_log.h>
#include "../Programs/TeXAndFriends/luatex/source/lua/luatex-api.h"

#if !defined(stringify_)
#define stringify__(x) #x
#define stringify_(x) stringify__(x)
#endif

#if defined(APPTAG)
#define APPTAGSTR stringify_(APPTAG)
#endif

#if !defined(MAINFUNC)
#define MAINFUNC Main
#endif
/*
#if !defined(MAINFUNC)
#define MAINFUNC miktex_dvipdfmx_main2
#endif
*/
#if defined(CPLUSPLUSMAIN)
#define EXTERN_C
#else
#define EXTERN_C extern "C"
#endif

// EXTERN_C int MAINFUNC(int argc, char** argv);
EXTERN_C int MIKTEXCEECALL MAINFUNC(int argc, char** argv, const tluatex_hook* hook);
// extern int miktex_dvipdfmx_main2(int argc, char *argv[]);
       

// Keep the application object in the global scope (C functions might
// call exit())
static MiKTeX::App::Application app;

static std::string nameOfTheGame;

#if defined(main) && !defined(_UNICODE)
#undef main
#endif

#if defined(_UNICODE)
#define WRAPPER_MAIN wmain
#define WRAPPER_CHAR wchar_t
#else
#define WRAPPER_MAIN main
#define WRAPPER_CHAR char
#endif

#define T_(x) MIKTEXTEXT(x)

class ArgcArgv
{
public:
    ArgcArgv(const std::vector<std::string>& args) 
        : argc_(args.size())
    {
        argv_ = new char*[args.size() + 1];
        
        for (size_t i = 0; i < args.size(); ++i) {
            argv_[i] = new char[args[i].size() + 1];
            std::strcpy(argv_[i], args[i].c_str());
        }
        argv_[args.size()] = nullptr;
    }
    
    ~ArgcArgv()
    {
        if (argv_) {
            for (int i = 0; i < argc_; ++i) {
                delete[] argv_[i];
            }
            delete[] argv_;
        }
    }
    
    ArgcArgv(const ArgcArgv&) = delete;
    ArgcArgv& operator=(const ArgcArgv&) = delete;
    
    ArgcArgv(ArgcArgv&) = delete;
  
    int argc() const { return argc_; }
    char** argv() const { return argv_; }
    
    std::pair<int, char**> get() const { 
        return {argc_, argv_}; 
    }

private:
    int argc_;
    char** argv_;
};

bool IsWindows10OrGreater2()
{
    // Very strange, even though the system is Windows 11, 'IsWindows10OrGreater' still returns false. 
    // However, on the same computer, in the MiKTeX project, this function returns true.
    // return IsWindows10OrGreater();

    return true;
}

// int MIKTEXCEECALL WRAPPER_MAIN(int argc, WRAPPER_CHAR* argv[])
// int miktex_dvipdfmx_main(const std::vector<std::string>& _args)
int miktex_luahbtex_main(const std::vector<std::string>& _args, const tluatex_hook* _hook)
{
  // ArgcArgv converter(args);
  // int argc = converter.argc();
  // char** argv = converter.argv();
    SDL_Log("miktex_luahbtex_main(...) start");

#if defined(MIKTEX_WINDOWS)
    if (!IsWindows10OrGreater2())
    {
        std::cerr << T_("MiKTeX requires Windows 10 (or greater): https://miktex.org/announcement/legacy-windows-deprecation") << std::endl;
        return 1;
    }
    MiKTeX::Core::ConsoleCodePageSwitcher cpSwitcher;
#endif
    try
    {
/*
#if defined(MIKTEX_WINDOWS)
        std::vector<std::string> utf8args;
        utf8args.reserve(argc);
#endif
        std::vector<char*> args;
        args.reserve(argc + 1);
        for (int idx = 0; idx < argc; ++idx)
        {
#if defined(MIKTEX_WINDOWS)
#if defined(_UNICODE)
            utf8args.push_back(MiKTeX::Util::StringUtil::WideCharToUTF8(argv[idx]));
#else
            utf8args.push_back(MiKTeX::Util::StringUtil::AnsiToUTF8(argv[idx]));
#endif
            // FIXME: eliminate const cast
            args.push_back(const_cast<char*>(utf8args[idx].c_str()));
#else
            args.push_back(argv[idx]);
#endif
        }
*/
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

        nameOfTheGame = args[0];

        app.Init(args);

#if defined(APPTAGSTR)
        app.GetSession()->PushBackAppName(APPTAGSTR);
#endif

#if defined(DISABLE_INSTALLER)
        app.EnableInstaller(MiKTeX::Configuration::TriState::False);
#endif

#if defined(BEQUIET)
        app.SetQuietFlag(true);
#endif

        int exitCode = MAINFUNC(args.size() - 1, &args[0], _hook);

        app.Finalize2(exitCode);

#if defined(MIKTEX_WINDOWS)
        if (exitCode == 0 && !IsWindows10OrGreater())
        {
            std::cerr
                << "\n"
                << "\n"
                << T_("MiKTeX requires Windows 10 (or greater): https://miktex.org/announcement/legacy-windows-deprecation") << std::endl;
        }
#endif

        return exitCode;
    }
    catch (const MiKTeX::Core::MiKTeXException& ex)
    {
        SDL_Log("{android}miktex_luahbtex_main, catch MiKTeXException: what(%s) info(%s)", ex.what(), ex.GetInfo().ToString().c_str());
        app.Sorry(nameOfTheGame, ex);
        app.Finalize2(EXIT_FAILURE);
        ex.Save();
        return EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        app.Sorry(nameOfTheGame, ex);
        app.Finalize2(EXIT_FAILURE);
        return EXIT_FAILURE;
    }
    catch (int exitCode)
    {
        if (exitCode != 0) {
            int ii = 0;
        }
        app.Finalize2(exitCode);
        return exitCode;
    }
}
