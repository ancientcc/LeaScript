#ifndef LIBROSE_TRAY_HPP
#define LIBROSE_TRAY_HPP

#include "rose_sdl_utils.hpp"
#include <SDL_tray.h>

class CVideo;

void tray_entry_cb(void* userdata, SDL_TrayEntry* entry);

class ttray
{
public:
	struct tcb_userdata
	{
		ttray* tray;
		int id;
		int ctx;
	};

	enum {objt_TrayMenu, objt_TrayEntry, objt_count};
	struct tobj_item
	{
		int id;
		int type;
		union {
			SDL_TrayMenu* menu;
			SDL_TrayEntry* entry;
		};
	};

	ttray(CVideo& video)
		: video_(video)
		, tray_(nullptr)
		, cb_userdata_pool_(sizeof(tcb_userdata), 64)
		, obj_items_(sizeof(tobj_item), 64)
	{}

	~ttray()
	{
		if (tray_ != nullptr) {
			// free_tray_objs();
			destroy();
		}
	}
	posix_noncopyable(ttray);

	void create(const surface& surf, const std::string& tooltip);
	void destroy();
	SDL_TrayMenu* insert_TrayMenu(int menu_id);
	SDL_TrayMenu* insert_TraySubmenu(int menu_id, int pos, const std::string& label, int submenu_id);
	SDL_TrayEntry* insert_TrayEntry(int menu_id, int pos, const std::string& label, SDL_TrayEntryFlags flags, int entry_id);

	bool valid() const { return tray_ != nullptr; }
	SDL_Tray& tray()
	{
		VALIDATE(tray_ != nullptr, null_str);
		return *tray_;
	}

	virtual void slice() {}
	virtual void handle_menu_entry(int id, int ctx) {}
	void dump_cb_userdata_pool();
	void dump_obj_items();

protected:
	tcb_userdata* malloc_cb_userdata(int id);
	
	void malloc_TrayMenu_item(int id, SDL_TrayMenu& obj);
	void malloc_TrayEntry_item(int id, SDL_TrayEntry& obj);

	tobj_item& find_obj_item(int id);
	tcb_userdata& find_cb_userdata(int id);
	void quit_app();

private:
	ttray::tobj_item* malloc_obj_item(int id);

protected:
	CVideo& video_;
	SDL_Tray* tray_;
	telem_array_C cb_userdata_pool_;
	telem_array_C obj_items_;
};

#endif
