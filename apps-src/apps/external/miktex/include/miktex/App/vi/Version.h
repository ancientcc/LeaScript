/**
 * @file miktex/App/vi/Version.h
 * @author Christian Schenk
 * @brief Library version
 *
 * @copyright Copyright © 2005-2022 Christian Schenk
 *
 * This file is part of the MiKTeX Application Framework.
 *
 * The MiKTeX Application Framework is licensed under GNU General Public License
 * version 2 or any later version.
 */

#pragma once

#include <miktex/App/config.h>

#include <string>
#include <vector>

#include <miktex/Core/LibraryVersion>

#define MIKTEX_APP_VI_BEGIN_NAMESPACE       \
    MIKTEX_APP_BEGIN_NAMESPACE              \
        namespace vi {

#define MIKTEX_APP_VI_END_NAMESPACE         \
        }                                   \
    MIKTEX_APP_END_NAMESPACE

MIKTEX_APP_VI_BEGIN_NAMESPACE;

class MIKTEXNOVTABLE Runtime
{

public:

    Runtime() = delete;
    Runtime(const Runtime& other) = delete;
    Runtime& operator=(const Runtime& other) = delete;
    Runtime(Runtime&& other) = delete;
    Runtime& operator=(Runtime&& other) = delete;
    ~Runtime() = delete;

    static MIKTEXAPPCEEAPI(std::string) GetName();
    static MIKTEXAPPCEEAPI(std::string) GetDescription();
    static MIKTEXAPPCEEAPI(int) GetInterfaceVersion();
    static MIKTEXAPPCEEAPI(MiKTeX::Core::VersionNumber) GetVersion();
    static MIKTEXAPPCEEAPI(std::vector<MiKTeX::Core::LibraryVersion>) GetDependencies();
};

class MIKTEXNOVTABLE Header
{

public:

    Header() = delete;
    Header(const Header& other) = delete;
    Header& operator=(const Header& other) = delete;
    Header(Header&& other) = delete;
    Header& operator=(Header&& other) = delete;
    ~Header() = delete;

    static std::string GetName()
    {
        return "MiKTeX Application Framework";
    }

    static std::string GetDescription()
    {
        return "MiKTeX application framework";
    }

    static int GetInterfaceVersion()
    {
        return 6;
    }

    static MiKTeX::Core::VersionNumber GetVersion()
    {
        return MiKTeX::Core::VersionNumber(4, 8, 0, 0);
    }
};

class MIKTEXNOVTABLE Version
{

public:

    Version() = delete;
    Version(const Version& other) = delete;
    Version& operator=(const Version& other) = delete;
    Version(Version&& other) = delete;
    Version& operator=(Version&& other) = delete;
    ~Version() = delete;

    static MiKTeX::Core::LibraryVersion GetLibraryVersion()
    {
        return MiKTeX::Core::LibraryVersion("miktex-app", Header::GetName(), Header::GetVersion().ToString(), Runtime::GetVersion().ToString());
    }
};

MIKTEX_APP_VI_END_NAMESPACE;
