/* $Id: preferences.cpp 47642 2010-11-21 13:58:27Z mordante $ */
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

/**
 *  @file
 *  Get and set user-preferences.
 */

#define GETTEXT_DOMAIN "rose-lib"

#include "rose_global.hpp"

#include "rose_prefs.hpp"
#include <SDL_thread.h>
#include "rose_exception.hpp"
#include "aplt_common.hpp"

using namespace std::placeholders;

trose_prefs::trose_prefs()
	: cfg_()
	, signature_tag_("signature")
	, data_(nullptr)
	, data_size_(0)
	, data_vsize_(0)
{
}

trose_prefs::trose_prefs(const trose_prefs& that)
	: cfg_(that.cfg_)
	, prefs_file_(that.prefs_file_)
	, signature_tag_(that.signature_tag_)
	, data_(nullptr)
	, data_size_(0)
	, data_vsize_(0)
{
	copy_data(that);
}

trose_prefs& trose_prefs::operator=(const trose_prefs & that)
{
	cfg_ = that.cfg_;
	prefs_file_ = that.prefs_file_;

	copy_data(that);
	return *this;
}

trose_prefs::~trose_prefs()
{
	if (data_ != nullptr) {
		free(data_);
	}
}

bool trose_prefs::did_read_preferences_dat(tfile& file, int64_t fsize, bool bak)
{
	fsize = file.read_2_data();
	std::string stream;
	stream.assign(file.data, fsize);
	try {
		aplt::read_config(stream, cfg_);
		// read(cfg_, stream);
		const config& signature_cfg = cfg_.child(signature_tag_.c_str());
		if (!signature_cfg) {
			// exist preferences, but isn't valid
			cfg_.clear();
		} else {
			if (!cfg_.empty()) {
				resize_data(fsize, 0);
				memcpy(data_, file.data, fsize);
				data_vsize_ = fsize;
			}
		}
	// } catch (twml_exception& ) {
	} catch (...) {
		cfg_.clear();
	}

	return !cfg_.empty();
}

// nposm: write fail. delete this file
//
bool trose_prefs::did_pre_write_preferences_dat()
{
	std::stringstream out;
	// wirte full prefs
	try {
		aplt::write_config(out, cfg_);
		// write(out, cfg_);

	} catch (...) {
		// error writing to preferences file '$get_prefs_file()'
		return false; 
	}

	if (data_vsize_ == out.str().size()) {
		if (memcmp(data_, out.str().c_str(), data_vsize_) == 0) {
			// no change, do nothing.
			return false;
		}
	}

	data_vsize_ = out.str().size();
	if (data_vsize_ != 0) {
		resize_data(data_vsize_, 0);
		memcpy(data_, out.str().c_str(), data_vsize_);
	}

	return true;
}

bool trose_prefs::did_write_preferences_dat(tfile& file)
{
	if (data_vsize_ != 0) {
		posix_fwrite(file.fp, data_, data_vsize_);
	}
	return true;
}

void trose_prefs::clear()
{
	VALIDATE_IN_MAIN_THREAD();

	{
		threading::lock lock(mutex);
		cfg_.clear();
	}
	prefs_file_.clear();

	if (data_ != nullptr) {
		free(data_);
		data_ = nullptr;
	}
	data_vsize_ = 0;
	data_size_ = 0;
}

void trose_prefs::resize_data(int size, int vsize)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > data_size_) {
		char* tmp = (char*)malloc(size);
		if (data_ != nullptr) {
			if (vsize) {
				memcpy(tmp, data_, vsize);
			}
			free(data_);
		}
		data_ = tmp;
		data_size_ = size;
	}
}

bool trose_prefs::operator==(const trose_prefs& that) const
{
	if (&cfg_ != &that.cfg_) return false;
	if (data_ != that.data_) return false;
	VALIDATE(data_size_ == data_size_, null_str);
	VALIDATE(data_vsize_ == data_vsize_, null_str);
	return true;
}

void trose_prefs::copy_data(const trose_prefs& from)
{
	VALIDATE(data_ == nullptr, null_str);
	VALIDATE(data_size_ == data_size_, null_str);
	VALIDATE(data_vsize_ == data_vsize_, null_str);

	if (from.data_ != nullptr) {
		VALIDATE(from.data_vsize_ > 0, null_str);
		VALIDATE(from.data_size_ >= from.data_vsize_, null_str);

		char* tmp = (char*)malloc(from.data_size_);
		memcpy(tmp, from.data_, from.data_vsize_);

		data_ = tmp;
		data_size_ = from.data_size_;
		data_vsize_ = from.data_vsize_;
	}
}

void trose_prefs::load_prefs(const std::string& prefs_file)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(!prefs_file.empty(), null_str);

	prefs_file_ = prefs_file;
	{
		tbakreader file(prefs_file_, true, std::bind(&trose_prefs::did_read_preferences_dat, this, _1, _2, _3));
		file.read();
	}
	if (!cfg_.has_child(signature_tag_.c_str())) {
		VALIDATE(cfg_.empty(), null_str);
		VALIDATE(data_ == nullptr && data_vsize_ == 0 && data_size_ == 0, null_str);
		cfg_.add_child(signature_tag_.c_str());
		// Although cfg_ are not in agreement with data_, it is not a big deal.
		// Once cfg_ is changed later, it will make data_ consistent with cfg_ when saving.
	}
}

void trose_prefs::write_prefs()
{
	VALIDATE(!prefs_file_.empty(), null_str);

	tbakwriter bakfile(prefs_file_, backup_on_idle, std::bind(&trose_prefs::did_write_preferences_dat, this, _1), std::bind(&trose_prefs::did_pre_write_preferences_dat, this));
	bakfile.write();
}

void trose_prefs::set_bool(const std::string& key, bool value)
{
	VALIDATE_IN_MAIN_THREAD();
	if (cfg_.has_attribute(key) && cfg_[key].to_bool() == value) {
		// if key isn't existed in prefs, will create it always.
		// because when key does not existed, I don't know what the default value is.
		return;
	}

	{
		threading::lock lock(mutex);
		cfg_[key].from_bool(value);
	}
	write_prefs();
}

void trose_prefs::set_int(const std::string &key, int value)
{
	VALIDATE_IN_MAIN_THREAD();
	if (cfg_.has_attribute(key) && cfg_[key].to_int() == value) {
		// if key isn't existed in prefs, will create it always.
		// because when key does not existed, I don't know what the default value is.
		return;
	}

	{
		threading::lock lock(mutex);
		cfg_[key].from_int(value);
	}
	write_prefs();
}

void trose_prefs::set_int64(const std::string &key, int64_t value)
{
	VALIDATE_IN_MAIN_THREAD();
	if (cfg_.has_attribute(key) && cfg_[key].to_int64() == value) {
		// if key isn't existed in prefs, will create it always.
		// because when key does not existed, I don't know what the default value is.
		return;
	}
	{
		threading::lock lock(mutex);
		cfg_[key].from_int64(value);
	}
	write_prefs();
}

void trose_prefs::set_str(const std::string &key, const std::string &value)
{
	VALIDATE_IN_MAIN_THREAD();

	if (cfg_.has_attribute(key) && cfg_[key].str() == value) {
		// if key isn't existed in prefs, will create it always.
		// because when key does not existed, I don't know what the default value is.
		return;
	}

	{
		threading::lock lock(mutex);
		cfg_[key].from_string(value, true);
	}
	write_prefs();
}
/*
void clear(const std::string& key)
{
	VALIDATE_IN_MAIN_THREAD();
	prefs.recursive_clear_value(key);
}
*/
void trose_prefs::set_child(const std::string& key, const config& val) 
{
	VALIDATE_IN_MAIN_THREAD();

	{
		threading::lock lock(mutex);
		cfg_.clear_children(key);
		cfg_.add_child(key, val);
	}
	write_prefs();
}

const config& trose_prefs::get_child(const std::string& key)
{
	return cfg_.child(key);
}

void trose_prefs::erase(const std::string& key) 
{
	VALIDATE_IN_MAIN_THREAD();
	if (!cfg_.has_attribute(key)) {
		return;
	}
	{
		threading::lock lock(mutex);
		cfg_.remove_attribute(key);
	}
	write_prefs();
}

std::string trose_prefs::get_str(const std::string& key) const
{
	if (!cfg_.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return null_str;
	}
	return cfg_[key];
}

bool trose_prefs::get_bool(const std::string &key, bool def) const
{
	if (!cfg_.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return cfg_[key].to_bool(def);
}

int trose_prefs::get_int(const std::string &key, int def) const
{
	if (!cfg_.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return cfg_[key].to_int(def);
}

uint32_t trose_prefs::get_uint32(const std::string &key, uint32_t def) const
{
	if (!cfg_.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return cfg_[key].to_unsigned(def);
}

int64_t trose_prefs::get_int64(const std::string &key, int64_t def) const
{
	if (!cfg_.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return cfg_[key].to_int64(def);
}

double trose_prefs::get_double(const std::string &key, double def) const
{
	if (!cfg_.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return cfg_[key].to_double(def);
}
