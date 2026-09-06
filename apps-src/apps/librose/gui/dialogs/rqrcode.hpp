#ifndef GUI_DIALOGS_RQRCODE_HPP_INCLUDED
#define GUI_DIALOGS_RQRCODE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class ttrack;

class trqrcode: public tdialog
{
public:
	// @track_width(unit: pixel)
	// @track_height(unit: pixel)
	explicit trqrcode(const std::string title, const std::string remark, int track_size, const std::string qrcode_text, surface surf);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_draw_qrcode(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);

private:
	const std::string title_;
	const std::string remark_;
	// unit: pixel, qrcode is square always.
	const int track_best_size_;

	const std::string qrcode_text_;
	surface qrcode_surf_;

	ttrack* track_widget_;
};

} // namespace gui2

#endif

