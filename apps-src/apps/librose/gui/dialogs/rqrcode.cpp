#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rqrcode.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "qr_code.hpp"
#include "font.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, rqrcode)

trqrcode::trqrcode(const std::string title, const std::string remark, int track_size, const std::string qrcode_text, surface surf)
	: title_(title)
	, remark_(remark)
	, track_best_size_(track_size)
	, qrcode_text_(qrcode_text)
	, qrcode_surf_(surf)
	, track_widget_(nullptr)
{
}

void trqrcode::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	window_->set_click_dismiss(true);
	
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (!title_.empty()) {
		label->set_label(title_);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	label = find_widget<tlabel>(window_, "remark", false, true);
	if (!remark_.empty()) {
		label->set_label(remark_);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	// find_widget<tlabel>(window_, "title", false).set_label(_("Hello World"));
	if (!qrcode_text_.empty()) {
		qrcode_surf_ = generate_qr(qrcode_text_, track_best_size_);
	}
	if (qrcode_surf_.get() == nullptr) {
		qrcode_surf_ = create_neutral_surface(track_best_size_, track_best_size_);
		surface text_surf = font::get_rendered_text(_("Generate qrcode fail"), 0, font::SIZE_DEFAULT, font::BAD_COLOR);
		SDL_Rect dst {(qrcode_surf_->w - text_surf->w) / 2, (qrcode_surf_->h - text_surf->h) / 2, text_surf->w, text_surf->h};
		sdl_blit(text_surf, nullptr, qrcode_surf_, &dst);
	}
	ttrack* track = find_widget<ttrack>(window_, "track", false, true);
	track->set_best_size_1th(track_best_size_, track->get_width_is_max(), track_best_size_, track->get_height_is_max());
	track->set_timer_interval(0);
	track->set_did_draw(std::bind(&trqrcode::did_draw_qrcode, this, _1, _2, _3));
	track_widget_ = track;
}

void trqrcode::post_show()
{
}

void trqrcode::did_draw_qrcode(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	
	if (qrcode_surf_.get() == nullptr) {
		return;
	}
/*
	VALIDATE(!qrcode_message_->text_box()->label().empty(), null_str);
	surface text_surf = font::get_rendered_text(qrcode_message_->text_box()->label(), 0, font::SIZE_DEFAULT, font::DISABLED_COLOR);
	SDL_Rect dst {widget_rect.x + (widget_rect.w - text_surf->w) / 2, widget_rect.y + 8 * twidget::hdpi_scale, text_surf->w, text_surf->h};
	render_surface(renderer, text_surf, nullptr, &dst);

	SDL_Rect dst;
	// const int max_allowed_width = widget_rect.w * 4 / 5; // more black background
	const int max_allowed_width = widget_rect.w; // more black background
	const tpoint ratio_size = calculate_adaption_ratio_size(max_allowed_width, widget_rect.h, qrcode_surf_->w, qrcode_surf_->h);
	dst = ::create_rect(widget_rect.x + (widget_rect.w - ratio_size.x) / 2, widget_rect.y + (widget_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y);
*/
	SDL_Rect dst = widget_rect;
	texture tex = SDL_CreateTextureFromSurface2(renderer, qrcode_surf_.get());
	SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
}

} // namespace gui2

