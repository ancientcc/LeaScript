#ifndef LAUNCHER_GAME_TRAY_HPP
#define LAUNCHER_GAME_TRAY_HPP

#include "tray.hpp"
#include "rose_ros/aplt.hpp"
#include "video.hpp"

class ttray2: public ttray, public ttray_window::tslot
{
public:
	ttray2(CVideo& video);

	~ttray2()
	{
	}
	posix_noncopyable(ttray2);

	void create_main_menu();
	void slice() override;
	void push_task(const std::string& msg, int duration_ms);
	void stop_last_task();

#define TRAYEVENT_PRE_POPUPMENU    1
	void handle_tray_event(uint32_t type);

private:
	void update_scene_submenu();
	void update_menu_on_click();

	void render_tray_window_idle();
	void handle_menu_entry(int id, int ctx) override;

	void start_dragging(int globalX, int globalY, int winX, int winY);

	// ttray_slot
	void tray_handle_WINDOWEVENT(const SDL_WindowEvent& windowevt) override;
	void tray_handle_MOUSEBUTTONDOWN(int x, int y) override;
	void tray_handle_MOUSEBUTTONUP(int x, int y) override;
	void tray_handle_MOUSEMOTION(int x, int y) override;

	void create_tray_window();
	void render_tray_window(const std::string& msg, int font_size, const SDL_Color& font_color);

	void render_tray_window_direct();

private:
	aplt::tb_api& b_api_;
	aplt::tpinyin& pinyin_;

	tobj_item* scene_obj_item_start_;
	tcb_userdata* scene_userdata_start_;

	// ttray_slot
	tpoint first_coordinate_;

#define BLINK_SHOW_MS	800
#define BLINK_HIDE_MS	600
	struct ttask
	{
		ttask() 
		{ 
			clear(); 
		}

		void set(int max_width, const std::string& _msg, int _duration_ms);
		bool valid() const { return !msg.empty(); }

		void clear()
		{
			msg.clear();
			duration_ms = nposm;
			blink = false;
			font_size = nposm;
			start_ticks = 0;

			next_blink_ticks = 0;
			is_hidden2 = false;
		}

		std::string msg;
		int duration_ms; // if duration_ms is nposm, it is persistent task.
		bool blink;
		int font_size;
		uint32_t start_ticks;

		uint32_t next_blink_ticks; // valid if blink = true
		bool is_hidden2;  // valid if blink = true
	};
	ttask last_task_;
	ttask persistent_task_;
	bool is_session_first_render_;
	uint32_t next_render_idle_ticks_;

	SDL_Color tray_bg_color_;
	int font_size_;

};

#endif
