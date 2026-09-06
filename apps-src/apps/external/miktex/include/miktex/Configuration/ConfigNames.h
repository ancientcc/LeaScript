/**
 * @file miktex/Configuration/ConfigNames.h
 * @author Christian Schenk
 * @brief Configuration names
 *
 * @copyright Copyright © 2017-2022 Christian Schenk
 *
 * This file is part of the MiKTeX Configuration Library.
 *
 * The MiKTeX Configuration Library is licensed under GNU General Public License
 * version 2 or any later version.
 */

#pragma once

constexpr auto MIKTEX_CONFIG_SECTION_AUTO = "Auto";
constexpr auto MIKTEX_CONFIG_SECTION_BIBTEX = "BibTeX";
constexpr auto MIKTEX_CONFIG_SECTION_CORE = "Core";
constexpr auto MIKTEX_CONFIG_SECTION_CORE_FILETYPES = "Core.FileTypes";
constexpr auto MIKTEX_CONFIG_SECTION_GENERAL = "General";
constexpr auto MIKTEX_CONFIG_SECTION_MAKEBASE = "MakeBase";
constexpr auto MIKTEX_CONFIG_SECTION_MAKEFMT = "MakeFMT";
constexpr auto MIKTEX_CONFIG_SECTION_MAKEPK = "MakePk";
constexpr auto MIKTEX_CONFIG_SECTION_MAKETFM = "MakeTFM";
constexpr auto MIKTEX_CONFIG_SECTION_MPM = "MPM";
constexpr auto MIKTEX_CONFIG_SECTION_NONE = "";
constexpr auto MIKTEX_CONFIG_SECTION_SETUP = "Setup";
constexpr auto MIKTEX_CONFIG_SECTION_TEXANDFRIENDS = "TeXandFriends";
constexpr auto MIKTEX_CONFIG_SECTION_TEXJP = "TeXjp";

constexpr auto MIKTEX_CONFIG_VALUE_ALLOWEDSHELLCOMMANDS = "AllowedShellCommands[]";
constexpr auto MIKTEX_CONFIG_VALUE_ALLOW_UNRESTRICTED_SUPER_USER = "AllowUnrestrictedSuperUser";
constexpr auto MIKTEX_CONFIG_VALUE_ALLOWUNSAFEINPUTFILES = "AllowUnsafeInputFiles";
constexpr auto MIKTEX_CONFIG_VALUE_ALLOWUNSAFEOUTPUTFILES = "AllowUnsafeOutputFiles";
constexpr auto MIKTEX_CONFIG_VALUE_ALTEXTENSIONS = "AltExtensions[]";
constexpr auto MIKTEX_CONFIG_VALUE_AUTOADMIN = "AutoAdmin";
constexpr auto MIKTEX_CONFIG_VALUE_AUTOINSTALL = "AutoInstall";
constexpr auto MIKTEX_CONFIG_VALUE_COMMONLINKTARGETDIRECTORY = "CommonLinkTargetDirectory";
constexpr auto MIKTEX_CONFIG_VALUE_COMMONLOGDIRECTORY = "CommonLogDirectory";
constexpr auto MIKTEX_CONFIG_VALUE_COMMON_CONFIG = "CommonConfig";
constexpr auto MIKTEX_CONFIG_VALUE_COMMON_DATA = "CommonData";
constexpr auto MIKTEX_CONFIG_VALUE_COMMON_INSTALL = "CommonInstall";
constexpr auto MIKTEX_CONFIG_VALUE_COMMON_ROOTS = "CommonRoots";
constexpr auto MIKTEX_CONFIG_VALUE_CONFIG = "Config";
constexpr auto MIKTEX_CONFIG_VALUE_CREATEAUXDIRECTORY = "CreateAuxDirectory";
constexpr auto MIKTEX_CONFIG_VALUE_CREATEOUTPUTDIRECTORY = "CreateOutputDirectory";
constexpr auto MIKTEX_CONFIG_VALUE_CSTYLEERRORS = "CStyleErrors";
constexpr auto MIKTEX_CONFIG_VALUE_DESTDIR = "DestDir";
constexpr auto MIKTEX_CONFIG_VALUE_DOC_EXTENSIONS = "";
constexpr auto MIKTEX_CONFIG_VALUE_EDITOR = "Editor";
constexpr auto MIKTEX_CONFIG_VALUE_ENVVARS = "EnvVars[]";
constexpr auto MIKTEX_CONFIG_VALUE_EXTENSIONS = "Extensions[]";
constexpr auto MIKTEX_CONFIG_VALUE_FORCE_LOCAL_SERVER = "ForceLocalServer";
constexpr auto MIKTEX_CONFIG_VALUE_GUESS_INPUT_KANJI_ENCODING = "GuessInputKanjiEncoding";
constexpr auto MIKTEX_CONFIG_VALUE_GUI_FRAMEWORK = "GUIFramework";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_ADMIN_DIAGNOSE = "LastAdminDiagnose";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_ADMIN_MAINTENANCE = "LastAdminMaintenance";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_ADMIN_UPDATE = "LastAdminUpdate";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_ADMIN_UPDATE_CHECK = "LastAdminUpdateCheck";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_ADMIN_UPDATE_DB = "LastAdminUpdateDb";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_USER_DIAGNOSE = "LastUserDiagnose";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_USER_MAINTENANCE = "LastUserMaintenance";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_USER_UPDATE = "LastUserUpdate";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_USER_UPDATE_CHECK = "LastUserUpdateCheck";
constexpr auto MIKTEX_CONFIG_VALUE_LAST_USER_UPDATE_DB = "LastUserUpdateDb";
constexpr auto MIKTEX_CONFIG_VALUE_LOCAL_REPOSITORY = "LocalRepository";
constexpr auto MIKTEX_CONFIG_VALUE_MIKTEXDIRECT_ROOT = "MiKTeXDirectRoot";
constexpr auto MIKTEX_CONFIG_VALUE_NO_REGISTRY = "NoRegistry";
constexpr auto MIKTEX_CONFIG_VALUE_OTHER_COMMON_ROOTS = "OtherCommonRoots";
constexpr auto MIKTEX_CONFIG_VALUE_OTHER_USER_ROOTS = "OtherUserRoots";
constexpr auto MIKTEX_CONFIG_VALUE_PARSE_FIRST_LINE = "ParseFirstLine";
constexpr auto MIKTEX_CONFIG_VALUE_PATHS = "Paths[]";
constexpr auto MIKTEX_CONFIG_VALUE_PK_FN_TEMPLATE = "PKFnTemplate";
constexpr auto MIKTEX_CONFIG_VALUE_PREFER_MIKTEX_GHOSTSCRIPT = "PreferMiKTeXGhostscript";
constexpr auto MIKTEX_CONFIG_VALUE_PROXY_AUTH_REQ = "ProxyAuthReq";
constexpr auto MIKTEX_CONFIG_VALUE_PROXY_HOST = "ProxyHost";
constexpr auto MIKTEX_CONFIG_VALUE_PROXY_PORT = "ProxyPort";
constexpr auto MIKTEX_CONFIG_VALUE_REMOTE_REPOSITORY = "RemoteRepository";
constexpr auto MIKTEX_CONFIG_VALUE_REMOTE_SERVICE = "RemoteService_4727";
constexpr auto MIKTEX_CONFIG_VALUE_RENEW_FORMATS_ON_UPDATE = "RenewFormatsOnUpdate";
constexpr auto MIKTEX_CONFIG_VALUE_REPOSITORY_RELEASE_STATE = "RepositoryReleaseState";
constexpr auto MIKTEX_CONFIG_VALUE_REPOSITORY_TYPE = "RepositoryType";
constexpr auto MIKTEX_CONFIG_VALUE_SHARED_SETUP = "SharedSetup";
constexpr auto MIKTEX_CONFIG_VALUE_SHELLCOMMANDMODE = "ShellCommandMode";
constexpr auto MIKTEX_CONFIG_VALUE_STARTUP_FILE = "StartupFile";
constexpr auto MIKTEX_CONFIG_VALUE_TEMPDIR = "TempDir";
constexpr auto MIKTEX_CONFIG_VALUE_TRACE = "Trace";
constexpr auto MIKTEX_CONFIG_VALUE_UI_LANGUAGES = "UILanguages[]";
constexpr auto MIKTEX_CONFIG_VALUE_USERINFO_FILE = "UserInfoFile";
constexpr auto MIKTEX_CONFIG_VALUE_USERLINKTARGETDIRECTORY = "UserLinkTargetDirectory";
constexpr auto MIKTEX_CONFIG_VALUE_USERLOGDIRECTORY = "UserLogDirectory";
constexpr auto MIKTEX_CONFIG_VALUE_USER_CONFIG = "UserConfig";
constexpr auto MIKTEX_CONFIG_VALUE_USER_DATA = "UserData";
constexpr auto MIKTEX_CONFIG_VALUE_USER_INSTALL = "UserInstall";
constexpr auto MIKTEX_CONFIG_VALUE_USER_ROOTS = "UserRoots";
constexpr auto MIKTEX_CONFIG_VALUE_USE_PROXY = "UseProxy";
constexpr auto MIKTEX_CONFIG_VALUE_VERSION = "Version";
