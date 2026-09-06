/* $Id: gettext.hpp 46599 2010-09-19 11:59:47Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_GETTEXT_HPP_INCLUDED
#define LIBROSE_GETTEXT_HPP_INCLUDED

/**
 * How to use gettext for wesnoth source files:
 * -# include this header file in the .cpp file
 * -# make sure, that the source file is listed in the respective POTFILES.in
 *    for the textdomain, in the case of rose-lib it is this file:
 *    po/rose-lib/POTFILES.in
 * -# add the following include to set the correct textdomain, in this example
 *    rose-lib (not required for the domain 'rose', required for all
 *    other textdomains).
 *    @code
 *    #define GETTEXT_DOMAIN "rose-lib"
 *    @endcode
 *
 * This should be all that is required to have your strings that are marked
 * translatable in the po files and translated ingame. So you at least have
 * to mark the strings translatable, too. ;)
 */

// gettext-related declarations

#include <libintl.h>
#include "tstring.hpp"
#include <sstream>
#include <SDL_stdinc.h>

#ifdef setlocale
// Someone in libintl world decided it was a good idea to define a "setlocale" macro.
#undef setlocale
#endif

LIB3RDPARTY_DECL const char* dsgettext(const char * domainname, const char *msgid);
LIB3RDPARTY_DECL const char* sngettext(const char *singular, const char *plural, int n);
LIB3RDPARTY_DECL const char* dsngettext(const char * domainname, const char *singular, const char *plural, int n);

extern LIB3RDPARTY_DECL char def_textdomain[];

#ifdef GETTEXT_DOMAIN
# define _(String) dsgettext(GETTEXT_DOMAIN, String)
# define _n(String1,String2,Int) dsngettext(String1,String2,Int)
#else
# define _(String) dsgettext(def_textdomain, String)
# define _n(String1,String2,Int) sngettext(String1,String2,Int)
#endif

#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/** Handy wrappers around interpolate_variables_into_string and gettext. */
// std::string vgettext(const char* msgid, const utils::string_map& symbols);
LIB3RDPARTY_DECL std::string vgettext(const char* domain
		, const char* msgid
		, const utils::string_map& symbols);

LIB3RDPARTY_DECL std::string vngettext(const char*, const char*, int, const utils::string_map&);

/**
 * @todo Convert all functions.
 *
 * All function in this file should have an overloaded version with a domain
 * and probably convert all callers to use the macro instead of directly calling
 * the function.
 */

#ifdef GETTEXT_DOMAIN
#define vgettext2(msgid, symbols)	vgettext(GETTEXT_DOMAIN, msgid, symbols)
#else
#define vgettext2(msgid, symbols)	vgettext(def_textdomain, msgid, symbols)
#endif

class tintl_buildings
{
public:
    tintl_buildings()
        : buildings(nullptr)
        , count(0)
    {
        buildings = SDL_IntlGetBindings(&count);
    }

    ~tintl_buildings()
    {
        if (buildings != NULL) {
            SDL_free(buildings);
        }
    }

    std::string intl_building_to_string(const SDL_IntlBinding& binding, const std::string& title) const
    {
        std::stringstream ss;
        ss << title;
        ss << "\n  domainname: " << binding.domainname;
        ss << "\n  codeset: " << binding.codeset;
        ss << "\n  dirname: " << binding.dirname;
        return ss.str();
    }

    std::string to_string() const
    {
        std::stringstream ss;
        char title[32];
        for (int at = 0; at < count; at ++) {
            const SDL_IntlBinding& building = buildings[at];
            SDL_snprintf(title, sizeof(title), "textdomain[%i/%i]", at + 1, count);
            if (at != 0) {
                ss << "\n";
            }
            ss << intl_building_to_string(building, title);
        }

        return ss.str();
    }

public:
    SDL_IntlBinding* buildings;
    int count;
};

class tintl_loaded_l10nfiles
{
public:
    tintl_loaded_l10nfiles()
        : l10nfiles(nullptr)
        , count(0)
    {
        l10nfiles = SDL_IntlGetLoaded_l10nfiles(&count);
        // for (int at = 0; at < count; at ++) {
        //    const SDL_IntlLoaded_l10nfile& l10nfile = l10nfiles[at];
        //    VALIDATE(l10nfile_set.count(l10nfile.filename) == 0, null_str);
        //    l10nfile_set.insert(l10nfile.filename);
        // }
    }

    ~tintl_loaded_l10nfiles()
    {
        if (l10nfiles != NULL) {
            SDL_free(l10nfiles);
        }
    }

    std::string intl_loaded_l10nfile_to_string(const SDL_IntlLoaded_l10nfile& binding, const std::string& title) const
    {
        std::stringstream ss;
        ss << title;
        ss << "\n  filename: " << binding.filename;
        ss << "\n  decided: " << binding.decided;
        ss << "\n  mmap_size: " << binding.mmap_size;
        ss << "\n  successor_count: " << binding.successor_count;
        return ss.str();
    }

    std::string to_string() const
    {
        std::stringstream ss;
        char title[32];
        for (int at = 0; at < count; at ++) {
            const SDL_IntlLoaded_l10nfile& l10nfile = l10nfiles[at];
            SDL_snprintf(title, sizeof(title), "loaded_l10nfiles[%i/%i]", at + 1, count);
            if (at != 0) {
                ss << "\n";
            }
            ss << intl_loaded_l10nfile_to_string(l10nfile, title);
        }

        return ss.str();
    }

public:
    SDL_IntlLoaded_l10nfile* l10nfiles;
    int count;
    // std::set<std::string> l10nfile_set;
};

// l10n: Localization
// i18n: Internationalization

// For safety, GETTEXT_DOMAIN must not be defined in gettext.cpp. 
// Therefore, do not place the implementation related to 'namespace i18n' in gettext.cpp;
// instead, put it in rose_string_utils_dll.cpp.
namespace i18n {

enum {msgid_notempty_and_utf8str, msgid_empty_or_utf8str, 
    msgid_greater_than_0, msgid_greater_than_equal_to_0, msgid_empty_or_greater_than_0, 
    msgid_isvalid_normal_id_or_var_name,
    msgid_open_file, msgid_save_file_as_from_no, msgid_save_file_as,
    // 2str
    msgid_confirm_delete_2str,
    // 3str
    msgid_load_file_fail,
    // 1str+2int
    msgid_value_range
};
LIB3RDPARTY_DECL std::string freq_msgstr(int msgid);
LIB3RDPARTY_DECL std::string freq_msgstr_2str(int msgid, const std::string& str1, const std::string& str2);
LIB3RDPARTY_DECL std::string freq_msgstr_3str(int msgid, const std::string& str1, const std::string& str2, const std::string& str3);
LIB3RDPARTY_DECL std::string freq_msgstr_1str_2int(int msgid, const std::string& str, int int1, int int2);

};

#endif
