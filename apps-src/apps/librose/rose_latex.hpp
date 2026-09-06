#ifndef LIBROSE_ROSE_LATEX_HPP
#define LIBROSE_ROSE_LATEX_HPP

#include "rose_sdl_utils.hpp"

namespace latex
{

void enable_latex();

std::string doc_to_doc2(const std::string& doc);
std::string doc2_to_doc(const std::string& doc2);
bool is_doc2(const std::string& str, std::string* doc = nullptr);

surface doc2_to_surf(const std::string& doc2, int max_width);

// app impletement below api
surface doc_to_surf(const std::string& doc, int max_width);

//
// pdf cache
//
class tlocator
{
public:
	tlocator(const std::string& tex);

	int in_cache() const;
	const surface& locate_in_cache(int index) const;
	void add_to_cache(const surface& data) const;

private:
	const std::string tex_;
	size_t hash_;
	size_t hash1_;
};

void clear_cache();

}

#endif
