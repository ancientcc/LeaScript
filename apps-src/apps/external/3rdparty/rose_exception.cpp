

#include "rose_exception.hpp"
#include <map>
#include <SDL_thread.h>

const std::string null_str = "";
const std::string ellipsis_str = "...";
const std::string uuid_nposm = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
const std::string formatted_uuid_nposm = "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee";
const std::string str_nposm = "_@_a_(_=b-c&d.e|f$g+h_[_]_<h>_?i_/j_k";

const SDL_Rect empty_rect = {0, 0, 0, 0};
// as if valid rect, x or y maybe negative.
const SDL_Rect null_rect = {0, 0, -1, -1};

std::map<bool_set_t, tcode3> bool_set_types;

unsigned long main_tid = nposm;

const std::string charge_pos_uuid = "e7c997a2-df8b-4601-9ddf-2c2cd5bca2cf";
std::string charge_pos_name; // set when
std::string privacy_protect_msgstr;
const std::string widget_id_main_map = "_main_map";
const std::string widget_id_mini_map = "_mini_map";

fnros_wml_exception s_fwml_exception = nullptr;

void rose_set_wml_exception(fnros_wml_exception fexception)
{
	s_fwml_exception = fexception;
}

// fnros_wml_exception rose_fwml_exception()
// {
//	return s_fwml_exception;
// }