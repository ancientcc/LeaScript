#define GETTEXT_DOMAIN "kdesktop-lib"

#include "gui/dialogs/klink.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/rvar_editor.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/listbox.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "base_instance.hpp"
#include "serialization/parser.hpp"
#include "game_config.hpp"

// using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(kdesktop, klink)

tklink::tklink(trvar_editor::tslot& var_editor_slot,
	std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tcfg_cpp_api_core& cfg_cpp_api,
	aplt::tbg_task& bg_task, tbase_driver_core& base_driver)
	: thelper_klink(var_editor_slot, applets, cfg_cpp_api, bg_task, base_driver)
	, report_(nullptr)
	, stack_(nullptr)
	, current_layer_(nposm)
{
	set_timer_interval(1000);
}

void tklink::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	// tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	thelper_klink::pre_show(*window_);

	tbutton* button = find_widget<tbutton>(window_, "refresh", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_refresh
			, this, std::ref(*button)));
	refresh_widget_ = button;

	//
	//
	// tgrid* grid = find_widget<tgrid>(window_, "main_grid", false, true);
	treport* report = find_widget<treport>(window_, "main_report", false, true);
	report_ = report;
	stack_ = find_widget<tstack>(window_, "main_stack", false, true);

	pre_scene(*stack_->layer(SCENE_LAYER));
	pre_env_var(*stack_->layer(ENV_VAR_LAYER));

	tcontrol* item = &report->insert_item(null_str, _("Base scene"));
	item = &report->insert_item(null_str, _("Env var"));

	report->set_did_item_changed(std::bind(&tklink::did_item_changed, this, _1, _2));
	report->select_item(SCENE_LAYER);
}

void tklink::post_show()
{
}

void tklink::did_item_changed(treport& report, ttoggle_button& widget)
{
	tgrid* current_layer = stack_->layer(widget.at());
	stack_->set_radio_layer(widget.at());

	current_layer_ = widget.at();

	if (widget.at() == SCENE_LAYER) {
		import_widget_->set_visible(twidget::VISIBLE);
		export_widget_->set_visible(twidget::VISIBLE);
		insert_scene_widget_->set_visible(twidget::VISIBLE);
		refresh_widget_->set_visible(twidget::INVISIBLE);

		reload_scene_list(*scene_list_);

	} else if (widget.at() == ENV_VAR_LAYER) {
		import_widget_->set_visible(twidget::INVISIBLE);
		export_widget_->set_visible(twidget::INVISIBLE);
		insert_scene_widget_->set_visible(twidget::INVISIBLE);
		refresh_widget_->set_visible(twidget::VISIBLE);

		reload_env_var_list(*env_var_list_);

	} else {
		VALIDATE(false, null_str);
	}
}


void tklink::app_get_task_item3fs(std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, uint32_t deny_type_mask, bool allow_empty)
{
	get_task_item3fs(items, item3fs, allow_type_mask, deny_type_mask, allow_empty);
}

void tklink::app_post_click_import()
{
	reload_scene_list(*scene_list_);
}

void tklink::app_pre_click_export()
{
	VALIDATE(current_layer_ == SCENE_LAYER, null_str);
}

void tklink::app_timer_handler(uint32_t now)
{
	// refresh_statusbar_grid(now);
}

} // namespace gui2

