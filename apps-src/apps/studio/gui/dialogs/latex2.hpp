#ifndef GUI_DIALOGS_LATEX2_HPP_INCLUDED
#define GUI_DIALOGS_LATEX2_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tbutton;

class tlatex2: public tdialog
{
public:
	explicit tlatex2();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_generate_rsp(tbutton& widget);

private:
	const std::string miktex_res_;
	const std::string zipfile_;
	const std::string rspfile_;
	const std::string unzipped_dir_;
};

} // namespace gui2

#endif

