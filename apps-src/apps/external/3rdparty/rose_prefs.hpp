/* $Id: preferences.hpp 47642 2010-11-21 13:58:27Z mordante $ */
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

/** @file */

#ifndef LIBROSE_ROSE_PREFS_HPP_INCLUDED
#define LIBROSE_ROSE_PREFS_HPP_INCLUDED


#include "config.hpp"
#include "rose_filesystem_dll.hpp"
#include "rose_thread.hpp"

class LIB3RDPARTY_DECL trose_prefs
{
public:
	trose_prefs();

	// is aplt::tapplet's member, must implte below tow copy-constructor
	trose_prefs(const trose_prefs& that);
	trose_prefs &operator=(const trose_prefs & that);

	virtual ~trose_prefs();

	virtual void load_prefs(const std::string& prefs_file);
	virtual void write_prefs();

	void set_bool(const std::string& key, bool value);
	void set_int(const std::string &key, int value);
	void set_int64(const std::string &key, int64_t value);
	// why not support set_double? -- look at preferences.hpp
	void set_str(const std::string &key, const std::string &value);

	void set_child(const std::string& key, const config& val);
	const config& get_child(const std::string& key);

	void erase(const std::string& key);

	std::string get_str(const std::string& key) const;
	bool get_bool(const std::string &key, bool def) const;
	int get_int(const std::string &key, int def) const;
	uint32_t get_uint32(const std::string &key, uint32_t def) const;
	int64_t get_int64(const std::string &key, int64_t def) const;
	double get_double(const std::string &key, double def) const;

	const config& cfg() const { return cfg_; }
	config& mutable_cfg() { return cfg_; }

	void clear();

	bool operator==(const trose_prefs& that) const;
	// bool operator!=(const trose_prefs& that) const { return !operator==(that); }

protected:
	bool did_read_preferences_dat(tfile& file, int64_t fsize, bool bak);
	bool did_pre_write_preferences_dat();
	bool did_write_preferences_dat(tfile& file);
	void resize_data(int size, int vsize);

	void copy_data(const trose_prefs& from);

public:
	threading::mutex mutex;

protected:
	config cfg_;

	std::string prefs_file_;
	const std::string signature_tag_;
	char* data_;
	int data_size_;
	int data_vsize_;
};

#endif // LIBROSE_ROSE_PREFERENCES_HPP_INCLUDED
