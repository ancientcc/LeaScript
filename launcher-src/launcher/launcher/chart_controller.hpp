/* $Id: editor_controller.hpp 47816 2010-12-05 18:08:38Z mordante $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef CHART_CONTROLLER_HPP_INCLUDED
#define CHART_CONTROLLER_HPP_INCLUDED

#include "base_controller.hpp"
#include "mouse_handler_base.hpp"
#include "chart_display.hpp"
#include "chart_unit_map.hpp"
#include "map.hpp"
#include "sound.hpp"
#include "gui/auxiliary/window_builder.hpp"
#include "gui/widgets/widget.hpp"
// #include "gui/dialogs/chart_scene.hpp"
#include "base_instance.hpp"
#include "speech_driver.hpp"

namespace gui2 {
class ttrack;
class tcontrol;
class ttoggle_button;
class tchart_scene;
}

extern tnot_main_base_msg_subscriber* capture_audio2_singleton;

/**
 * The editor_controller class containts the mouse and keyboard event handling
 * routines for the editor. It also serves as the main editor class with the
 * general logic.
 */
class chart_controller : public base_controller, public events::mouse_handler_base, 
	public tbase_msg_subscriber, public tnot_main_base_msg_subscriber
{
public:
	static int calculate_divisor(int sum, size_t count);
	static bool calculate_unit_width(int map_w, int total_samples, int zoom, int& samples_per_unit, int& samples_per_pixel, int& used_units, int& unit_w, int& can_spread_samples);

	static int original_width;
	static int original_height;

	class tclear_cursor_draging_lock
	{
	public:
		tclear_cursor_draging_lock(chart_controller& controller)
			: controller_(controller)
		{}

		~tclear_cursor_draging_lock()
		{
			controller_.cursor_draging_ = false;
		}

	private:
		chart_controller& controller_;
	};

	/**
	 * The constructor. A initial map context can be specified here, the controller
	 * will assume ownership and delete the pointer during destruction, but changes
	 * to the map can be retrieved between the main loop's end and the controller's
	 * destruction.
	 */
	chart_controller(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tspeech_driver& speech_driver, const config& app_cfg, CVideo& video);

	~chart_controller();

	chart_display& gui() { return *gui_; }
	const chart_display& gui() const { return *gui_; }

	const map_location& selected_hex() const { return selected_hex_; }

	bool app_mouse_motion(const int x, const int y, const bool minimap) override;
	void app_left_mouse_down(const int x, const int y, const bool minimap) override;
	void app_left_mouse_up(const int x, const int y, const bool click) override;
	void app_right_mouse_down(const int x, const int y) override;

	bool app_in_context_menu(const std::string& id) const override;
	bool actived_context_menu(const std::string& id) const;

	void do_right_click();
	void select_unit(chart_unit* u);


	void pinch_event(bool out);

	/* base_controller overrides */
	mouse_handler_base& get_mouse_handler_base();
	chart_display& get_display() { return *gui_; }
	const chart_display& get_display() const { return *gui_; }

	chart_unit_map& get_units() { return units_; }
	const chart_unit_map& get_units() const { return units_; }

	void update_rec_ui(bool ing);
	void update_play_ui(bool into_play);

	void fill_unit_pixel();
	void fill_unit_pixel2(const std::vector<chart_unit*>& top_units, bool audio);

	void set_played(int played);
	void notify_finished();

	void update_play_cursor(int played);

	double music_duration() const { return music_during_; }
	void stop_music();

	bool cursor_draging() const { return cursor_draging_; }

	void slice_end();
	void reposition_cursor_pos();
	void update_tag_ui() const;

	void did_draw_tag(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, bool tag);
	void did_draw_float_tag1(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, int set_height);
	void signal_handler_left_button_down(gui2::tcontrol* control, const tpoint& coordinate);
	bool callback_control_drag_detect(gui2::tcontrol* control, const tpoint& coordinate);
	bool callback_set_drag_coordinate(gui2::tcontrol* control, const tpoint& last);

	void set_gain(gui2::twindow& window, const int valid_height, const int full_height);
	bool show_mic_wave() const { return show_mic_wave_; }
	bool show_nagtive() const { return show_nagtive_; }
	int speech_voice_threshold_and_set(int& my_voice_threshold);
	int speech_voice_threshold() const { return curr_visual_info_.voice_threshold; }
	bool allow_draw() const { return allow_draw_; }

	void app_first_drawn();
	void app_resize_screen();

private:
	void app_create_display(int initial_zoom) override;
	void app_post_initialize() override;
	void app_play_slice() override;

	/** command_executor override */
	void app_execute_command(int command, const std::string& sparam) override;

	void reload_map(int w, int h);
	void set_format_report(const std::string& msg);
	void set_status_report(const std::string& msg);
	void set_icon_report(int num, bool visible);

	void speech_resize_data(int size);
	void speech_fill_mic_data(const SDL_AudioSpec& using_spec);

	void speech_did_recognition_result2(const std::string& result) override;

	// 
	void speech_did_capture_audio2(const uint8_t* stream, int len) override;

	void goto_title();
	void did_show_nagtive_changed(gui2::ttoggle_button& widget);
	void click_record();
	void play_music(const std::string& file);
	void stop_rec(bool manual);

	bool finger_coordinate_valid(int x, int y) const;
	void slice_before_scroll();

private:
	net::trdpd_manager& rdpd_mgr_;
	tpble2& pble_;
	tprivacy& privacy_;
	tspeech_driver& speech_driver_;
	chart_display* gui_;
	gui2::tchart_scene* dlg_;
	gui2::twindow* window_;

	gui2::tcontrol* voice_maybe_start_widget_;

	tmap map_;
	chart_unit_map units_;
	sound::tfrequency_lock* frequency_lock_;
	sound::thook_music_finished* hook_music_finished_;
	// sound::thook_music_playing* hook_music_playing_;

	map_location selected_hex_;

	std::string wav_file_name_;

	std::unique_ptr<tfile> wav_file_;
	twave_pcm_hdr_44bytes wav_hdr_;

	bool cursor_draging_;

	int play_samples_per_pixel_;
	int play_integer_part_;
	int play_spread_part_;

	double music_during_;
	Uint32 disable_cursor_pos_;

	time_t play_from_;

	int played_;
	bool notify_finished_;

	tpoint first_coordinate_;
	int start_drag_x_;
	

	bool seting_gain_;
	int last_drag_x_;
	int current_gain_;
	int mercury_yoffset_;
	int mercury_height_;
	int gain_yoffset_;
	int current_mercury_height_;

	int audio_bytes_per_sec_;
	int sound_buffer_size_;

	uint32_t next_1second_ticks_;
	bool allow_draw_;

	bool show_mic_wave_;
	bool show_nagtive_;
	int avg_bytes_per_sec_;
	uint8_t* speech_data_;
	int speech_data_size_;
	int speech_data_vsize_;

	std::string default_status_msg_;
	int filled_units_;
	std::unique_ptr<tfile> rec_file_;
	const int max_rec_speech_bytes_;
	int rec_speech_bytes_;

	aplt::tspeech_slot::tvisual_info_C my_visual_info_;
	aplt::tspeech_slot::tvisual_info_C curr_visual_info_;

	tuint8data_C dbg_speech_data_;
	int dbg_speech_data_byte_pos_;
};

#endif
