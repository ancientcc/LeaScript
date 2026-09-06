/*
   Copyright (C) 2003 - 2015 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Definitions for the interface to Wesnoth Markup Language (WML).
 *
 * This module defines the interface to Wesnoth Markup Language (WML).  WML is
 * a simple hierarchical text-based file format.  The format is defined in
 * Wiki, under BuildingScenariosWML
 *
 * All configuration files are stored in this format, and data is sent across
 * the network in this format.  It is thus used extensively throughout the
 * game.
 */

#ifndef LIBROSE_CONFIG_HPP_INCLUDED
#define LIBROSE_CONFIG_HPP_INCLUDED

#include "rose_global.hpp"

#include <map>
#include <iosfwd>
#include <vector>

// #include <boost/exception/exception.hpp>
// #include <boost/range/iterator_range.hpp>

#include "exceptions.hpp"
#include "tstring.hpp"
#include <json/json.h>

// class config;
struct tconfig_implementation;
class vconfig;
struct lua_State;

#ifdef __APPLE__
	#include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && TARGET_OS_IPHONE
#else
#define VERBOSE_CONFIG
#endif

/** A config object defines a single node in a WML file, with access to child nodes. */
class LIB3RDPARTY_DECL config
{
	friend LIB3RDPARTY_DECL bool operator==(const config& a, const config& b);
	friend struct tconfig_implementation;

	static config invalid;

	/**
	 * Raises an exception if @a this is not valid.
	 */
	void check_valid() const;

	/**
	 * Raises an exception if @a this or @a cfg is not valid.
	 */
	void check_valid(const config &cfg) const;

public:
	static const config empty_cfg;

	// Create an empty node.
	config();

	config(const config &);
	config &operator=(const config &);
/*
#ifdef HAVE_CXX11
	config(config &&);
	config &operator=(config &&);
#endif
*/
	/**
	 * Creates a config object with an empty child of name @a child.
	 */
	explicit config(const std::string &child);

	~config();

	// Verifies that the string can be used as a tag name
	static bool valid_tag(const std::string& name);

	// Verifies that the string can be used as an attribute name
	static bool valid_attribute(const std::string& name);

	explicit operator bool() const
	{ return this != &invalid; }

	typedef std::vector<config*> child_list;
	typedef std::map<std::string,child_list> child_map;

	struct const_child_iterator;

	struct child_iterator
	{
		typedef config value_type;
		typedef std::forward_iterator_tag iterator_category;
		typedef int difference_type;
		typedef config *pointer;
		typedef config &reference;
		typedef child_list::iterator Itor;
		explicit child_iterator(const Itor &i): i_(i) {}

		child_iterator &operator++() { ++i_; return *this; }
		child_iterator operator++(int) { return child_iterator(i_++); }
		child_iterator &operator--() { --i_; return *this; }
		child_iterator operator--(int) { return child_iterator(i_--); }

		config &operator*() const { return **i_; }
		config *operator->() const { return &**i_; }

		bool operator==(const child_iterator &i) const { return i_ == i.i_; }
		bool operator!=(const child_iterator &i) const { return i_ != i.i_; }

	private:
		Itor i_;
		friend struct const_child_iterator;
	};

	struct const_child_iterator
	{
		typedef config value_type;
		typedef std::forward_iterator_tag iterator_category;
		typedef int difference_type;
		typedef const config *pointer;
		typedef const config &reference;
		typedef child_list::const_iterator Itor;
		explicit const_child_iterator(const Itor &i): i_(i) {}
		const_child_iterator(const child_iterator &i): i_(i.i_) {}

		const_child_iterator &operator++() { ++i_; return *this; }
		const_child_iterator operator++(int) { return const_child_iterator(i_++); }
		const_child_iterator &operator--() { --i_; return *this; }
		const_child_iterator operator--(int) { return const_child_iterator(i_--); }

		const config &operator*() const { return **i_; }
		const config *operator->() const { return &**i_; }

		bool operator==(const const_child_iterator &i) const { return i_ == i.i_; }
		bool operator!=(const const_child_iterator &i) const { return i_ != i.i_; }

	private:
		Itor i_;
	};

	typedef std::pair<child_iterator,child_iterator> child_itors;
	typedef std::pair<const_child_iterator,const_child_iterator> const_child_itors;

	/**
	 * Variant for storing WML attributes.
	 * The most efficient type is used when assigning a value. For instance,
	 * strings "yes", "no", "true", "false" will be detected and stored as boolean.
	 * @note The blank variant is only used when querying missing attributes.
	 *       It is not stored in config objects.
	 */
	class LIB3RDPARTY_DECL attribute_value
	{
		friend class config;
		/// A wrapper for bool to get the correct streaming ("true"/"false").
		/// Most visitors can simply treat this as bool.
		class LIB3RDPARTY_DECL true_false
		{
			bool value_;
		public:
			explicit true_false(bool value = false) : value_(value) {}
			operator bool() const { return value_; }

			const std::string & str() const
			{ return value_ ? config::attribute_value::s_true :
			                  config::attribute_value::s_false; }
		};
		// friend std::ostream& operator<<(std::ostream &os, const true_false &v);

		/// A wrapper for bool to get the correct streaming ("yes"/"no").
		/// Most visitors can simply treat this as bool.
		class LIB3RDPARTY_DECL yes_no
		{
			bool value_;
		public:
			explicit yes_no(bool value = false) : value_(value) {}
			operator bool() const { return value_; }

			const std::string & str() const
			{ return value_ ? config::attribute_value::s_yes :
			                  config::attribute_value::s_no; }
		};
		// friend std::ostream& operator<<(std::ostream &os, const yes_no &v);

	public:
		/// Default implementation, but defined out-of-line for efficiency reasons.
		attribute_value();
		/// Default implementation, but defined out-of-line for efficiency reasons.
		~attribute_value();
		/// Default implementation, but defined out-of-line for efficiency reasons.
		attribute_value(const attribute_value &);
		/// Default implementation, but defined out-of-line for efficiency reasons.
		attribute_value& operator=(const attribute_value &);

		attribute_value& set_nposm();

		// Numeric assignments:
		attribute_value& from_bool(bool v);
		attribute_value& from_int(int v);
		attribute_value& from_int64(int64_t v);
		attribute_value& from_uint64(uint64_t v);
		attribute_value& from_double(double v);

		// String assignments:
		attribute_value &from_string(const std::string &v, bool keep_str);
		attribute_value &operator=(const char *v)   { return operator=(std::string(v)); }
		attribute_value &operator=(const std::string &v);
		attribute_value &operator=(const t_string &v);

		// Extracting as a specific type:
		bool to_bool(bool def = false) const;
		bool to_bool2(bool def = false) const;
		int to_int(int def = 0) const;
		int to_int2(int def = 0) const;
		int64_t to_int64(int64_t def = 0) const;
		int64_t to_int64_2(int64_t def = 0) const;
		uint32_t to_unsigned(uint32_t def = 0) const;
		size_t to_size_t(size_t def = 0) const;
		// replace with to_int64()
		// time_t to_time_t(time_t def = 0) const; 
		double to_double(double def = 0.) const;
		double to_double2(double def = 0.) const;
		std::string str() const;
		const std::string& str_ref() const;
		t_string t_str() const;
		const t_string& t_str_ref() const;

		// Implicit conversions:
		operator int() const { return to_int(); }
		operator std::string() const { return str(); }
		operator t_string() const { return t_str(); }

		/// Tests for an attribute that was never set.
		bool blank() const;
		/// Tests for an attribute that either was never set or was set to "".
		bool empty() const;


		// Comparisons:
		bool operator==(const attribute_value &other) const;
		bool operator!=(const attribute_value &other) const
		{ return !operator==(other); }

		// Streaming:
		friend LIB3RDPARTY_DECL std::ostream& operator<<(std::ostream &os, const attribute_value &v);

		var_type_t type() const { return type_; }

		// Special strings.
		static const std::string s_yes;
		static const std::string s_no;
		static const std::string s_true;
		static const std::string s_false;

	private:
		void evaluate_value(const attribute_value& that);
		void evaluate_str(const std::string& that);
		void evaluate_tstr(const t_string& that);
		void evaluate_double(double v);

	private:
		union {
			bool bool_value_;
			int64_t int_value_;
			double double_value_;
		};
		// must not union str_value_ and tstr_value_.
		std::string* str_value_;
		t_string* tstr_value_;

		// vtype type_;
		var_type_t type_;
#ifdef VERBOSE_CONFIG
		std::string verbose_;
#endif
	};

	typedef std::map<std::string, attribute_value> attribute_map;
	typedef attribute_map::value_type attribute;

	struct const_attribute_iterator
	{
		typedef attribute value_type;
		typedef std::forward_iterator_tag iterator_category;
		typedef int difference_type;
		typedef const attribute *pointer;
		typedef const attribute &reference;
		typedef attribute_map::const_iterator Itor;
		explicit const_attribute_iterator(const Itor &i): i_(i) {}

		const_attribute_iterator &operator++() { ++i_; return *this; }
		const_attribute_iterator operator++(int) { return const_attribute_iterator(i_++); }

		const attribute &operator*() const { return *i_; }
		const attribute *operator->() const { return &*i_; }

		bool operator==(const const_attribute_iterator &i) const { return i_ == i.i_; }
		bool operator!=(const const_attribute_iterator &i) const { return i_ != i.i_; }

	private:
		Itor i_;
	};

	// typedef boost::iterator_range<const_attribute_iterator> const_attr_itors;

	template <typename Iterator>
	struct iterator_range {
		Iterator begin_, end_;
		iterator_range(Iterator begin, Iterator end) : begin_(begin), end_(end) {}
		Iterator begin() const { return begin_; }
		Iterator end() const { return end_; }
		bool empty() const { return begin_ == end_; }

		void pop_front() {
			if (!empty()) {
				// Moving the starting iterator one bit backwards, 
				// it is equivalent to removing the first element
				++ begin_;
			}
		}

		decltype(auto) front() const {
			// Return the reference or value of the first element
			return *begin_;
		}
	};

	typedef iterator_range<const_attribute_iterator> const_attr_itors;


	child_itors child_range(const std::string& key);
	const_child_itors child_range(const std::string& key) const;
	unsigned child_count(const std::string &key) const;

	/**
	 * Determine whether a config has a child or not.
	 *
	 * @param key                 The key of the child to find.
	 *
	 * @returns                   Whether a child is available.
	 */
	bool has_child(const std::string& key) const;

	/**
	 * Returns the first child with the given @a key, or an empty config if there is none.
	 */
	const config & child_or_empty(const std::string &key) const;

	/**
	 * Returns the nth child with the given @a key, or
	 * a reference to an invalid config if there is none.
	 * @note A negative @a n accesses from the end of the object.
	 *       For instance, -1 is the index of the last child.
	 */
	config &child(const std::string& key, int n = 0);

	/**
	 * Returns the nth child with the given @a key, or
	 * a reference to an invalid config if there is none.
	 * @note A negative @a n accesses from the end of the object.
	 *       For instance, -1 is the index of the last child.
	 */
	const config &child(const std::string& key, int n = 0) const
	{ return const_cast<config *>(this)->child(key, n); }

	/**
	 * Returns a mandatory child node.
	 *
	 * If the child is not found a @ref wml_exception is thrown.
	 *
	 * @pre                       parent[0] == '['
	 * @pre                       parent[parent.size() - 1] == ']'
	 *
	 * @param key                 The key of the child item to return.
	 * @param parent              The section in which the child should reside.
	 *                            This is only used for error reporting.
	 *
	 * @returns                   The wanted child node.
	 */
	config& child(const std::string& key, const std::string& parent);

	/**
	 * Returns a mandatory child node.
	 *
	 * If the child is not found a @ref wml_exception is thrown.
	 *
	 * @pre                       parent[0] == '['
	 * @pre                       parent[parent.size() - 1] == ']'
	 *
	 * @param key                 The key of the child item to return.
	 * @param parent              The section in which the child should reside.
	 *                            This is only used for error reporting.
	 *
	 * @returns                   The wanted child node.
	 */
	const config& child(
			  const std::string& key
			, const std::string& parent) const;

	config& add_child(const std::string& key);
	config& add_child(const std::string& key, const config& val);
	config& add_child_at(const std::string &key, const config &val, unsigned index);
/*
#ifdef HAVE_CXX11
	config &add_child(const std::string &key, config &&val);
#endif
*/
	/**
	 * Returns a reference to the attribute with the given @a key.
	 * Creates it if it does not exist.
	 */
	attribute_value &operator[](const std::string &key);

	/**
	 * Returns a reference to the attribute with the given @a key
	 * or to a dummy empty attribute if it does not exist.
	 */
	const attribute_value &operator[](const std::string &key) const;

	/**
	 * Returns a pointer to the attribute with the given @a key
	 * or NULL if it does not exist.
	 */
	const attribute_value *get(const std::string &key) const;

	/**
	 * Function to handle backward compatibility
	 * Get the value of key and if missing try old_key
	 * and log msg as a WML error (if not empty)
	*/
	const attribute_value &get_old_attribute(const std::string &key, const std::string &old_key, const std::string& msg = "") const;
	/**
	 * Returns a reference to the first child with the given @a key.
	 * Creates the child if it does not yet exist.
	 */
	config &child_or_add(const std::string &key);

	bool has_attribute(const std::string &key) const;
	/**
	 * Function to handle backward compatibility
	 * Check if has key or old_key
	 * and log msg as a WML error (if not empty)
	*/
	bool has_old_attribute(const std::string &key, const std::string &old_key, const std::string& msg = "") const;

	void remove_attribute(const std::string &key);
	void merge_attributes(const config &);

	const_attr_itors attribute_range() const;

	/**
	 * Returns the first child of tag @a key with a @a name attribute
	 * containing @a value.
	 */
	config &find_child(const std::string &key, const std::string &name,
		const std::string &value);

	const config &find_child(const std::string &key, const std::string &name,
		const std::string &value) const
	{ return const_cast<config *>(this)->find_child(key, name, value); }

	void clear_children(const std::string& key);

	/**
	 * Moves all the children with tag @a key from @a src to this.
	 */
	void splice_children(config &src, const std::string &key);

	void remove_child(const std::string &key, unsigned index);
	void recursive_clear_value(const std::string& key);

	void clear();
	bool empty() const;
	void to_json(Json::Value& node) const;

	std::string debug() const;
	std::string hash() const;

	// struct error : public game::error, public boost::exception {
	//	error(const std::string& message) : game::error(message) {}
	// };

	struct error: public game::error
	{
		error(const std::string& message, int code = 0, const std::string& file = "", int line = 0)
			: game::error(message), err_code(code), file_name(file), line_number(line) {}

		int get_code() const { return err_code; }
		const std::string& get_file() const { return file_name; }
		int get_line() const { return line_number; }

	private:
		int err_code;
		std::string file_name;
		int line_number;
	};

	struct child_pos
	{
		child_pos(child_map::iterator p, unsigned i) : pos(p), index(i) {}
		child_map::iterator pos;
		unsigned index;

		bool operator==(const child_pos& o) const { return pos == o.pos && index == o.index; }
		bool operator!=(const child_pos& o) const { return !operator==(o); }
	};

	struct any_child
	{
		const child_map::key_type &key;
		const config &cfg;
		any_child(const child_map::key_type *k, const config *c): key(*k), cfg(*c) {}
	};

	struct LIB3RDPARTY_DECL all_children_iterator
	{
		struct arrow_helper
		{
			any_child data;
			arrow_helper(const all_children_iterator &i): data(*i) {}
			const any_child *operator->() const { return &data; }
		};

		typedef any_child value_type;
		typedef std::forward_iterator_tag iterator_category;
		typedef int difference_type;
		typedef const arrow_helper pointer;
		typedef const any_child reference;
		typedef std::vector<child_pos>::const_iterator Itor;
		explicit all_children_iterator(const Itor &i): i_(i) {}

		all_children_iterator &operator++() { ++i_; return *this; }
		all_children_iterator operator++(int) { return all_children_iterator(i_++); }

		reference operator*() const;
		pointer operator->() const { return *this; }

		bool operator==(const all_children_iterator &i) const { return i_ == i.i_; }
		bool operator!=(const all_children_iterator &i) const { return i_ != i.i_; }

	private:
		Itor i_;

		friend class config;
	};

	typedef std::pair<all_children_iterator, all_children_iterator> all_children_itors;

	/** In-order iteration over all children. */
	all_children_itors all_children_range() const;

	all_children_iterator ordered_begin() const;
	all_children_iterator ordered_end() const;
	all_children_iterator erase(const all_children_iterator& i);

	/**
	 * A function to get the differences between this object,
	 * and 'c', as another config object.
	 * I.e. calling cfg2.apply_diff(cfg1.get_diff(cfg2))
	 * will make cfg1 identical to cfg2.
	 */
	config get_diff(const config& c) const;
	void get_diff(const config& c, config& res) const;

	/**
	 * The name of the attribute used for tracking diff changes
	 */
	static const char* diff_track_attribute;

	/**
	 * A function to apply a diff config onto this config object.
	 *
	 * If the "track" parameter is true, the changes made will be marked in a
	 * magic attribute (defined above) of this and child nodes of this config,
	 * with "new" value indicating an added child, "modified" a modified one,
	 * and "deleted" for the deleted items, *which will not be actually
	 * deleted* (so calling code can easily see what they are).
	 * Use clear_diff_track with the same diff object to clear the tracking
	 * info and actually delete the nodes.
	 */
	void apply_diff(const config& diff, bool track = false); //throw error

	/**
	 * Clear any tracking info from a previous apply_diff call with tracking.
	 * This also removes the nodes that are to be deleted, in effect making
	 * apply_diff(c, true); clear_diff_tracking(c);
	 * equivalent to apply_diff(c, false);
	 */
	void clear_diff_track(const config& diff);

	/**
	 * Merge config 'c' into this config, overwriting this config's values.
	 */
	void merge_with(const config& c);

	/**
	 * Merge config 'c' into this config, preserving this config's values.
	 */
	void inherit_from(const config& c);

	bool matches(const config &filter) const;

	/**
	 * Append data from another config object to this one.
	 * Attributes in the latter config object will clobber attributes in this one.
	 */
	void append(const config& cfg);

	/**
	 * Adds children from @a cfg.
	 */
	void append_children(const config &cfg);

	/**
	 * All children with the given key will be merged
	 * into the first element with that key.
	 */
	void merge_children(const std::string& key);

	/**
	 * All children with the given key and with equal values
	 * of the specified attribute will be merged into the
	 * element with that key and that value of the attribute
	 */
	void merge_children_by_attribute(const std::string& key, const std::string& attribute);

	//this is a cheap O(1) operation
	void swap(config& cfg);

private:
	/**
	 * Removes the child at position @a pos of @a l.
	 */
	std::vector<child_pos>::iterator remove_child(const child_map::iterator &l, unsigned pos);

	/** All the attributes of this node. */
	attribute_map values_;

	/** A list of all children of this node. */
	child_map children_;

	std::vector<child_pos> ordered_children;
};

class variable_set
{
public:
	virtual ~variable_set() {}
	virtual config::attribute_value get_variable_const(const std::string &id) const = 0;
};

LIB3RDPARTY_DECL bool operator==(const config &, const config &);
inline bool operator!=(const config &a, const config &b) { return !operator==(a, b); }
std::ostream &operator << (std::ostream &, const config &);

// extern LIB3RDPARTY_DECL std::ostream& operator<<(std::ostream &os, const config::attribute_value &v);
// inline std::ostream &operator<<(std::ostream &os, const config::attribute_value::true_false &v) { return os << v.str(); }
// inline std::ostream &operator<<(std::ostream &os, const config::attribute_value::yes_no &v)     { return os << v.str(); }

namespace aplt {

// implemented by 4libros.so. all libroseaplt.so share this get_config().
// set_get_config() is call by launcher only.
typedef void (*fget_config)(const std::string& path, config& cfg, bool no_macro);
LIB3RDPARTY_DECL void set_get_config(fget_config fget);
LIB3RDPARTY_DECL void get_config(const std::string& path, config& cfg, bool no_macro = true);

typedef void (*fread_config)(const std::string& in, config& cfg);
LIB3RDPARTY_DECL void set_read_config(fread_config fread);
LIB3RDPARTY_DECL void read_config(const std::string& in, config& cfg);
LIB3RDPARTY_DECL bool read_config_ex(const std::string& stream, bool must_utf8, config& result);

typedef void (*fwrite_config)(std::ostream& out, const config& cfg);
LIB3RDPARTY_DECL void set_write_config(fwrite_config fwrite);
LIB3RDPARTY_DECL void write_config(std::ostream& out, const config& cfg);

}

#endif
