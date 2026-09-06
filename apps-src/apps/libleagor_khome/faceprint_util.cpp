#define GETTEXT_DOMAIN "aplt_leagor_khome-lib"

#include "faceprint_util.hpp"
#include "rose_filesystem.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include "json/json.h"
// #include "game_config.hpp"
// #include "net.hpp"
#include "rose_font.hpp"
#include "face_sdk.hpp"

#include <iomanip>
#include <openssl/sha.h>
#include <opencv2/imgproc.hpp>

using namespace std::placeholders;

#ifdef USE_DFACE

namespace faceprint {

std::string res_dir;
std::string preferences_dir;
std::string sn_path;
bool oem_3s2 = false;
bool use_facestore = true;
std::set<int> valid_feature_blob_sizes;
std::map<int, tst_model> st_models;
std::string activecode_file_name;
int dtype = nposm;
int FEATURE_BLOB_SIZE = nposm;
int MAXIMUM_FACES = nposm;

int face_sdk = nposm;
std::map<int, std::string> face_sdks;
std::unique_ptr<threading::mutex> facestore_mutex;
tlist_feature list_features[MAX_CAMERAS];

void copy_model_2_preferences(const std::string& src_dir)
{
	VALIDATE(!res_dir.empty(), null_str);
	VALIDATE(!st_models.empty(), null_str);
	// copy font directory from res/tflites to user_data_dir/tflites
	VALIDATE(src_dir == res_dir + "/tflites", null_str);

	std::vector<std::string> v;
	// st_sdk1 relative
	for (std::map<int, tst_model>::const_iterator it = st_models.begin(); it != st_models.end(); ++ it) {
		const tst_model& model = it->second;
		if (SDL_strcmp(model.verify_model_path.c_str(), FAKE_MODEL_STR) != 0) {
			v.push_back(it->second.verify_model_path);
		}
	}

	const bool copy_v8_small_rknn = false;

	// df_sdk relatvie
	v.push_back("model/ce.em");
	v.push_back("model/cm.em");
	v.push_back("model/cn.em");
	v.push_back("model/fds.rknn");
	v.push_back("model/ocum.em");
	v.push_back("model/pdrt.rknn");
	v.push_back("model/qgme.rknn");
	v.push_back("model/rnet.rknn");
	v.push_back("model/shc.rknn");
	v.push_back("model/v3.rknn");
	if (copy_v8_small_rknn) {
		v.push_back("model/v8_small.rknn");  // 37.4MB
	}
	v.push_back("model/vmusbface.rknn");

	const std::string dst_dir = preferences_dir + "/tflites";
	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const std::string& postfix = *it;
		const std::string src = src_dir + "/" + postfix;
		const std::string dst = dst_dir + "/" + postfix;
		if (!file_exists(dst)) {
			bool ok = create_directory_if_missing(utils::extract_directory(dst));
			VALIDATE(ok, null_str);

			tfile src_file(src, GENERIC_READ, OPEN_EXISTING);
			int32_t fsize = src_file.read_2_data();
			VALIDATE(fsize > 0, null_str);

			tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
			VALIDATE(dst_file.valid(), null_str);
			posix_fwrite(dst_file.fp, src_file.data, fsize);
		}
	}
}

std::map<int, tst_model> get_st_models(const std::string& str)
{
	std::map<int, tst_model> result;
	std::vector<std::string> vstr = utils::split(str, ';');
	VALIDATE(!vstr.empty(), null_str);
	const std::string src = res_dir;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		std::vector<std::string> vstr2 = utils::split(*it, ',');
		VALIDATE(vstr2.size() == 3, null_str);
		const int feature_size = utils::to_int(vstr2[0]);
		VALIDATE(valid_feature_blob_sizes.count(feature_size) != 0, null_str);
		VALIDATE(result.count(feature_size) == 0, null_str);

		std::string fullpath;
		if (SDL_strcmp(vstr2[1].c_str(), FAKE_MODEL_STR) != 0) {
			fullpath = src + "/tflites/" + vstr2[1];
			SDL_bool ok = SDL_IsFile(fullpath.c_str());
			VALIDATE(ok, null_str);
		}

		fullpath = src + "/cert/" + vstr2[2];
		std::string example_feature_dat;
		{
			tfile file(fullpath, GENERIC_READ, OPEN_EXISTING);
			int fsize = file.read_2_data();
			VALIDATE(fsize == feature_size, null_str);
			example_feature_dat.assign(file.data, fsize);
		}

		// result.insert(std::make_pair(feature_size, tst_model(feature_size, vstr2[1], vstr2[2])));
		result.insert(std::make_pair(feature_size, tst_model(feature_size, vstr2[1], example_feature_dat)));
	}
	return result;
}

static bool did_read_activecode_dat(tfile& file, int64_t dsize, bool bak, int& lfsize)
{
	int fsize = file.read_2_data();
	lfsize = SDL_max(fsize, lfsize);
	VALIDATE(fsize == dsize, null_str);
	bool ret = tface_sdk::authorize(false, file.data);

	SDL_Log("{st_sdk}did_read_activecode_dat, offline, bak: %s, fsize: %d, ret: %s", bak? "true": "false", fsize, ret? "true": "false");
	return ret;
}

bool authorize(bool onlinestsdk)
{
	// To ensure sensetime.lic does not outflow, Released APK must use offline methods.
	VALIDATE(!onlinestsdk, null_str);
	SDL_Log("{st_sdk}[%s]authorize start...", onlinestsdk? "online": "offline");
	bool ret = false;
	if (onlinestsdk) {
		ret = tface_sdk::authorize(true);
		if (!ret) {
			return false;
		}

	} else {
		VALIDATE(!activecode_file_name.empty(), null_str);
		const std::string fname = preferences_dir + "/cert/" + activecode_file_name;

		int lfsize = 0;
		std::unique_ptr<tbakreader> file;
		file.reset(new tbakreader(fname, true, std::bind(&did_read_activecode_dat, _1, _2, _3, std::ref(lfsize))));
/*		
		// there maybe tow file in tbakreader, I don't judge both file, so use stid_facepro_activate forbition.
		bool require_get_activecode = false;
		utils::string_map symbols;
		symbols["sn"] = game_config::sn;
		while (!ret) {
			if (require_get_activecode) {
				// get file from netwrk
				SDL_Log("{st_sdk}authorize, offline, require get activecode from network");
				file.reset(nullptr); // do_activecode_file will write this file. must close it.
				if (!net::do_activecode_file(game_config::sn, lfsize)) {
					SDL_Log("{st_sdk}authorize, offline, Get activecode from server fail, exit");
					net::do_reporterror(errcode_stsdk, nposm, vgettext2("Get activecode($sn) from server fail", symbols), true);
					return false;
				}
				file.reset(new tbakreader(fname, true, std::bind(&did_read_activecode_dat, _1, _2, _3, std::ref(lfsize))));
				// if success, download file always valid first file in tbakreader.
				if (!file->valid() || posix_fsize(file->fp) == 0) {
					return false;
				}
			}
			if (file->read() > 0) {
				ret = true;

			}
			if (!ret) {
				if (!require_get_activecode) {
					SDL_Log("{st_sdk}authorize, offline, stid_facepro_activate fail, require_get_activecode = true");
					require_get_activecode = true;
				} else {
					symbols.clear();
					symbols["sn"] = game_config::sn;
					symbols["activecode_file"] = utils::extract_file(game_config::sn_path) + "/" + get_url_activecode_file_name(game_config::sn);

					std::stringstream err;
					err << "[" << faceprint::face_sdks.find(face_sdk)->second << "]";
					err << (game_config::dtype != dtype_gauth? vgettext2("sensetime^activate fail help", symbols): vgettext2("sensetime^gauth activate fail help", symbols));
					std::string extra_msg = tface_sdk::authorize_extra_err_msg();
					if (!extra_msg.empty()) {
						err << "\n\n" << extra_msg;
					}
					gui2::show_message(null_str, err.str());
					return false;
				}
			}
		}
*/
	}

	return true;
}

int sizeof_tcs_feature = 0;
bool facestore_initialized = false;
bool gauth_facestore_init_called = false;
int list_size = 0;
// why use list_feature? when sensetime compare, use it as input features memory block.
char** list_feature_base = nullptr;
int list_cs_vsize = 0;
tcs_feature* list_cs_feature = nullptr;
int64_t facestore_ts = 0;
int max_sizeof_tcs_feature = nposm;

void tface::blend_write_face(surface& surf, cv::Mat& mat, const face_face_t& face, int index, bool is_best) const
{
	VALIDATE(surf->w == mat.cols && surf->h == mat.rows, null_str);
	cv::Scalar color = cv::Scalar(0, 0, 255, 255);
	if (is_best) {
		color = cv::Scalar(0, 255, 0, 255);
	}

	const int left = face.rect.left >= 0? face.rect.left: 0;
	const int right = face.rect.right < mat.cols? face.rect.right: mat.cols - 1;
	const int top = face.rect.top >= 0? face.rect.top: 0;
	const int bottom = face.rect.bottom < mat.rows? face.rect.bottom: mat.rows - 1;

	cv::Rect roi(left, top, right - left, bottom - top);
	cv::rectangle(mat, roi, color);

	char score_c_str[32];
	if (roi.width > 0 && roi.height > 0) {
		SDL_snprintf(score_c_str, sizeof(score_c_str), "#%i(%.1f%%, %.1f%%)", index, face.score * 100, face.quality * 100);

	} else {
		SDL_snprintf(score_c_str, sizeof(score_c_str), "#%i(%.1f%%, -)", index, face.score * 100);
	}

	surface text_surf = font::rose_get_rendered_text(score_c_str, 0, font::SIZE_SMALLER, font::BLUE_COLOR);
	SDL_Rect dst_rect{face.rect.left, face.rect.top, text_surf->w, text_surf->h};
	sdl_blit(text_surf, nullptr, surf, &dst_rect);
}

tface_sdk* tface_sdk::create()
{
    tface_sdk* sdk = nullptr;
    if (face_sdk == st_sdk1) {
        // sdk = create_st_sdk1();

    } else if (face_sdk == st_sdk2) {
        sdk = create_st_sdk2();

    } else {
        VALIDATE(false, null_str);
    }
    return sdk;
}

void recalculate_sizeof_tcs_feature()
{
	const int personversion = slot_personversion();
	VALIDATE(valid_feature_blob_sizes.count(FEATURE_BLOB_SIZE) != 0, null_str);
	if (personversion == personversion_v0) {
		sizeof_tcs_feature = sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE;

	} else {
		VALIDATE(personversion == personversion_v9, null_str);
		sizeof_tcs_feature = sizeof(tcs_feature_v9) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE;
	}
}

void allocate_list_feature(int max_faces)
{
	VALIDATE(use_facestore, null_str);
	VALIDATE(max_faces > 0, null_str);

	if (!facestore_mutex.get()) {
		facestore_mutex.reset(new threading::mutex);
	}

	threading::lock lock(*facestore_mutex.get());

	VALIDATE(max_faces >= MAXIMUM_FACES && max_faces == MAXIMUM_FACES, null_str);
	VALIDATE(list_feature_base == nullptr && list_size == 0 && list_cs_feature == nullptr && list_cs_vsize == 0 && facestore_ts == 0, null_str);

	list_size = max_faces;
	list_feature_base = (char**)malloc(sizeof(char*) * max_faces * MAX_CAMERAS);
	memset(list_feature_base, 0, sizeof(char*) * max_faces * MAX_CAMERAS);

	memset(list_features, 0, sizeof(list_features));
	for (int at = 0; at < MAX_CAMERAS; at ++) {
		list_features[at].features = list_feature2(at);
		SDL_Log("%i, #%i 0x%p + 0x%x = list_feature: 0x%p", (int)sizeof(list_feature_base[0]), at, 
			list_feature_base, (int)sizeof(char*) * MAXIMUM_FACES * at, list_features[at].features);
	}
	{
		char** ptr = list_feature_base;
		int size1 = sizeof(char*) * MAXIMUM_FACES * MAX_CAMERAS;
		SDL_Log("0x%p + %i = 0x%p", ptr, size1, ptr + size1);
	}

	// app may switch between tcs_feature_v9/tcs_feature_v7/tcs_feature_v0, 
	// so must not use sizeof_tcs_feature. memset in clear_list_feature maybe result Memory-access-exception.
	max_sizeof_tcs_feature = sizeof(tcs_feature_v9) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE;
	list_cs_feature = (tcs_feature*)malloc(max_sizeof_tcs_feature * max_faces);
	memset(list_cs_feature, 0, max_sizeof_tcs_feature * max_faces);
}

void free_list_feature()
{
	VALIDATE(use_facestore, null_str);

	threading::lock lock(*facestore_mutex.get());

	VALIDATE(list_feature_base != nullptr && list_cs_feature != nullptr, null_str);
	free(list_cs_feature);
	list_cs_vsize = 0;
	list_cs_feature = nullptr;

	free(list_feature_base);
	memset(list_features, 0, sizeof(list_features));
	list_size = 0;
	list_feature_base = nullptr;

	facestore_ts = 0;
}

static void validate_facestore(const int compare_at, const int compare_faceid)
{
	tlist_feature& list_lessv9 = list_features[LE102_INDEX];
	std::set<int> faceids;

	const int positions = slot_positions();
	const int personversion = slot_personversion();
	if (support_subfacestore(personversion)) {
		for (int at = 0; at < positions; at ++) {
			tlist_feature& list = list_features[at];
			VALIDATE(list_cs_vsize >= list.vsize, null_str);
			list.validate_tmp = 0;
		}
	} else {
		VALIDATE(list_cs_vsize == list_lessv9.vsize, null_str);
	}
	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		// const cs_feature_ptr_variable(e, cs_feature) = cs_feature_ptr_from_list(i);
		if (support_subfacestore(personversion)) {
			const tcs_feature_bh_v9* cs_feature_bh = (tcs_feature_bh_v9*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
			for (int at = 0; at < positions; at ++) {
				if (cs_feature_bh->camera_mask & (1 << at)) {
					tlist_feature& list = list_features[at];
					VALIDATE(cs_feature->feature == list.features[list.validate_tmp ++], null_str);
				}
			}
		} else {
			VALIDATE(cs_feature->feature == list_lessv9.features[i], null_str);
		}
		VALIDATE(cs_feature->ts <= facestore_ts, null_str);

		if (compare_at != nposm) {
			// Why use compare_at != nposm branch?
			// --- To reduce cpu required for validation. There is no need to touch faceids.
			if (i != compare_at) {
				// other line must not equal compare_at line.
				VALIDATE(cs_feature->faceid != compare_faceid, null_str);
			}
		} else {
			// compare all line.
			VALIDATE(faceids.count(cs_feature->faceid) == 0, null_str);
			faceids.insert(cs_feature->faceid);
		}
	}

	if (support_subfacestore(personversion)) {
		for (int at = 0; at < positions; at ++) {
			tlist_feature& list = list_features[at];
			VALIDATE(list.vsize == list.validate_tmp, null_str);
		}
	}
}

static void card_string_2_u32(tcs_feature& cs_feature)
{
	const int inputcardtype = slot_inputcardtype();
	const int terminate_at = MAX_CS_CARDNUMBER_SIZE - 5;
	cs_feature.card[terminate_at] = '\0';
	uint32_t u32;
	if (inputcardtype == inputcardtype_wg34base16) {
		u32 = utils::to_uint32(cs_feature.card, true);
	} else {
		// think is inputcardtype_wg34base10
		u32 = utils::to_uint32(cs_feature.card);
	}
	memcpy(cs_feature.card + terminate_at + 1, &u32, 4);
}

void load_facestore(int64_t server_facestore_ts, const char* data, int size, bool sha1)
{
	VALIDATE(use_facestore, null_str);

	threading::lock lock(*facestore_mutex.get());

	VALIDATE(!list_cs_vsize && !facestore_ts, null_str);
	char** features_tmp = list_feature_base;
	for (int at = 0; at < MAX_CAMERAS; at ++) {
		tlist_feature& list = list_features[at];
		VALIDATE(list.features == features_tmp && list.vsize == 0, null_str);
		features_tmp += MAXIMUM_FACES;
	}

	const int positions = slot_positions();
	const int personversion = slot_personversion();
	if (support_subfacestore(personversion)) {
		// VALIDATE(positions > 0, null_str);
	}
	facestore_ts = server_facestore_ts;

	if (!size) {
		// in thie scene, server_facestore_ts maybe not 0.
		// insert some face, then delete them all. server_facestore_ts will is the ts last face deleted.
		// so must valuate server_facestore_ts to facestore_ts.
		return;
	}

	if (sha1) {
		// get rid of sha1
		VALIDATE(size > SHA_DIGEST_LENGTH, null_str);
		size -= SHA_DIGEST_LENGTH;
		std::unique_ptr<uint8_t[]> md = utils::sha1((const uint8_t*)data, size);
		if (memcmp(md.get(), data + size, SHA_DIGEST_LENGTH) != 0) {
			return;
		}
	}

	const int items = size / sizeof_tcs_feature;
	VALIDATE((size % sizeof_tcs_feature) == 0, null_str);
	memcpy(list_cs_feature, data, size);
	list_cs_vsize = items;

	tlist_feature& list_lessv9 = list_features[LE102_INDEX];

	for (int i = 0; i < items; i ++) {
		tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		card_string_2_u32(*cs_feature);
		if (support_subfacestore(personversion)) {
			const tcs_feature_bh_v9* cs_feature_bh = (tcs_feature_bh_v9*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
			for (int at = 0; at < positions; at ++) {
				if (cs_feature_bh->camera_mask & (1 << at)) {
					tlist_feature& list = list_features[at];
					list.features[list.vsize ++] = cs_feature->feature;
				}
			}
		} else {
			list_lessv9.features[list_lessv9.vsize ++] = cs_feature->feature;
		}

	}

	validate_facestore(nposm, nposm);
}

void clear_list_feature()
{
	VALIDATE(use_facestore, null_str);

	threading::lock lock(*facestore_mutex.get());

	VALIDATE(list_feature_base && list_cs_feature, null_str);

	// backtrace:
	//     #00 pc 00012c48  /system/lib/libc.so (memset+76)
	//     #01 pc 00738219  /data/app/com.leagor.iaccess-1/lib/arm/libmain.so (faceprint::clear_list_feature()+120)
	//     #02 pc 0077926d  /data/app/com.leagor.iaccess-1/lib/arm/libmain.so (net::do_login2(std::__ndk1::basic_string<char, std::__ndk1::char_traits<char>, std::__ndk1::allocator<char> > const&, std::__ndk1::basic_string<char, std::__ndk1::char_traits<char>, std::__ndk1::allocator<char> > const&, int, bool)+2352)
	//     #03 pc 00a2dae3  /data/app/com.leagor.iaccess-1/lib/arm/libmain.so (gui2::tprogress::app_first_drawn()+154)
	SDL_Log("clear_list_feature, [1], sizeof_tcs_feature: %i, list_size: %i, list_cs_vsize: %i", sizeof_tcs_feature, list_size, list_cs_vsize);

	memset(list_cs_feature, 0, max_sizeof_tcs_feature * list_size);
	list_cs_vsize = 0;

	SDL_Log("clear_list_feature, [2], sizeof(char*): %i, list_size: %i, list_feautres[0].vsize: %i", (int)sizeof(char*), list_size, list_features[0].vsize);
	memset(list_feature_base, 0, sizeof(char*) * list_size * MAX_CAMERAS);
	for (int at = 0; at < MAX_CAMERAS; at ++) {
		list_features[at].vsize = 0;
	}

	SDL_Log("clear_list_feature, [3], X");

	facestore_ts = 0;
}

void do_modify_person_v9(int positions, int personversion, tcs_feature* cs_feature)
{
	VALIDATE(support_subfacestore(personversion), null_str);
	const tcs_feature_bh_v9* cs_feature_bh = (tcs_feature_bh_v9*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);

	// const int positions = current_user.camera.positions.size();
	for (int camera_at = 0; camera_at < positions; camera_at ++) {
		tlist_feature& list = list_features[camera_at];
		bool has_existed = false;
		int n = 0;
		for (; n < list.vsize; n ++) {
			if (list.features[n] >= cs_feature->feature) {
				has_existed = list.features[n] == cs_feature->feature;
				break;
			}
		}
		if (cs_feature_bh->camera_mask & (1 << camera_at)) {
			// require be existed
			if (has_existed) {
				// both status is same, do nothing.
				continue;
			}
			// insert at n
			if (n != list.vsize) {
				memmove(list.features + n + 1, list.features + n, (list.vsize - n) * 4);
			}
			list.features[n] = cs_feature->feature;
			list.vsize ++;

		} else {
			// require be not existed
			if (!has_existed) {
				// both status is same, do nothing.
				continue;
			}
			// delete n
			if (n < list.vsize - 1) {
				memcpy(list.features + n, list.features + n + 1, (list.vsize - n - 1) * 4);
			}
			list.features[list.vsize - 1] = nullptr;
			list.vsize --;
		}
	}
}

void sync_facestore(int64_t server_facestore_ts, const char* data, int size, int modifies, bool sha1)
{
	VALIDATE(use_facestore, null_str);
	VALIDATE(facestore_initialized, null_str);

	threading::lock lock(*facestore_mutex.get());

	VALIDATE(server_facestore_ts >= facestore_ts, null_str);
	facestore_ts = server_facestore_ts;

	if (size == 0) {
		VALIDATE(modifies == 0, null_str);
		return;
	}

	if (sha1) {
		// get rid of sha1
		VALIDATE(size > SHA_DIGEST_LENGTH, null_str);
		size -= SHA_DIGEST_LENGTH;
		std::unique_ptr<uint8_t[]> md = utils::sha1((const uint8_t*)data, size);
		if (memcmp(md.get(), data + size, SHA_DIGEST_LENGTH) != 0) {
			return;
		}
	}

	VALIDATE(size >= modifies * sizeof_tcs_feature, null_str);
	int dels_size = size - modifies * sizeof_tcs_feature;
	VALIDATE((dels_size % 4) == 0, null_str);

	// modify or insert section
	tlist_feature& list_lessv9 = list_features[LE102_INDEX];
	const int positions = slot_positions();
	const int personversion = slot_personversion();
	for (int i = 0; i < modifies; i ++) {
		const tcs_feature* cs_feature_m = cs_feature_ptr_from_list2(data, i);
		int n = 0;
		for (; n < list_cs_vsize; n ++) {
			tcs_feature* cs_feature = cs_feature_ptr_from_list(n);
			if (cs_feature->faceid == cs_feature_m->faceid) {
				// it is modify.
				int cmp = memcmp(cs_feature->feature, cs_feature_m->feature, FEATURE_BLOB_SIZE);
				memcpy(cs_feature, cs_feature_m, sizeof_tcs_feature);
				card_string_2_u32(*cs_feature);
				if (support_subfacestore(personversion)) {
					// >=v9 version
					// For subfacestore, modification are only made when "position" changes, and once "position" changes, 
					// it is certain that the face library will reload. For this time, don't have to consider the modifying scene.
					do_modify_person_v9(positions, personversion, cs_feature);
				}
				facestore_dirty = true;
				break;
			}
		}
		if (n == list_cs_vsize) {
			// it is insert.
			tcs_feature* cs_feature = cs_feature_ptr_from_list(list_cs_vsize);
			list_cs_vsize ++;
			memcpy(cs_feature, cs_feature_m, sizeof_tcs_feature);
			card_string_2_u32(*cs_feature);
			if (support_subfacestore(personversion)) {
				const tcs_feature_bh_v9* cs_feature_bh = (tcs_feature_bh_v9*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
				for (int at = 0; at < positions; at ++) {
					if (cs_feature_bh->camera_mask & (1 << at)) {
						tlist_feature& list = list_features[at];
						list.features[list.vsize ++] = cs_feature->feature;
					}
				}
			} else {
				list_lessv9.features[list_lessv9.vsize ++] = cs_feature->feature;
			}
			facestore_dirty = true;
		}
		validate_facestore(n, cs_feature_ptr_from_list(n)->faceid);
	}

	// delete faceid section
	const char* del_ptr = data + modifies * sizeof_tcs_feature;
	int faceid;
	for (int i = 0; i < dels_size; i += 4) {
		// as if tcs_feature has (4+8+8+701) byte, why use memcpy simply?
		// 1)deleting is not a frequent operator.
		// 2)As time accumulates, a boundary will gradually generate. upper is the owner, lower is temporary visitors. memcpy is only part of the temporary visitor.
		memcpy(&faceid, del_ptr + i, 4);
		int at = list_cs_vsize - 1;
		for (; at >= 0; at --) {
			tcs_feature* cs_feature = cs_feature_ptr_from_list(at);
			if (cs_feature->faceid == faceid) {
				// it is delete.
				const char* deleted_feature = cs_feature->feature;
				if (at < list_cs_vsize - 1) {
					memcpy(cs_feature_ptr_from_list(at), cs_feature_ptr_from_list(at + 1), (list_cs_vsize - at - 1) * sizeof_tcs_feature);
					if (support_subfacestore(personversion)) {
						for (int camera_at = 0; camera_at < positions; camera_at ++) {
							tlist_feature& list = list_features[camera_at];
							int n = 0;
							bool require_delete = false;
							for (; n < list.vsize; n ++) {
								if (list.features[n] >= deleted_feature) {
									require_delete = list.features[n] == deleted_feature;
									break;
								}
							}
							if (n != list.vsize) {
								// features[i] ptr is calculated list_cs_feature[n] + feature's offset.
								// since has one tcs_feature is delete, follow-up features[i] require be forward one sizeof_tcs_feature.
								if (require_delete) {
									// this list has one delete.
									for (; n < list.vsize - 1; n ++) {
										list.features[n] = list.features[n + 1] - sizeof_tcs_feature;
									}
									// in order to make below code execute list.vsize --, set last feature to nullptr
									list.features[list.vsize - 1] = nullptr;
								} else {
									// this list hasn't item delete.
									for (; n < list.vsize; n ++) {
										list.features[n] = list.features[n] - sizeof_tcs_feature;
									}
								}
							}
						}
					} else {
						for (int n = at; n < list_cs_vsize; n ++) {
							// don't use memcpy. cs_feature.feature is address depend. above memcpy will modify it.
							tcs_feature* cs_feature2 = cs_feature_ptr_from_list(n);
							list_lessv9.features[n] = cs_feature2->feature;
						}
					}
				} else if (support_subfacestore(personversion)) {
					for (int camera_at = 0; camera_at < positions; camera_at ++) {
						tlist_feature& list = list_features[camera_at];
						if (list.vsize > 0 && list.features[list.vsize - 1] == deleted_feature) {
							// in order to make below code execute list.vsize --, set last feature to nullptr
							list.features[list.vsize - 1] = nullptr;
						}
					}
				}

				list_cs_vsize --;
				memset(cs_feature_ptr_from_list(list_cs_vsize), 0, sizeof_tcs_feature);
				if (support_subfacestore(personversion)) {
					for (int camera_at = 0; camera_at < positions; camera_at ++) {
						tlist_feature& list = list_features[camera_at];
						if (list.vsize > 0) {
							const char* back = list.features[list.vsize - 1];
							// Must not use "if (back == nullptr || back == deleted_feature)"
							// why? --back maybe modified above code: list.features[n] = list.features[n] - sizeof_tcs_feature;
							// is not original value.
							if (back == nullptr) {
								list.features[list.vsize - 1] = nullptr;
								list.vsize --;
							}
						}
					}
				} else {
					list_lessv9.vsize --;
					list_lessv9.features[list_lessv9.vsize] = nullptr;
				}
				facestore_dirty = true;
				break;
			}
		}
		if (at == -1) {
			// it is possible that local doesn't exist this faceid. do nothing.
			continue;
		}
		VALIDATE(at >= 0, null_str);
		validate_facestore(nposm, nposm);
	}
}

const tcs_feature* is_card_existed(const uint32_t card)
{
	threading::lock lock(*facestore_mutex.get());
	const int card_at = MAX_CS_CARDNUMBER_SIZE - 4;

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		uint32_t tmp_card;
		memcpy(&tmp_card, cs_feature->card + card_at, 4);
		if (tmp_card == card) {
			return cs_feature;
		}
	}
	return nullptr;
}

const tcs_feature* is_faceid_existed(const int faceid)
{
	threading::lock lock(*facestore_mutex.get());

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->faceid == faceid) {
			return cs_feature;
		}
	}
	return nullptr;
}

uint32_t faceid_2_card(int faceid)
{
	threading::lock lock(*facestore_mutex.get());
	const int card_at = MAX_CS_CARDNUMBER_SIZE - 4;

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->faceid == faceid) {
			uint32_t tmp_card;
			memcpy(&tmp_card, cs_feature->card + card_at, 4);
			return tmp_card;
		}
	}
	VALIDATE(false, null_str);
	return 0;
}
/*
bool in_blacklist(int faceid)
{
	threading::lock lock(*facestore_mutex.get());

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->faceid == faceid) {
			return PERSONTYPE_FROM_PERSONTYPE2(cs_feature->persontype2) == game_config::blacklist_persontype;
		}
	}
	VALIDATE(false, null_str);
	return false;
}
*/

const tcs_feature_bh_v9* faceid_2_cs_feature_bh_v9(int faceid, bool must_exist)
{
	VALIDATE_IN_MAIN_THREAD();

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->faceid == faceid) {
			return (tcs_feature_bh_v9*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
		}
	}
	if (must_exist) {
		VALIDATE(false, null_str);
	}
	return nullptr;
}

void tcs_feature_2_cs_person(const tcs_feature* cs_feature, tcs_person& person)
{
	person.faceid = cs_feature->faceid;
	person.ts = cs_feature->ts;
	person.persontype = PERSONTYPE_FROM_PERSONTYPE2(cs_feature->persontype);
	if (person.persontype < 0) {
		person.persontype = 0;
	}
	const int card_at = MAX_CS_CARDNUMBER_SIZE - 4;
	person.card_str = cs_feature->card;
	memcpy(&(person.card), cs_feature->card + card_at, 4);

	person.idtype = cs_feature->idtype;
	person.idnumber = utils::truncate_to_max_bytes(cs_feature->idnumber, nposm, MAX_CS_IDNUMBER_SIZE);
	person.name = utils::truncate_to_max_bytes(cs_feature->name, nposm, MAX_CS_NAME_SIZE);
	person.house = utils::lowercase(utils::truncate_to_max_bytes(cs_feature->house, nposm, MAX_CS_HOUSE_SIZE));

	const int personversion = slot_personversion();
	if (personversion >= personversion_v9) {
		const tcs_feature_bh_v9* cs_feature_bh = (tcs_feature_bh_v9*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
	}
}

tcs_person faceid_2_cs_person(int faceid)
{
	VALIDATE_IN_MAIN_THREAD();

	tcs_person person;
	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->faceid == faceid) {
			tcs_feature_2_cs_person(cs_feature, person);
			return person;
		}
	}

	VALIDATE(false, null_str);
	return person;
}

tcs_person cardnumber_2_cs_person(const uint32_t cardnumber)
{
	VALIDATE_IN_MAIN_THREAD();

	tcs_person person;
	const int card_at = MAX_CS_CARDNUMBER_SIZE - 4;
	uint32_t tmp_card;

	if (cardnumber == 0) {
		return person;
	}

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		memcpy(&tmp_card, cs_feature->card + card_at, 4);
		if (tmp_card == cardnumber) {
			tcs_feature_2_cs_person(cs_feature, person);
			return person;
		}
	}
	return person;
}

void recalculate_card_u32(int inputcardtype)
{
	VALIDATE_IN_MAIN_THREAD();
	threading::lock lock(*facestore_mutex.get());

	for (int i = 0; i < list_cs_vsize; i ++) {
		tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		card_string_2_u32(*cs_feature);
	}
}

static std::string facestore_dat()
{
	return preferences_dir + "/facestore.dat";
}

static bool did_write_facestore_dat(tfile& file, int personversion)
{
	// as if list_cs_vsize == 0, write file.
	threading::lock lock(*facestore_mutex.get());

	tfacestore_header header;
	header.fourcc = SDL_FOURCC('F', 'S', 'D', 'T');
	header.version = posix_mku32(FEATURE_BLOB_SIZE, posix_mku16(dtype, personversion));
	header.count = list_cs_vsize;
	header.facestore_ts = facestore_ts;
	
	posix_fwrite(file.fp, &header, sizeof(tfacestore_header));
	if (list_cs_vsize) {
		posix_fwrite(file.fp, list_cs_feature, list_cs_vsize * sizeof_tcs_feature);
	}
	return true;
}

bool facestore_dirty = false;
uint32_t last_write_facestore_ticks = 0;

void write_facestore_dat()
{
	VALIDATE(use_facestore, null_str);
	// VALIDATE(dtype == dtype_access || dtype == dtype_gauth, null_str);
	VALIDATE(facestore_dirty, null_str);

	int personversion = slot_personversion();
	bool use_backup = !oem_3s2;
	tsha1writer writer(facestore_dat(), use_backup, std::bind(&did_write_facestore_dat, _1, personversion));
	writer.write();

	last_write_facestore_ticks = 0;
	facestore_dirty = false;
	SDL_Log("%u, write_facestore_dat, list_features[0].vsize: %i", SDL_GetTicks(), list_features[0].vsize);
}

static bool did_load_facestore_dat(tfile& file, int64_t dsize)
{
	if (dsize < sizeof(tfacestore_header)) {
		return false;
	}
	tfacestore_header header;
	posix_fread(file.fp, &header, sizeof(header));
	if (header.fourcc != SDL_FOURCC('F', 'S', 'D', 'T')) {
		return false;
	}
	const int personversion = slot_personversion();
	if (header.version != posix_mku32(FEATURE_BLOB_SIZE, posix_mku16(dtype, personversion))) {
		return false;
	}
	if (header.count == 0 || (int)header.count > MAXIMUM_FACES) {
		return false;
	}
	if (dsize != (int)(sizeof(tfacestore_header) + header.count * sizeof_tcs_feature)) {
		return false;
	}

	posix_fseek(file.fp, sizeof(tfacestore_header));
	file.resize_data(header.count * sizeof_tcs_feature);
	posix_fread(file.fp, file.data, header.count * sizeof_tcs_feature);

	threading::lock lock(*facestore_mutex.get());
	clear_list_feature();
	load_facestore(header.facestore_ts, file.data, dsize - sizeof(tfacestore_header), false);
	return true;
}

void load_facestore_dat()
{
	VALIDATE(!facestore_initialized, null_str);
	tsha1reader reader(facestore_dat(), true, std::bind(&did_load_facestore_dat, _1, _2));
	reader.read();
}

static int max_faceid_in_cs_features()
{
	int max_faceid = 0;

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->faceid > max_faceid) {
			max_faceid = cs_feature->faceid;
		}
	}
	return max_faceid;
}

static int64_t max_ts_in_cs_features()
{
	int64_t max_ts = 0;

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->ts > max_ts) {
			max_ts = cs_feature->ts;
		}
	}
	return max_ts;
}

void gauth_sync_facestore_erase(const std::set<std::string>& signouted_ids, int64_t max_ts)
{
	// I hope max_ts > facestore_ts, but agbox don't it.
	// VALIDATE(max_ts > facestore_ts, null_str);
	if (max_ts < facestore_ts) {
		max_ts = facestore_ts;
	}

	// section#1, delete feature in facestore that in this sync-getPersonList's signouted list.
	if (list_cs_vsize != 0) {
		const int item_size = 4;
		int* delete_data = (int*)malloc(list_cs_vsize * item_size);
		memset(delete_data, 0, list_cs_vsize * item_size);
		int vsize = 0;
		for (int i = 0; i < list_cs_vsize; i ++) {
			const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
			// const tcs_feature_bh_v7* cs_feature_bh = (tcs_feature_bh_v7*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
			std::string idnumber = utils::truncate_to_max_bytes(cs_feature->idnumber, nposm, MAX_CS_IDNUMBER_SIZE);
			if (signouted_ids.count(idnumber) != 0) {
				memcpy(delete_data + vsize, &(cs_feature->faceid), item_size);
				vsize ++;
			}
		}
		if (vsize > 0) {
			sync_facestore(max_ts, (const char*)delete_data, vsize * item_size, 0, false);
			VALIDATE(facestore_dirty, str_cast(vsize));
			write_facestore_dat();
		}
		free(delete_data);
	}

}

const tcs_feature* is_idnumber_existed(int idtype, const std::string& idnumber)
{
	VALIDATE(!idnumber.empty(), null_str);

	for (int i = 0; i < list_cs_vsize; i ++) {
		const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
		if (cs_feature->idtype != idtype) {
			continue;
		}
		// const tcs_feature_bh_v7* cs_feature_bh = (tcs_feature_bh_v7*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
		int cmp_size = idnumber.size();
		if (cmp_size > MAX_CS_IDNUMBER_SIZE) {
			cmp_size = MAX_CS_IDNUMBER_SIZE;
		}
		if (cmp_size == MAX_CS_IDNUMBER_SIZE || cs_feature->idnumber[cmp_size] == '\0') {
			if (memcmp(idnumber.c_str(), cs_feature->idnumber, cmp_size) == 0) {
				// length in cs_feature is cmp_size && cmp_size are same.
				return cs_feature;
			}
		}
	}
	return nullptr;
}


void facestore_insert_or_modify_v9(const std::vector<tmodify_task>& tasks, std::vector<int>* faceids)
{
	VALIDATE_IN_MAIN_THREAD();
	// VALIDATE(current_user.valid(), null_str);
	VALIDATE(slot_personversion() == personversion_v9, null_str);

	VALIDATE(!tasks.empty(), null_str);
	if (faceids != nullptr) {
		faceids->clear();
	}

	threading::lock lock(*facestore_mutex.get());

	int camera_at = 0;
	// form tcs_feature block
	faceprint::tcs_feature* cs_features = (tcs_feature*)malloc(tasks.size() * sizeof_tcs_feature);
	memset(cs_features, 0, tasks.size() * sizeof_tcs_feature);

	const int start_faceid = max_faceid_in_cs_features() + 1;
	int vsize = 0;
	int insert_size = 0;
	int64_t server_facestore_ts = facestore_ts;
	for (std::vector<tmodify_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const tmodify_task& task = *it;
		
		if (task.flags & tmodify_task::FLAG_FEATURE_DATA) {
			// gauth_do_feature_tasks should make usre no feature_size == 0 task in tasks.
			VALIDATE(task.feature_size == FEATURE_BLOB_SIZE, null_str);
		}

		faceprint::tcs_feature* cs_feature = cs_feature_ptr_from_list2(cs_features, vsize);
		vsize ++;

		const tcs_feature* existed = nullptr;
		if (task.faceid == nposm) {
			if (task.idtype != nposm) {
				existed = is_idnumber_existed(task.idtype, task.idnumber);
			}
		} else {
			existed = is_faceid_existed(task.faceid);
			VALIDATE(existed != nullptr, null_str);
		}

		if (existed != nullptr) {
			// scend: update
			memcpy(cs_feature, existed, sizeof_tcs_feature);

			// I hope task.updatetime > existed->ts, but agbox don't it.
			// VALIDATE(task.updatetime > existed->ts, null_str);
		} else {
			// scend: insert
			cs_feature->faceid = start_faceid + insert_size;
			insert_size ++;
		}

		if (faceids != nullptr) {
			faceids->push_back(cs_feature->faceid);
		}

		int coping_size = 0;
		if (existed == nullptr) {
			int new_idtype = task.idtype;
			std::string new_idnumber = task.idnumber;
			if (task.idtype == nposm) {
				new_idtype = gat517_faceid;
				new_idnumber = str_cast(cs_feature->faceid);
			}
			// idtype
			cs_feature->idtype = new_idtype;
			// idnumber
			coping_size = new_idnumber.size();
			if (coping_size > MAX_CS_IDNUMBER_SIZE) {
				coping_size = MAX_CS_IDNUMBER_SIZE;

			} /*else if (coping_size < MAX_CS_IDNUMBER_SIZE) {
				cs_feature->idnumber[coping_size] = '\0';
			} */
			memcpy(cs_feature->idnumber, new_idnumber.c_str(), coping_size);
		}

		if (task.flags & tmodify_task::FLAG_TS) {
			cs_feature->ts = task.ts;
			if (cs_feature->ts > server_facestore_ts) {
				server_facestore_ts = cs_feature->ts;
			}
		}
		if (task.flags & tmodify_task::FLAG_PERSONTYPE) {
			cs_feature->persontype = task.persontype;
		}

		// name
		if (task.flags & tmodify_task::FLAG_NAME) {
			coping_size = task.name.size();
			if (coping_size > MAX_CS_NAME_SIZE) {
				coping_size = MAX_CS_NAME_SIZE;

			} else if (coping_size < MAX_CS_NAME_SIZE) {
				// if existed != nullptr, cs_feature->name is copied from existed's name, it maybe longer then task.name.
				cs_feature->name[coping_size] = '\0';
			}
			if (coping_size) {
				memcpy(cs_feature->name, task.name.c_str(), coping_size);
			}
		}

		if (task.flags & tmodify_task::FLAG_CARD) {
			coping_size = task.card.size();
			if (coping_size > MAX_CS_CARDNUMBER_VSIZE) {
				coping_size = MAX_CS_CARDNUMBER_VSIZE;

			} else if (coping_size < MAX_CS_CARDNUMBER_VSIZE) {
				// if existed != nullptr, cs_feature->card is copied from existed's card, it maybe longer then task.card.
				cs_feature->card[coping_size] = '\0';
			}
			if (coping_size) {
				memcpy(cs_feature->card, task.card.c_str(), coping_size);
			}
		}
		if (task.flags & tmodify_task::FLAG_FEATURE_DATA) {
			memcpy(cs_feature->feature, task.feature_data, FEATURE_BLOB_SIZE);
		}

		tcs_feature_bh_v9* cs_feature_bh = (tcs_feature_bh_v9*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
		cs_feature_bh->camera_mask = 1 << camera_at;
	}

	if (vsize > 0) {
		if (!facestore_initialized) {
			VALIDATE(false, null_str);
			clear_list_feature();

			// personversion of local facestore maybe different from new version.
			recalculate_sizeof_tcs_feature();

			// int64_t server_facestore_ts = results["facestore"].asInt64();
			load_facestore(server_facestore_ts, (const char*)cs_features, vsize * faceprint::sizeof_tcs_feature, false);
			facestore_initialized = true;
			facestore_dirty = true;
		} else {
			sync_facestore(server_facestore_ts, (const char*)cs_features, vsize * faceprint::sizeof_tcs_feature, vsize, false);
		}
		if (facestore_dirty) {
			write_facestore_dat();
		}
	}
	free(cs_features);

	if (faceids != nullptr) {
		VALIDATE(tasks.size() == faceids->size(), null_str);
	}
}

void facestore_erase(const std::set<terase_task>& erases, int64_t max_ts)
{
	// I hope max_ts > facestore_ts, but agbox don't it.
	// VALIDATE(max_ts > facestore_ts, null_str);
	if (max_ts < facestore_ts) {
		max_ts = facestore_ts;
	}

	// section#1, delete feature in facestore that in this sync-getPersonList's signouted list.
	if (list_cs_vsize != 0) {
		const int item_size = 4;
		int* delete_data = (int*)malloc(list_cs_vsize * item_size);
		memset(delete_data, 0, list_cs_vsize * item_size);
		int vsize = 0;
		for (std::set<terase_task>::const_iterator it = erases.begin(); it != erases.end(); ++ it) {
			const terase_task& task = *it;
			if (task.faceid != nposm) {
				memcpy(delete_data + vsize, &(task.faceid), item_size);
				vsize ++;
				continue;
			}
			for (int i = 0; i < list_cs_vsize; i ++) {
				const tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
				// const tcs_feature_bh_v7* cs_feature_bh = (tcs_feature_bh_v7*)((uint8_t*)cs_feature + sizeof(tcs_feature_v0) + FEATURE_BLOB_SIZE - DEFAULT_FEATURE_BLOB_SIZE);
				std::string idnumber = utils::truncate_to_max_bytes(cs_feature->idnumber, nposm, MAX_CS_IDNUMBER_SIZE);
				if (cs_feature->idtype == task.idtype && idnumber == task.idnumber) {
					memcpy(delete_data + vsize, &(cs_feature->faceid), item_size);
					vsize ++;
				}
			}
		}
		if (vsize > 0) {
			sync_facestore(max_ts, (const char*)delete_data, vsize * item_size, 0, false);
			VALIDATE(facestore_dirty, str_cast(vsize));
			write_facestore_dat();
		}
		free(delete_data);
	}

}

}

#endif