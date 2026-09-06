#ifndef LAUNCHER_GAME_LATEX_HPP
#define LAUNCHER_GAME_LATEX_HPP

#include "rose_latex.hpp"
// #include "../Programs/TeXAndFriends/luatex/source/lua/luatex-api.h"
#include "rose/miktex_api.h"
#include "rose_version.hpp"

extern std::string miktex_output_dir;

int miktex_luahbtex_main(const std::vector<std::string>& args, const tluatex_hook* hook);
int miktex_miktex_main(const std::vector<std::string>& args);

namespace latex
{

surface doc_to_surf(const std::string& doc, int max_width);

bool tex_file_to_pdf_file(const std::string& tex_file, const tluatex_hook& hook);
void make_lualatex_fmt();

extern const std::map<int, std::string> latex_types;
bool sandbox_is_valid(version_info* _version, int64_t* _ts, int* _type, std::string* _desc);

}

#endif
