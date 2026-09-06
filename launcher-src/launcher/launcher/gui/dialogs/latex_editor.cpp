#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/latex_editor.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "rose_filesystem_dll.hpp"
#include "font.hpp"

// #include <mupdf/fitz.h>
// #include "libyuv/convert_argb.h"
// #include <opencv2/imgproc.hpp>
#include "rose_latex.hpp"

using namespace std::placeholders;

namespace latex {
// extern void tex_file_to_pdf_file(const std::string& tex_file);
}

namespace gui2 {

REGISTER_DIALOG(launcher, latex_editor)

tlatex_editor::tlatex_editor()
	: tpl_list_(nullptr)
	, pdf_surf_widget_(nullptr)
	, content_widget_(nullptr)
{
	// load_formula();
	std::string tmp_tex_filename = game_config::preferences_dir + "/saves/__tmp_tex.tex";

	// latex::tex_file_to_pdf_file(tmp_tex_filename);
}

void tlatex_editor::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	find_widget<tlabel>(window_, "title", false).set_label(_("Edit LaTeX text"));

	tlistbox* list = find_widget<tlistbox>(window_, "tpl_list", false, true);
	list->enable_select(false);
	// list->set_did_row_pre_change(std::bind(&tlatex_editor::did_tpl_list_row_pre_change, this, _1, _2));
	list->set_did_row_changed(std::bind(&tlatex_editor::did_tpl_list_row_changed, this, _1, _2));
	tpl_list_ = list;

	ttrack* track = find_widget<ttrack>(window_, "pdf_surf", false, true);
	track->disable_rose_draw_bg();
	track->set_did_draw(std::bind(&tlatex_editor::did_draw_pdf_surf, this, _1, _2, _3));
	// track->set_did_mouse_leave(std::bind(&tmanual_home::did_mouse_leave_portrait, this, _1, _2, _3));
	// track->set_did_mouse_motion(std::bind(&tmanual_home::did_mouse_motion_portrait, this, _1, _2, _3));
	pdf_surf_widget_ = track;

	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(window_, "content", false, true);
	content_widget_ = scroll_text_box;

	reload_tpl_list(*tpl_list_);
}

void tlatex_editor::post_show()
{
}

void tlatex_editor::load_formula()
{
	std::string stream;
	std::string filename = game_config::app_dir_root + "/cert/formula.cfg";
	{
		const int max_task_cpp_cfg_size = 512 * 1024; // 512K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize == 0 || fsize > max_task_cpp_cfg_size) {
			SDL_Log("Cann't find %s, or size must be <= 512K bytes", filename.c_str());
			return;
		}

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		if (!all_is_utf8) {
			SDL_Log("%s isn't utf-8 format", filename.c_str());
			return;
		}
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	aplt::read_config_ex(stream, true, top_cfg);

	BOOST_FOREACH (const config& c, top_cfg.child_range("formula")) {
		const std::string name = c["name"].str();
		const std::string tex = c["tex"].str();
		if (name.empty() || tex.empty()) {
			continue;
		}
		formulas_.push_back(tformula(name, tex));
	}
}

void tlatex_editor::reload_tpl_list(tlistbox& list)
{
/*
	if (formulas_.empty()) {
		formulas_.push_back("$(1 + x)^n \\geqslant 1 + nx \\quad (x > -1, \\ n \\in \\mathbb{N}^+ )$");

		formulas_.push_back("$(a_1^2 + a_2^2 + \\cdots + a_n^2)(b_1^2 + b_2^2 + \\cdots + b_n^2) \\geqslant (a_1b_1 + a_2b_2 + \\cdots + a_nb_n)^2$");

		formulas_.push_back("$( \\sum\\limits_{i=1}^n (a_i + b_i)^p )^{1/p} \\leqslant ( \\sum\\limits_{i=1}^n a_i^p )^{1/p} + ( \\sum\\limits_{i=1}^n b_i^p )^{1/p}$");
	}
*/
	std::map<std::string, std::string> data;
	for (std::vector<tformula>::const_iterator it = formulas_.begin(); it != formulas_.end(); ++ it) {
		const tformula& formula = *it;

		data["name"] = formula.name;
		data["formula"] = latex::doc_to_doc2(formula.tex);

		list.insert_row(data);
	}
}

void tlatex_editor::did_tpl_list_row_changed(tlistbox& list, ttoggle_panel& row)
{
	int at = row.at();
	const tformula& formula = formulas_[at];

	pdf_surf_ = latex::doc_to_surf(formula.tex, 384);
	pdf_surf_widget_->immediate_draw();

	// content_widget_->set_label(formula);
/*
	if (ignore_file_list_row_changed_) {
		return;
	}

	int at = row.at();
	const tcourseware_file& courseware_file = courseware_file_from_at(at);

	std::string cfgfile;
	if (courseware_file.uid == COURSEWARE_UPLOAD_UID) {
		cfgfile = join_main_cfg_filename2(upload_path_, courseware_file.dir_name);

	} else {
		cfgfile = join_main_cfg_filename(courseware_file.dir_name);
	}
	load_courseware_cfg(cfgfile);

	clear_tree2();

	{
		// tignore_tree_msg_text_changed_lock lock(*this);

		tcookie3f cookie3f(0, type_global, field_title);
		ttree& tree = *tree_widget_;
		ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		tree.select_node(node);
		// node->set_widget_label("label", tmp_courseware_.title);
	}

	refresh_toolbar_active(nullptr);

	if (curr_layer_ == UPLOAD_LAYER || curr_layer_ == DOWNLOAD_LAYER) {
		bool favorited = cfg_cpp_api_.is_existed_course(courseware_file.uid, courseware_file.title);
		favorite_widget_->set_label(favorited? favorited_png: unfavorite_png);
	}

	if (curr_layer_ == DOWNLOAD_LAYER) {
		if (curr_main_layer_ != MAIN_TREE_LAYER) {
			main_stack_->set_radio_layer(MAIN_TREE_LAYER);
		}
	}

	bool dirty = courseware_dirty();
	save_widget_->set_active(dirty);
*/
}

void tlatex_editor::did_draw_pdf_surf(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();

	// const bool require_render = current_layer_ == SCENE_LAYER && current_page_ != QUERYPERSON_PAGE;
	const bool require_render = true;

	if (require_render && !bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), NULL, &widget_rect);
	}
	
	surface color_surf = image::get_image("misc/bg_ff0000.png");
	// const tpoint ratio_size = calculate_adaption_ratio_size(widget_rect.w, widget_rect.h, portrait_->w, portrait_->h);
	// SDL_Rect dst {widget_rect.x + (widget_rect.w - ratio_size.x) / 2, widget_rect.y + (widget_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
	SDL_Rect dst {widget_rect.x, widget_rect.y, 384, (int)(4 * gui2::twidget::hdpi_scale)};
	texture tex = SDL_CreateTextureFromSurface2(renderer, color_surf.get());
	if (require_render) {
		SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
	}

	if (pdf_surf_.get() != nullptr) {
		VALIDATE(pdf_surf_.get(), null_str);
		// const tpoint ratio_size = calculate_adaption_ratio_size(widget_rect.w, widget_rect.h, portrait_->w, portrait_->h);
		// SDL_Rect dst {widget_rect.x + (widget_rect.w - ratio_size.x) / 2, widget_rect.y + (widget_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
		dst = ::create_rect(widget_rect.x, widget_rect.y, pdf_surf_->w, pdf_surf_->h);

		tex = SDL_CreateTextureFromSurface2(renderer, pdf_surf_.get());
		if (require_render) {
			SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
		}
	}
/*
	if (reload_card_surf_.get()) {
		// reset button
		tex = SDL_CreateTextureFromSurface2(renderer, reload_card_surf_);
		reload_card_rect_ = ::create_rect(widget_rect.x, dst.y, reload_card_surf_->w, reload_card_surf_->h);
		if (require_render) {
			SDL_RenderCopy(renderer, tex.get(), nullptr, &reload_card_rect_);
		}
	}
*/
}

} // namespace gui2

