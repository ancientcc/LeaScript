#ifndef GUI_DIALOGS_TLATEX_EDITOR_HPP_INCLUDED
#define GUI_DIALOGS_TLATEX_EDITOR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tlistbox;
class ttoggle_panel;
class ttext_box;
class ttrack;
class tscroll_text_box;

class tlatex_editor: public tdialog
{
public:
	explicit tlatex_editor();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void load_formula();

	void did_draw_pdf_surf(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);
	void reload_tpl_list(tlistbox& list);
	void did_tpl_list_row_changed(tlistbox& list, ttoggle_panel& row);

private:
	struct tformula
	{
		tformula(const std::string& name, const std::string& tex)
			: name(name)
			, tex(tex)
		{}

		const std::string name;
		const std::string tex;
	};

	std::vector<tformula> formulas_;
	surface pdf_surf_;

	tlistbox* tpl_list_;
	ttrack* pdf_surf_widget_;
	tscroll_text_box* content_widget_;
};

} // namespace gui2

#endif

