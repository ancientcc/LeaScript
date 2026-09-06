#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/latex2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"

#include "rose_filesystem.hpp"
#include "rose_config_3rdparty.hpp"
#include "minizip/minizip.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, latex2)

tlatex2::tlatex2()
	: miktex_res_(utils::normalize_path(game_config::preferences_dir + "/../launcher/miktex/sandbox"))
	, zipfile_(game_config::preferences_dir + "/__temp.zip")
	, rspfile_(utils::normalize_path(game_config::preferences_dir + "/../launcher/latex.rsp"))
	, unzipped_dir_(utils::normalize_path(game_config::preferences_dir + "/../launcher/__temp"))
{
}

void tlatex2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	find_widget<tlabel>(window_, "title", false).set_label(_("LaTex"));

	tbutton* button = find_widget<tbutton>(window_, "generate_rsp", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tlatex2::click_generate_rsp
			, this, std::ref(*button)));

	utils::string_map symbols;
	symbols["generate"] = button->label();
	find_widget<tlabel>(window_, "remark", false).set_label(vgettext2("latex remark, $generate", symbols));
}

void tlatex2::post_show()
{
}

// @latex_zip: filename of zip.
bool did_write_rsp_latex(tfile& file, const std::string& bundleid, const version_info& rose_version, int type, const std::string& desc,
	const std::string& latex_zip, int64_t& rsp_ts)
{
	VALIDATE(type >= rsplatextype_min && type <= rsplatextype_max, null_str);

	tfile src(latex_zip, GENERIC_READ, OPEN_EXISTING);
	const int fsize = posix_fsize(src.fp);
	VALIDATE(fsize > 0, null_str);

	const int one_block = 1024 * 1024;
	src.resize_data(one_block);

	// 3.1 trsp_header
	rsp_ts = time(nullptr);
	// part(1/5): rsp header
	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_latex));
	header.version = SDL_FOURCC(0, 0, 0, RSP_LATEX_VER);

	// const time_t t = ts; // for xcode(ios)
	// tm* timeptr = localtime(&t);
	// VALIDATE(timeptr != nullptr, null_str);
	// int build_date = (1900 + timeptr->tm_year) * 10000 + (timeptr->tm_mon + 1) * 100 + timeptr->tm_mday;
	header.build_date = ts_2_build_date(rsp_ts);

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());
	header.zip_size = sizeof(trsp_latex80bytes) + fsize; // will overwrite later.
	posix_fwrite(file.fp, &header, sizeof(header));

	// 3.2 latex80bytes
	trsp_latex80bytes latex_header;
	memset(&latex_header, 0, sizeof(latex_header));
	latex_header.ts = rsp_ts;
	latex_header.type = type;
	SDL_strlcpy(latex_header.desc, desc.c_str(), sizeof(latex_header.desc));
	posix_fwrite(file.fp, &latex_header, sizeof(latex_header));

	// 3.3 latex zip data
	int pos = 0;
	while (pos < fsize) {
		int bytes = one_block;
		if (pos + bytes > fsize) {
			bytes = fsize - pos;
		}
		posix_fread(src.fp, src.data, bytes);
		posix_fwrite(file.fp, src.data, bytes);

		pos += bytes;
	}
	return true;
}

void tlatex2::click_generate_rsp(tbutton& widget)
{
	// VALIDATE(curr_layer_ == UPLOAD_LAYER, null_str);

	std::string err;
	utils::string_map symbols;

	const std::string miktex_res_path_plus1 = miktex_res_ + "/";
	const std::string androidfont_file = miktex_res_path_plus1 + "tex/latex/ctex/fontset/ctex-fontset-windows-androidfont.def";
	const std::string windows_file = miktex_res_path_plus1 + "tex/latex/ctex/fontset/ctex-fontset-windows.def";
	if (!SDL_IsFile(androidfont_file.c_str()) || !SDL_IsFile(windows_file.c_str())) {
		symbols["winfont"] = "ctex-fontset-windows-winfont.def";
		symbols["windows"] = "ctex-fontset-windows.def";
		symbols["androidfont"] = "ctex-fontset-windows-androidfont.def";
		gui2::show_message(null_str, vgettext2("When releasing, rename '$winfont' to '$windows', and keep '$androidfont' as is.", symbols));
		return;
	}

	std::set<std::string> src_dirs;
	src_dirs.insert("bibtex");
	src_dirs.insert("doc");
	src_dirs.insert("dvipdfm");
	src_dirs.insert("dvipdfmx");
	src_dirs.insert("dvips");
	src_dirs.insert("executables");
	src_dirs.insert("fontconfig");
	src_dirs.insert("fonts");
	src_dirs.insert("ghostscript");
	src_dirs.insert("hunspell");
	// src_dirs.insert("luatex-cache");
	// src_dirs.insert("luatexja");
	src_dirs.insert("makeindex");
	src_dirs.insert("metafont");
	src_dirs.insert("metapost");
	src_dirs.insert("miktex");
	src_dirs.insert("pdftex");
	src_dirs.insert("scripts");
	src_dirs.insert("source");
	src_dirs.insert("tex");
	src_dirs.insert("tex4ht");
	src_dirs.insert("tpm");

	std::set<std::string> src_files;
/*
	src_files.insert(APPLET_ICON);
	src_files.insert("settings.cfg");
*/

	std::vector<std::string> input;
	for (std::set<std::string>::const_iterator it = src_dirs.begin(); it != src_dirs.end(); ++ it) {
		const std::string path = miktex_res_path_plus1 + *it;
		if (SDL_IsDirectory(path.c_str())) {
			input.push_back(path);

		} else {
			symbols["file"] = path;
			gui2::show_message(null_str, vgettext2("$file isn't existed, generate fail", symbols));
			return;
		}
	}
	for (std::set<std::string>::const_iterator it = src_files.begin(); it != src_files.end(); ++ it) {
		const std::string path = miktex_res_path_plus1 + *it;
		if (SDL_IsFile(path.c_str())) {
			input.push_back(path);
		} else {
			symbols["file"] = path;
			gui2::show_message(null_str, vgettext2("$file isn't existed, generate fail", symbols));
			return;
		}
	}

	std::vector<std::string> deletes{{unzipped_dir_}, {zipfile_}, {rspfile_}};
	for (std::vector<std::string>::const_iterator it = deletes.begin(); it != deletes.end(); ++ it) {
		const std::string& path = *it;
		SDL_DeleteFiles(path.c_str());
		if (SDL_IsFile(path.c_str()) || SDL_IsDirectory(path.c_str())) {
			symbols["file"] = path;
			err = vgettext2("Failed to delete '$file'.", symbols);
			gui2::show_message(null_str, err);
			return;
		}
	}

	uint32_t start_ticks = SDL_GetTicks();
	bool fok = minizip::zip_file(zipfile_, input, null_str);
	if (!fok) {
		symbols["name"] = utils::extract_file(zipfile_);
		symbols["src"] = miktex_res_path_plus1;
		symbols["dst"] = zipfile_;
		symbols["result"] = fok? _("Success"): _("Fail");
		err = vgettext2("Generate rsp. [1/2]Zip $name from \"$src\" to \"$dst\", $result!", symbols);
		if (game_config::os == os_windows) {
			err = utils::normalize_path(err, true);
		}
		gui2::show_message(null_str, err);
		return;
	}
	SDL_Log("[1/2]minizip::zip_file(%s) coust %u ms", zipfile_.c_str(), SDL_GetTicks() - start_ticks);

	const std::string latex_rsp = rspfile_;
	int64_t rsp_ts = 0;
	const std::string bundleid = "latex.leagor.miktex";
	const std::string desc = _("2025-09-15 Physics homework");
	start_ticks = SDL_GetTicks();
	tsha1writer sha1file(latex_rsp, nposm, std::bind(&gui2::did_write_rsp_latex, _1, bundleid, std::ref(game_config::rose_version), 
		rsplatextype_miktex, desc, zipfile_, std::ref(rsp_ts)));
	sha1file.write();
	SDL_Log("[2/2]sha1file(%s) coust %u ms", latex_rsp.c_str(), SDL_GetTicks() - start_ticks);

	symbols["rspfile"] = rspfile_;
	gui2::show_message(null_str, vgettext2("Generate rsp finished. file: $rspfile", symbols));
}

} // namespace gui2

