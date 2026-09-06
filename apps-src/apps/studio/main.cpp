#define GETTEXT_DOMAIN "studio-lib"

#include "base_instance.hpp"
#include "preferences_display.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/home.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/tools.hpp"
#include "gui/dialogs/anim.hpp"
#include "gui/dialogs/opencv.hpp"
#include "gui/dialogs/latex2.hpp"
#include "gui/dialogs/pinyin2.hpp"
#include "gui/widgets/window.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "hotkeys.hpp"
#include "formula_string_utils.hpp"
#include "rose_version.hpp"
#include "mkwin_controller.hpp"
#include "help.hpp"

#include <errno.h>
#include <iostream>

namespace latex {
surface doc_to_surf(const std::string& formula, int max_width) { return nullptr; }
}

surface generate_qr(const std::string& text, int size, int margin, int eccLevel)
{
	VALIDATE(false, "this version doesn's support genrate_qr()");
	return nullptr;
}

class game_instance: public base_instance
{
public:
	game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv);

	void app_fill_anim_tags(std::map<const std::string, int>& tags) override;

	void start_mkwin(const std::map<std::string, std::string>& app_tdomains);
	void edit_animation();

private:
	void app_load_settings_config(const config& cfg) override;
	void app_pre_setmode(tpre_setmode_settings& settings) override;
};

game_instance::game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv)
	: base_instance(ss, argc, argv)
{
	teditor_ editor_(game_config::path);
	editor_.make_system_bins_exist();
}

void game_instance::app_fill_anim_tags(std::map<const std::string, int>& tags)
{
	// although don't use below animation, but pass program verify, define them still.
}

void game_instance::start_mkwin(const std::map<std::string, std::string>& app_tdomains)
{
	hotkey::scope_changer changer(core_cfg(), "hotkey_mkwin");

	mkwin_controller mkwin(core_cfg(), video_, app_tdomains);
	mkwin.initialize(64 * gui2::twidget::hdpi_scale);
	mkwin.main_loop();
}

void game_instance::edit_animation()
{
	gui2::tanim3 dlg(app_cfg_);
	dlg.show();
}

void game_instance::app_load_settings_config(const config& cfg)
{
	game_config::logo_png = cfg["log_png"].str();
	game_config::version = game_config::rose_version;

	game_config::absolute_path = game_config::path + "/absolute";
	game_config::apps_src_path = utils::extract_directory(game_config::path) + "/apps-src";
	if (!check_apps_src_folder(game_config::apps_src_path)) {
		game_config::apps_src_path.clear();
	}

	preferences::set_theme(utils::join_app_prefix_id("rose", "default"));
}

void game_instance::app_pre_setmode(tpre_setmode_settings& settings)
{
	settings.landscape = true;
}

#include <expat/expat.h>

void start_element(void* userdata, const char* name, const char** atts)
{
	int i;
	int* depth_ptr = (int*)userdata;
	SDL_Log("%s", name);

	for (i = 0; atts[i]; i += 2) {
		SDL_Log("\t%s='%s'", atts[i], atts[i + 1]);
	}
	*depth_ptr += 1;
}

void end_element(void* userdata, const char* name)
{
	int* depth_ptr = (int*)userdata;
	*depth_ptr -= 1;
}

void test_xml(const std::string& file_name)
{
	XML_Parser parser = XML_ParserCreate(NULL);
	int done;
	int depth = 0;

	const int one_block = 1024 * 1024;
	tfile file(file_name, GENERIC_READ, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);
	file.resize_data(one_block);

	XML_SetUserData(parser, &depth);
	XML_SetElementHandler(parser, start_element, end_element);
	do {
		int byte_read = posix_fread(file.fp, file.data, one_block);
		if (byte_read == 0) {
			break;
		}
		done = byte_read < file.data_size;
		if (!XML_Parse(parser, file.data, byte_read, done)) {
			SDL_Log("%s at line %d\n", XML_ErrorString(XML_GetErrorCode (parser)), XML_GetCurrentLineNumber (parser));
			return;
		}
	} while (!done);
	XML_ParserFree(parser);		
}

bool did_walk_imageset_path(const std::string& dir, const SDL_dirent2* dirent, std::vector<std::string>& files, const std::vector<std::string>& white_extname)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		// const std::string name = utils::lowercase(dirent->name);
		const std::string name = dirent->name;

		bool hit = false;
		for (std::vector<std::string>::const_iterator it = white_extname.begin(); it != white_extname.end(); ++ it) {
			const std::string& extname = *it;
			size_t pos = name.rfind(extname);
			if (pos != std::string::npos && pos + extname.size() == name.size()) {
				hit = true;
				break;
			}
		}
		if (hit) {
			files.push_back(dir + "/" + dirent->name);
		}
	}
	return true;
}

extern int edit_project_pbxproj();

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	rtc::PhysicalSocketServer ss;
	instance_manager<game_instance> manager(ss, argc, argv, "studio", "#rose");
	game_instance& game = manager.get();

	if (game_config::os == os_windows) {
		// test_xml("C:/ddksample/apps-src/apps/projectfiles/vc/applet.vcxproj.filters");
		// edit_project_pbxproj();
	}

	try {
		if (lobby->enable_chat()) {
			// if want use irc/lobby module(enable_chat_ = ture), you may use bellow lobby->set_nick2(..) set yourself nick. 
			// NOTE: all digit nick is not safied to irc nick protocol.
			lobby->set_nick2("studio");
		}

		std::map<std::string, std::string> app_tdomains;
		for (;;) {
			preferences::set_keyboard_style(keyboard_style_default);

			game.loadscreen_manager().reset();

			gui2::thome::tresult res = gui2::thome::NOTHING;

			const font::floating_label_context label_manager;

			cursor::set(cursor::NORMAL);

			if (res == gui2::thome::NOTHING) {
				// load/reload hero_map from file
				gui2::thome dlg(group.leader());
				dlg.show();
				res = static_cast<gui2::thome::tresult>(dlg.get_retval());
				app_tdomains = dlg.get_app_tdomains();
			}

			if (res == gui2::thome::QUIT_GAME) {
				SDL_Log("do_gameloop, received QUIT_GAME, will exit!\n");
				return 0;

			} else if (res == gui2::thome::WINDOW) {
				game.start_mkwin(app_tdomains);

			} else if (res == gui2::thome::ANIMATION) {
				game.edit_animation();

			} else if (res == gui2::thome::OPENCV) {
				gui2::topencv dlg;
				dlg.show();
			
			} else if (res == gui2::thome::PINYIN) {
				gui2::tpinyin2 dlg;
				dlg.show();
			
			} else if (res == gui2::thome::LATEX) {
				gui2::tlatex2 dlg;
				dlg.show();
			
			} else if (res == gui2::thome::DESIGN) {
				gui2::ttools dlg;
				dlg.show();

			} else if (res == gui2::thome::CHANGE_LANGUAGE) {
				if (game.change_language()) {
					t_string::reset_translations();
					image::flush_cache();
				}

			} else if (res == gui2::thome::MESSAGE) {
				gui2::tchat_slot slot;
				gui2::tchat2 dlg(slot, "chat_module");
				dlg.show();
			
			} else if (res == gui2::thome::EDIT_PREFERENCES) {
				preferences::show_preferences_dialog(game.video());

			}
		}

	} catch (twml_exception& e) {
		e.show();

	} catch (CVideo::quit&) {
		//just means the game should quit
		SDL_Log("SDL_main, catched CVideo::quit\n");

	} catch (game_logic::formula_error& e) {
		gui2::show_error_message(e.what());
	} 

	return 0;
}

int main(int argc, char** argv)
{
	int ver = _MSC_VER;

	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		SDL_SimplerMB("%s", e.user_message.c_str());
	}

	return 0;
}
