/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "rose-lib"

#include "rose_latex.hpp"
#include "rose_config.hpp"
#include <list>

#include <openssl/mem.h>
#include <openssl/siphash.h>

namespace latex {

const std::string header_tag = "<__latex&/+>";
const std::string tail_tag = "</#$%latex__>";

static bool enabled = false;

void enable_latex()
{
	enabled = true;
}

std::string doc_to_doc2(const std::string& doc)
{
	VALIDATE(enabled, null_str);
	VALIDATE(!doc.empty(), null_str);
	std::stringstream ss;
	ss << header_tag << doc << tail_tag;

	return ss.str();
}

std::string doc2_to_doc(const std::string& doc2)
{
	VALIDATE(enabled, null_str);
	std::string doc;
	bool ret = is_doc2(doc2, &doc);
	VALIDATE(ret, null_str);

	return doc;
}

bool is_doc2(const std::string& str, std::string* doc)
{
	if (!enabled) {
		return false;
	}

	int header_size = header_tag.size();
	int tail_size = tail_tag.size();

	int s = str.size();
	if (s < header_size + tail_size + 1) {
		return false;
	}

	if (memcmp(str.c_str(), header_tag.c_str(), header_size) != 0) {
		return false;
	}

	if (memcmp(str.c_str() + s - tail_size, tail_tag.c_str(), tail_size) != 0) {
		return false;
	}

	if (doc != nullptr) {
		*doc = str.substr(header_size, s - header_size - tail_size);
	}

	return true;
}

surface doc2_to_surf(const std::string& doc2, int max_width)
{
	VALIDATE(enabled, null_str);
	VALIDATE(max_width == nposm || max_width > 0, null_str);

	const std::string doc = doc2_to_doc(doc2);
	return doc_to_surf(doc, max_width);
}


//
// pdf cache
//

// This caching solution comes from 'class cache_type' in liborse/image.cpp
const int hash_table_size = 200; // store up to 50(200/4) pdf

struct hash_node {
	size_t hash;
	size_t hash1;
	int index;
};

struct tpdf_cache_item
{
	tpdf_cache_item()
		: item()
		, pos_in_hash_table(-1)
		// position(dummy_list.end())
	{}

	surface item;
	int pos_in_hash_table;
	std::list<int>::iterator position;
};

class tpdf_cache
{
public:
	tpdf_cache(bool clear_cookie = true) :
			cache_max_size_(hash_table_size / 4),
			lru_list_(),
			content_(),
			clear_cookie_(clear_cookie)
	{
		// content_.resize(cache_max_size_);
		content_ = new tpdf_cache_item[cache_max_size_];

		for (int index = cache_max_size_ - 1; index >= 0; index --) {
			lru_list_.push_front(index);
			content_[index].position = lru_list_.begin();
		}
		for (int index = 0; index < hash_table_size; index ++) {
			hash_table_[index].index = -1;
		}

		union {
			uint8_t bytes[16];
			uint64_t words[2];
		} key;

		for (unsigned i = 0; i < 16; i++) {
			key.bytes[i] = i;
		}
		sip_hash_key[0] = key.words[0];
		sip_hash_key[1] = key.words[1];

	}
	~tpdf_cache()
	{
		delete []content_;
	}

	int cache_max_size() const { return cache_max_size_; }

	void flush(bool force = false)
	{ 
		if (force || clear_cookie_) {
			for (int index = cache_max_size_ - 1; index >= 0; index --) {
				tpdf_cache_item& elt = content_[index];
				if (elt.pos_in_hash_table != -1) {
					hash_table_[elt.pos_in_hash_table].index = -1;
					elt.item = NULL;
				}
				elt.pos_in_hash_table = -1;
			}
		}
		verify_pos();
	}
	int add(const surface& item, size_t hash, size_t hash1);

	void verify_pos() const;
public:
	uint64_t sip_hash_key[2];

	int cache_max_size_;
	bool clear_cookie_;
    std::list<int> lru_list_;
	// std::vector<cache_item<T> > content_;
	tpdf_cache_item* content_;
	hash_node hash_table_[hash_table_size];
};

void tpdf_cache::verify_pos() const
{
	if (game_config::os != os_windows) {
		// return;
	}

	int valid_in_locator_table = 0;
	int valid_in_content = 0;

	std::set<int> table_indexs;
	for (int index = 0; index < hash_table_size; index ++) {
		if (hash_table_[index].index != -1) {
			valid_in_locator_table ++;

			table_indexs.insert(index);
		}
	}

	for (int index = cache_max_size_ - 1; index >= 0; index --) {
		tpdf_cache_item& elt = content_[index];
		if (elt.pos_in_hash_table != -1) {
			VALIDATE(table_indexs.count(elt.pos_in_hash_table) != 0, null_str);
			valid_in_content ++;
		}
	}
	VALIDATE(valid_in_locator_table == valid_in_content, null_str);
}

int tpdf_cache::add(const surface& item, size_t hash, size_t hash1)
{
	// in hash_table, find a empty position.
	size_t pos = hash % hash_table_size;
	while (hash_table_[pos].index != -1) {
		if (++ pos == hash_table_size) {
			pos = 0;
		}
	}

	// calcuate index of content_
	int index = lru_list_.back();

	tpdf_cache_item& elt = content_[index];
	if (elt.pos_in_hash_table != -1) {
		hash_table_[elt.pos_in_hash_table].index = -1;
	}
	elt.item = item;
	elt.pos_in_hash_table = pos;

	lru_list_.erase(elt.position);
	lru_list_.push_front(index);
	elt.position = lru_list_.begin();

	// fill (hash,hash1,index)
	hash_table_[pos].hash = hash;
	hash_table_[pos].hash1 = hash1;
	hash_table_[pos].index = index;

	return index;
}

tpdf_cache pdf_cache;

void clear_cache()
{
	pdf_cache.flush(true);
}

tlocator::tlocator(const std::string& tex)
	: tex_(tex)
{
	hash_ = OPENSSL_hash32(tex.c_str(), tex.size());
	hash1_ = SIPHASH_24(pdf_cache.sip_hash_key, (const uint8_t*)tex.c_str(), tex.size());
}

int tlocator::in_cache() const
{
	tpdf_cache& cache = pdf_cache;

	int stop_after_invalids = 4;

	hash_node* hash_table = cache.hash_table_;

	size_t pos = hash_ % hash_table_size;
	do {
		while (hash_table[pos].index != -1) {
			hash_node* node = hash_table + pos;
			if (node->hash == hash_ && node->hash1 == hash1_) {
				return node->index;
			}
			if (++ pos == hash_table_size) {
				pos = 0;
			}
		}
		if (++ pos == hash_table_size) {
			pos = 0;
		}
	} while (-- stop_after_invalids);
	return -1;
}

const surface& tlocator::locate_in_cache(int index) const
{
	tpdf_cache& cache = pdf_cache;
	VALIDATE(index >= 0 && index < pdf_cache.cache_max_size(), null_str);

	std::list<int>& lru_list = cache.lru_list_;
	tpdf_cache_item& elt = cache.content_[index];
	if (index != lru_list.front()) {
		lru_list.erase(elt.position);
		lru_list.push_front(index);
		elt.position = lru_list.begin();
	}
	return elt.item;
}

void tlocator::add_to_cache(const surface& surf) const
{
	VALIDATE(!tex_.empty(), null_str);

	pdf_cache.add(surf, hash_, hash1_);
	pdf_cache.verify_pos();
}

}