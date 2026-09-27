#ifndef MKSCRIPT_CONTROLLER_HPP_INCLUDED
#define MKSCRIPT_CONTROLLER_HPP_INCLUDED

#include "base_controller.hpp"
#include "mouse_handler_base.hpp"
#include "mkscript_display.hpp"
#include "mkscript_unit_map.hpp"
#include "map.hpp"
#include "wkoscript.hpp"
#include "mediapipe/rose/mediapipe_api.hpp"

namespace gui2 {
class tmkscript_scene;
class tlabel;
}

class mkscript_controller : public base_controller, public events::mouse_handler_base
{
public:
	mkscript_controller(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const config &app_cfg, CVideo& video,
		int sdl_field_small_font_size, const std::string& saves_courseware_dir, std::string& wkoscript_dir);
	~mkscript_controller();

	mkscript_display& gui() { return *gui_; }
	const mkscript_display& gui() const { return *gui_; }
	events::mouse_handler_base& get_mouse_handler_base() override { return *this; }
	mkscript_display& get_display() override { return *gui_; }
	const mkscript_display& get_display() const override { return *gui_; }

	mkscript_unit_map& get_units() override { return units_; }
	const mkscript_unit_map& get_units() const override { return units_; }

	void app_first_drawn();
	void app_resize_screen();

	bool app_mouse_motion(const int x, const int y, const bool minimap) override;
	void app_left_mouse_down(const int x, const int y, const bool minimap) override;
	void app_left_mouse_up(const int x, const int y, const bool click);
	void app_right_mouse_down(const int x, const int y) override;

	enum {dragtype_place, dragtype_move};
	void longpress_widget(bool& halt, const tpoint& coordinate, gui2::twindow& window, const visio::tshape& shape);
	bool did_drag_mouse_motion(int type, const int x, const int y, gui2::twindow& window);
	void did_drag_mouse_leave(int type, const int x, const int y, bool up_result);

	void do_right_click();

	const std::map<int, visio::tshape>& shape_types() const { return shape_types_; }
	const std::string& add_to_working_dir_msgstr() const { return add_to_working_dir_msgstr_; }

	enum {file_new_from_benchmark, file_new_empty, file_open, file_save_as, file_exit};
	const std::map<int, std::string>& file_ops() const { return file_ops_; }

	class tedge;

#define MAX_EDGES_PER_OBJ		8

	struct tobject
	{
	public:
		tobject(mkscript_controller& controller, const visio::tshape& shape, int obj_at)
			: controller(controller)
			, shape(shape)
			, obj_at(obj_at)
			, sel(false)
			, edge_count(0)
		{
			memset(stretch_rects, 0, sizeof(stretch_rects));
			memset(stretch_points, 0, sizeof(stretch_points));
			memset(edges, 0, sizeof(edges));
		}

		virtual ~tobject()
		{
			// SDL_Log("~tobject()");
		}

		void create_surf_for_state(int width, int height, bool sel);
		void switch_surf(bool sel);

		void fresh_2surf(int width, int height)
		{
			create_surf_for_state(width, height, false);
			if (sel) {
				create_surf_for_state(width, height, true);
			} else {
				sel_surf = nullptr;
			}
		}

		// virtual void create_surf_for_edge() {}

		void calculate_stretch_rects(const SDL_Rect& item_rect)
		{
			visio::calculate_stretch_rects(item_rect, stretch_rects, stretch_points);
		}

	private:
		virtual void app_create_bg_surf(const SDL_Size& max_size, surface& bg_surf) {};
		virtual void app_create_label_surf(int max_width, surface& label_surf) {};

	public:
		mkscript_controller& controller;
		const visio::tshape& shape;
		int obj_at;
		surface surf;
		surface sel_surf;
		bool sel;
		SDL_Rect stretch_rects[SHAPE_STRETCH_RECT_COUNT];
		SDL_Point stretch_points[SHAPE_STRETCH_RECT_COUNT];

		tedge* edges[MAX_EDGES_PER_OBJ];
		int edge_count;
	};

	struct tdraw_item_C
	{
		SDL_Rect rect;
		tobject* obj;
	};

	class tvoice_state: public tobject
	{
	public:
		tvoice_state(mkscript_controller& controller, const visio::tshape& shape, int obj_at)
			: tobject(controller, shape, obj_at)
		{}

		~tvoice_state()
		{
			// SDL_Log("~tvoice_state()");
		}

	private:
		void app_create_label_surf(int max_width, surface& label_surf) override;

	public:
		std::string msgstr;
	};

	class tpose_state: public tobject
	{
	public:
		tpose_state(mkscript_controller& controller, const visio::tshape& shape, int obj_at)
			: tobject(controller, shape, obj_at)
		{}

		~tpose_state()
		{
			// SDL_Log("~tpose_state()");
		}

	private:
		void app_create_bg_surf(const SDL_Size& max_size, surface& bg_surf) override;
		void app_create_label_surf(int max_width, surface& label_surf) override;
	};

	struct tedge_endpoint
	{
		// tdraw_item_C* obj;
		int obj_at;
		int rect8p_at;
	};

	class tedge: public tobject
	{
	public:
		tedge(mkscript_controller& controller, const visio::tshape& shape)
			: tobject(controller, shape, nposm)
			, start({nposm, nposm})
			, end({nposm, nposm})
		{}

		~tedge()
		{
			// SDL_Log("~tpose_state()");
		}

		SDL_Rect set_endpoints2(tdraw_item_C& start_obj, tdraw_item_C& end_obj);
		SDL_Rect set_endpoints(tdraw_item_C& start_obj, int start_at, tdraw_item_C& end_obj, int end_at);

		void start_end_SDL_Points(SDL_Point& _start, SDL_Point& _end) const;
	private:
		SDL_Rect create_surf_for_edge();

	public:
		tedge_endpoint start;
		tedge_endpoint end;
	};

	class ttext: public tobject
	{
	public:
		ttext(mkscript_controller& controller, const visio::tshape& shape, int obj_at)
			: tobject(controller, shape, obj_at)
		{}

		~ttext()
		{
			// SDL_Log("~tpose_state()");
		}

		SDL_Rect set_label(const std::string& new_label)
		{
			if (new_label != label || surf.get() == nullptr) {
				label = new_label;
				return create_surf_for_text();
			}
			return controller.find_draw_item_for_text(obj_at, nullptr)->rect;
		}

	private:
		SDL_Rect create_surf_for_text();

	public:
		std::string label;
	};

	const tdraw_item_C* draw_items(int& count) const 
	{ 
		count = draw_items_.vsize;
		tdraw_item_C* result = (tdraw_item_C*)draw_items_.data;
		return result;
	}

	tdraw_item_C* find_draw_item_for_state(int obj_at) const
	{
		VALIDATE(obj_at != nposm, null_str);
		tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
		for (int at = 0; at < draw_items_.vsize; at ++) {
			tdraw_item_C* item = items + at;
			if (item->obj->obj_at == obj_at) {
				if (shape_type_is_state(item->obj->shape.type)) {
					return item;
				}
			}
		}
		
		return nullptr;
	}

	tdraw_item_C* find_draw_item_for_edge(int start_obj_at, int* item_at) const
	{
		VALIDATE(start_obj_at != nposm, null_str);
		tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
		for (int at = 0; at < draw_items_.vsize; at ++) {
			tdraw_item_C* item = items + at;
			if (item->obj->shape.type == visio::shape_edge) {
				tedge* edge = static_cast<tedge*>(item->obj);
				if (edge->start.obj_at == start_obj_at) {
					if (item_at != nullptr) {
						*item_at = at;
					}
					return item;
				}	
			}
		}
		
		return nullptr;
	}

	tdraw_item_C* find_draw_item_for_text(int obj_at, int* item_at) const
	{
		VALIDATE(obj_at != nposm, null_str);
		tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
		for (int at = 0; at < draw_items_.vsize; at ++) {
			tdraw_item_C* item = items + at;
			if (item->obj->shape.type == visio::shape_text && at == obj_at) {
				if (item_at != nullptr) {
					*item_at = at;
				}
				return item;
			}
		}
		
		return nullptr;
	}

	int sel_obj_at_with_validate() const
	{
		VALIDATE(sel_obj_ != nullptr, null_str);
		VALIDATE(sel_obj_->obj->obj_at != nposm, null_str);

		const int obj_at = sel_obj_->obj->obj_at;
		tdraw_item_C* item = find_draw_item_for_state(obj_at);
		VALIDATE(item == sel_obj_, null_str);

		VALIDATE(tmp_script_.states.count(obj_at) != 0, null_str);
		return obj_at;
	}

	const aplt::twkoscript::tstate2& state2_from_sel_obj_with_validate() const
	{
		const int obj_at = sel_obj_at_with_validate();
		const aplt::twkoscript::tstate2& state2 = tmp_script_.states.find(obj_at)->second;
		VALIDATE(state2.state == obj_at, null_str);
		return state2;
	}

	aplt::twkoscript::tstate2& mutable_state2_from_sel_obj_with_validate()
	{
		const int obj_at = sel_obj_at_with_validate();
		aplt::twkoscript::tstate2& state2 = tmp_script_.states.find(obj_at)->second;
		VALIDATE(state2.state == obj_at, null_str);
		return state2;
	}

	void click_object(tdraw_item_C& item);
	void handle_file_menu(int sel);

private:
	void app_create_display(int initial_zoom) override;
	void app_post_initialize() override;
	void app_play_slice() override;

	void app_execute_command(int command, const std::string& sparam) override;
	bool app_in_context_menu(const std::string& id) const override;
	bool actived_context_menu(const std::string& id) const override;

	void update_title_label();
	void update_status_label(bool valid);
	void load_preset_poses_cfg();
	void load_action_tpl2s_cfg();

	void reload_map(int w, int h);
	void draw_fix_text_shapes();
	void draw_flowchart_from_script(const std::string& filename, const aplt::twkoscript& script);
	void script_clear_and_set_valid_id();
	void new_empty_flowchart();

	void clear_draw_items();
	tobject* create_derived_obj(const visio::tshape& shape, int obj_at, const SDL_Rect& item_rect);
	tdraw_item_C* insert_draw_item(const visio::tshape& shape, const SDL_Rect& rect, bool new_state2);
	void erase_draw_item(int obj_at);
	void insert_edge(tdraw_item_C& start, tdraw_item_C& end);
	void erase_edge_from_draw_items(int item_at);
	tdraw_item_C* insert_text(int obj_at, const SDL_Point& offset, const std::string& label);

	tdraw_item_C* in_which_obj(int screen_x, int screen_y) const;
	int in_which_stretch_rect(const SDL_Point& map_xy) const;
	void select_object(tdraw_item_C* item);
	void scroll_to_object(tdraw_item_C& item);
	void reconnect_edges(const tdraw_item_C& reason_obj);
	void switch_item(int obj_at1, int obj_at2);
	void modify_item_rect(tdraw_item_C& item, const SDL_Rect& new_rect);
	int calculate_edge_count(const tdraw_item_C& item, int* start, int* end) const;
	void validate_draw_items() const;
	const SDL_Rect* get_stretch_rects(const tdraw_item_C& item) const;

	std::string editing_phase_surf_dir() const;

	// edit, compare, save wkoscript
	bool wkoscript_dirty() const;

	void click_system();
	bool confirm_file_op(int sel);
	void open_cfg_file_bh(const std::string& filename);
	void handle_file_op(int sel);
	bool handle_pre_save();
	void click_save();
	void list_wkoscript_files2(int type, std::set<std::string>& result_set);
	enum {edittype_id, edittype_name, edittype_author, edittype_reference, edittype_state_name};
	bool did_verify_text_changed(const std::string& label, const std::string& initial, int type, const std::set<std::string>& xcludes) const;
	void handle_edit_text(int type);
	void click_edit_state_name();
	void click_setting();
	void click_next_state();
	void click_clone();
	void click_switch_state(bool add1);
	void click_erase();
	void click_copy_action_tpl();
	void click_paste_action_tpl();
	void click_add_to_working_dir();
	void click_share();

private:
	net::trdpd_manager& rdpd_mgr_;
	tpble2& pble_;
	tprivacy& privacy_;
	const int sdl_field_small_font_size_;
	const std::string upload_path_;
	const std::string download_path_;
	std::string& wkoscript_dir_;
	mkscript_unit_map units_;
	tmap map_;
	const std::map<int, visio::tshape> shape_types_;
	const std::map<int, std::string> file_ops_;
	const int max_draw_items_;
	const SDL_Size min_map_size_;
	const SDL_Size flowchart_margin_;
	const int shape_text_font_size_;
	const std::string add_to_working_dir_msgstr_;
	mkscript_display* gui_;
	gui2::tmkscript_scene* dlg_;
	gui2::twindow* window_;

	gui2::tlabel* title_widget_;
	gui2::tlabel* status_widget_;

	uint32_t next_1second_ticks_;
	bool allow_draw_;

	std::string filename_;
	aplt::twkoscript script_;
	aplt::twkoscript tmp_script_;
	int min_vsize_;
	int next_lmk33_png_at_;
	telem_array_C draw_items_;

	class tdisable_validate_items_states_lock
	{
	public:
		tdisable_validate_items_states_lock(mkscript_controller& controller)
			: controller_(controller)
		{
			VALIDATE(!controller_.disable_validate_items_states_, null_str);
			controller_.disable_validate_items_states_ = true;
		}

		~tdisable_validate_items_states_lock()
		{
			VALIDATE(controller_.disable_validate_items_states_, null_str);
			controller_.disable_validate_items_states_ = false;
		}

	private:
		mkscript_controller& controller_;
	};
	bool disable_validate_items_states_;

	const visio::tshape* longpress_shape_;
	SDL_Point longpress_custom_xy_formula_;

	class tclear_drag_obj_lock
	{
	public:
		tclear_drag_obj_lock(mkscript_controller& controller, int type)
			: controller_(controller)
			, type_(type)
		{
			if (type == dragtype_place) {
				VALIDATE(controller_.longpress_shape_ != nullptr, null_str);

			} else if (type == dragtype_move) {
				VALIDATE(controller_.downing_obj_ != nullptr, null_str);

			} else {
				VALIDATE(false, null_str);
			}
		}

		~tclear_drag_obj_lock();

	private:
		mkscript_controller& controller_;
		int type_;
	};
	tdraw_item_C* downing_obj_;
	tdraw_item_C* sel_obj_;

	struct tstretch_helper
	{
		tstretch_helper()
		{
			clear();
		}

		void set(int _at, const SDL_Rect& _orignal_rect, const SDL_Point& _first_mouse_point)
		{
			at = _at;
			orignal_rect = _orignal_rect;
			first_mouse_point = _first_mouse_point;
			VALIDATE(!halt, null_str);
		}
		bool valid() const { return at != nposm; }

		void stretch(tdraw_item_C& sel_item, const SDL_Point& map_xy);

		void clear()
		{
			at = nposm;
			orignal_rect = empty_rect;
			first_mouse_point = SDL_Point{nposm, nposm};
			halt = false;
		}

		int at;
		SDL_Rect orignal_rect;
		SDL_Point first_mouse_point; // map coor
		bool halt;
	};

	class tclear_left_mouse_down_lock
	{
	public:
		tclear_left_mouse_down_lock(mkscript_controller& controller)
			: controller_(controller)
		{}

		~tclear_left_mouse_down_lock()
		{
			controller_.stretch_helper_.clear();
			controller_.obj_may_click_ = nullptr;
			controller_.nposm_when_down_ = false;
		}

	private:
		mkscript_controller& controller_;
	};
	tstretch_helper stretch_helper_;
	tdraw_item_C* obj_may_click_;
	bool nposm_when_down_;

	// preset pose
	std::map<int, aplt::tpreset_pose> preset_poses_;

	// action tpl2
	const std::map<std::string, aplt::taction_tpl2>& action_tpl2s_;
	aplt::taction_tpl2 clipboard_action_tpl2_;

	// std::unique_ptr<mediapipe::tpose_tracking_api> api_ptr_;
};

#endif
