/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "aplt_leagor_khome-lib"

#include "kface.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>
#include "rose_sdl_utils.hpp"
#include "rose_font.hpp"
#include "rose_lua.hpp"

#include "common.hpp"

// #include <opencv2/imgproc.hpp>

#ifdef USE_DFACE

using namespace std::placeholders;

#define DEFAULT_AREATHRESHOLD		15

namespace faceprint {

int slot_positions()
{
	return 1;
}

int slot_personversion()
{
	return personversion_v9;
}

int slot_inputcardtype()
{
	return inputcardtype_wg34base10;
}

}
#endif

namespace aplt {

#ifdef USE_DFACE
void faceprint_allocate(tapplet& aplt)
{
	faceprint::res_dir = aplt.res_path;
	faceprint::preferences_dir = aplt.preferences_dir;

	faceprint::valid_feature_blob_sizes.insert(1024);
	faceprint::valid_feature_blob_sizes.insert(512);
	VALIDATE(faceprint::valid_feature_blob_sizes.count(DEFAULT_FEATURE_BLOB_SIZE) != 0, null_str);

	const std::string st_model_str = "512,fake.model,df-feature512.dat";
	faceprint::st_models = faceprint::get_st_models(st_model_str);

	const std::string src = faceprint::res_dir + "/tflites";
	faceprint::copy_model_2_preferences(src);

	VALIDATE(faceprint::FEATURE_BLOB_SIZE == nposm, null_str);
	faceprint::FEATURE_BLOB_SIZE = DF_FEATURE_SIZE;

	// now calculate sizeof_tcs_feature base on current_user.personversion, FEATURE_BLOB_SIZE
	faceprint::recalculate_sizeof_tcs_feature();

	faceprint::MAXIMUM_FACES = 1000; // 10000
	faceprint::allocate_list_feature(faceprint::MAXIMUM_FACES);

	faceprint::load_facestore_dat();
	faceprint::facestore_initialized = true;
}

void faceprint_free()
{
	faceprint::free_list_feature();
}

//
// face task
//
#define CAMERA_AT		0
tkface::tkface(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
	: tcamera_task_slot(cfg_task, task_vars)
	, fpsdk_(nullptr)
	, ops_({{"insert", op_insert}, {"match", op_match}, {"checking", op_checking}})
	, var_name_faceid_(utils::join_app_prefix_id(aplt_.bundleid, "faceid"))
	, var_name_face_name_(utils::join_app_prefix_id(aplt_.bundleid, "face_name"))
	, var_name_face_ts_(utils::join_app_prefix_id(aplt_.bundleid, "ts"))
	, camera_at_(CAMERA_AT)
	, areathreshold_(DEFAULT_AREATHRESHOLD)
	, comparethreshold_(80)
	, liveness_(false)
	, hack_threshold_(0.98f)
{
	clear_kface();

	output_var_keys_.push_back(var_name_faceid_);
	output_var_keys_.push_back(var_name_face_name_);
	output_var_keys_.push_back(var_name_face_ts_);

	faceprint::face_sdk = faceprint::st_sdk2;

	{
		SDL_Log("{tface}don't call tface_sdk::authorize, s: %i", (int)sizeof(faceprint::tfacestore_header));
		// faceprint::tface_sdk::authorize(true, "VLQ73GUY6I2BHNFM");
	}

	fpsdk_.reset(faceprint::tface_sdk::create());
	fpsdk_->enable_same_thread(false);
}

tkface::~tkface()
{
	fpsdk_.reset(nullptr);
	// faceprint_free();
	// faceprint::free_list_feature();
}

void tkface::calculate_face_item(tface_item& item)
{
	VALIDATE(!item.filename.empty(), null_str);

	SDL_Log("{start_task}calculate_face_item, image: %s", item.filename.c_str());

	const std::string src_image = aplt_.preferences_dir + "/" + item.filename;
	surface surf = image::rose_get_image(src_image);
	VALIDATE(surf.get() != nullptr, null_str);

	faceprint::tface_sdk& face_sdk = *fpsdk_.get();
	faceprint::tface_wraper face(nullptr);
	faceprint::tfeature_wraper feature(nullptr);

	tsurface_2_mat_lock lock(surf);
	
	face.reset(face_sdk.track_get_face(lock.mat, empty_rect));
	VALIDATE(face->valid(), null_str);

	const faceprint::face_face_t* best_face = nullptr;
	best_face = &face->best_face();
	item.score = best_face->score;
	item.quality = best_face->quality;
	feature.reset(face_sdk.verify_get_feature(lock.mat, face->best_face()));

	VALIDATE(feature->size == sizeof(item.feature), null_str);
	memcpy(item.feature, feature->str, feature->size);

	float livenessScore = face_sdk.singleliveness_score(lock.mat, *best_face);
	item.livenessScore = livenessScore;

	// tfile file(src_image + ".dat", GENERIC_WRITE, CREATE_ALWAYS);
	// VALIDATE(file.valid(), null_str);
	// posix_fwrite(file.fp, outFeature_ptr, outFeature.size());
}

void tkface::test_image_compare()
{
	SDL_Log("{start_task}1.6, pre new FaceFeatureComparator");
	FaceFeatureComparator* faceFeatureComparator = new FaceFeatureComparator;

	std::vector<tface_item> items;
	items.push_back(tface_item("owner-sc-1.jpg"));
	calculate_face_item(items.back());

	items.push_back(tface_item("owner-sc-2.jpg"));
	calculate_face_item(items.back());

	items.push_back(tface_item("owner-wlk-1.jpg"));
	calculate_face_item(items.back());

	items.push_back(tface_item("owner-wlk-2.jpg"));
	calculate_face_item(items.back());

	for (int at = 1; at < (int)items.size(); at ++) {
		float cmp_score = faceFeatureComparator->compareFeature(items[0].feature, items[at].feature, 512);
		SDL_Log("{start_task}%s(%.5f/%.5f, live: %.5f) cmp %s(%.5f/%.5f, live: %.5f), cmp_socre: %.5f", 
			items[0].filename.c_str(), items[0].score, items[0].quality, items[0].livenessScore, 
			items[at].filename.c_str(), items[at].score, items[at].quality, items[at].livenessScore, cmp_score);
	}

	for (int at = 0; at < (int)items.size() - 1; at ++) {
		float cmp_score = faceFeatureComparator->compareFeature(items[3].feature, items[at].feature, 512);
		SDL_Log("{start_task}%s(%.5f/%.5f, live: %.5f) cmp %s(%.5f/%.5f, live: %.5f), cmp_socre: %.5f", 
			items[3].filename.c_str(), items[3].score, items[3].quality, items[3].livenessScore, 
			items[at].filename.c_str(), items[at].score, items[at].quality, items[at].livenessScore, cmp_score);
	}

	delete faceFeatureComparator;
}

std::string tkface::start_task()
{
	VALIDATE(op_ == nposm, null_str);

	utils::string_map symbols;
	std::string err_msg;

	std::string var_name_op = utils::join_app_prefix_id(aplt_.bundleid, "op");
	if (task_vars_.existed(var_name_op)) {
		const ttask_var& var_op = task_vars_.get_var(var_name_op);
		if (var_op.val.type() != var_type_nposm) {
			std::string op_str = var_op.val.str();
			if (ops_.count(op_str) != 0) {
				op_ = ops_.find(op_str)->second;
			} else {
				symbols["op"] = op_str;
				ros_.aplt_add_msg_log(time(nullptr), vgettext2("Unknown op value: $op. The behavior will be to only search for faces.", symbols), 0, false);
			}
		}
	}


	// test_image_compare();

	return err_msg;
}

void tkface::task_finished(const tapplet::ttask& cfg_task)
{
	clear_kface();
}

static std::string get_face_filename(const std::string& aplt_preferences_dir, int faceid)
{
	VALIDATE(faceid != nposm, null_str);

	char filename[256];
	SDL_snprintf(filename, sizeof(filename), "%s/saves/facestore/fid_%i.png", aplt_preferences_dir.c_str(), faceid);
	return filename;
}

bool tkface::slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	threading::lock lock(variable_mutex_);
	if (current_face_rect_ != empty_rect) {
		classifier_rects.push_back(std::make_pair(score_, current_face_rect_));

		char score_c_str[72];
		SDL_snprintf(score_c_str, sizeof(score_c_str), 
			"box.score: %.1f%%\narea*1000: %i\nmatched_score: %i\nfaceid_: %i", 
			quality_ * 100, thousands_, matched_score_, matched_faceid_);
		result_str = score_c_str;
	}

	bool require_stop = false;
	if (op_ == op_insert) {
		if (camera_feature_.get() != nullptr) {
			std::vector<faceprint::tmodify_task> tasks;
			uint32_t flags = 0xffffffff;
			int persontype = 0;
			std::string name = _("Untitle");
			std::string card;
			tasks.push_back(faceprint::tmodify_task(camera_at_, nposm, null_str, flags, time(nullptr), persontype,
				name, card, camera_feature_.get(), faceprint::FEATURE_BLOB_SIZE));
			std::vector<int> faceids;
			faceprint::facestore_insert_or_modify_v9(tasks, &faceids);

			const int faceid = faceids[0];
			task_vars_.insert_integer(var_name_faceid_, false, faceid);

			const std::string& filename = get_face_filename(aplt_.preferences_dir, faceid);
			imwrite(captured_surf_, filename);

			require_stop = true;
		}

	} else if (op_ == op_match || op_ == op_checking) {
		if (matched_faceid_ != nposm) {
			const faceprint::tcs_person person = faceprint::faceid_2_cs_person(matched_faceid_);
			// pinyin_.speak(person.name);

			task_vars_.insert_integer(var_name_faceid_, false, matched_faceid_);
			task_vars_.insert_string(var_name_face_name_, false, person.name);

			if (op_ == op_checking) {
				int64_t now_time = time(nullptr);
				const int64_t now_local_time = now_time + game_config::equation_of_time;
				const int64_t facestore_local_time = person.ts + game_config::equation_of_time;

				if (now_local_time / ONE_DAY_SECONDS != facestore_local_time / ONE_DAY_SECONDS) {
					uint32_t flags = faceprint::tmodify_task::FLAG_TS;
					std::vector<faceprint::tmodify_task> tasks;
					tasks.push_back(faceprint::tmodify_task(CAMERA_AT, matched_faceid_, flags, now_time));
					faceprint::facestore_insert_or_modify_v9(tasks, nullptr);

					task_vars_.insert_integer(var_name_face_ts_, false, now_time);
				}
			}

			require_stop = true;
		}
	}
	return require_stop;
}

void tkface::clear_kface()
{
	op_ = nposm;

	ffreason_ = nposm;
	current_face_rect_ = empty_rect;
	score_ = float_nposm;
	quality_ = float_nposm;
	thousands_ = nposm;

	matched_score_ = nposm;
	recognition_time_ = nposm;
	matched_faceid_ = nposm;
	hack_score_ = nposm;

	reset_camera_feature();
}

void tkface::set_camera_feature(const cv::Mat& argb, const char* feature_str, uint32_t feature_size, const SDL_Rect& facerect)
{
	VALIDATE_NOT_MAIN_THREAD();
	VALIDATE(captured_surf_.empty(), null_str);

	if (SDL_RectEmpty(&facerect)) {
		// captured_surf_ = clone_surface(argb);
		captured_surf_ = argb.clone();
	} else {
		// captured_surf_ = cut_surface(argb, facerect);
		cv::Rect roi(facerect.x, facerect.y, facerect.w, facerect.h);
		captured_surf_ = argb(roi).clone();
	}

	VALIDATE(camera_feature_.get() == nullptr, null_str);
	camera_feature_.reset(new char[faceprint::FEATURE_BLOB_SIZE]);
	memcpy(camera_feature_.get(), feature_str, feature_size);
/*
	memcpy(blended_tex_pixels_, surf->pixels, surf->w * surf->h * 4); 
	cv_tex_pixel_dirty_ = true;
*/
}

void tkface::reset_camera_feature()
{
	camera_feature_.reset();
	captured_surf_ = cv::Mat();
	// thumbnail_surf_ = nullptr;
	// thumbnail_rect_ = empty_rect;
	// stop_rect_ = empty_rect;
	// commit_person_.fakefeature = false;
	// commit_person_.dummyimage = false;
}

SDL_Rect tune_stsdk_facerect(const SDL_Rect& facerect)
{
	SDL_Rect ret = facerect;
	if (ret.x < 0) {
		ret.w += ret.x;
		ret.x = 0;
	}
	if (ret.y < 0) {
		ret.h += ret.y;
		ret.y = 0;
	}
	return ret;
}

SDL_Rect calculate_portrait_clip_rect(int surf_w, int surf_h, const SDL_Rect& facerect, double width_ratio_height, double inc_ratio)
{
	if (surf_w == nposm || facerect.x < 0 || facerect.y < 0 || facerect.w <= 0 || facerect.h <= 0) {
		return empty_rect;
	}
	VALIDATE(surf_w > 0 && surf_h > 0, null_str);

	const int h_inc = facerect.w * inc_ratio;
	const int v_inc = facerect.h * inc_ratio;
	SDL_Rect clip = enlarge_rect(facerect, h_inc, h_inc, v_inc, v_inc, surf_w, surf_h);
	VALIDATE(clip.w > 0 && clip.h > 0, null_str);
	const double ratio = 1.0 * clip.w / clip.h;

	if (ratio > width_ratio_height) {
		// height is min value, use height as reference. shink width
		const int width_extra = clip.w - clip.h * width_ratio_height;
		VALIDATE(width_extra >= 0, null_str);
		clip.x += width_extra / 2;
		clip.w -= width_extra;
		if (clip.x > facerect.x) {
			// at leat display valid part(from top-x).
			clip.x = facerect.x;
		}
		
	} else if (ratio < width_ratio_height) {
		// width is min value, use width as reference. shink height
		const int height_extra = clip.h - 1.0 * clip.w / width_ratio_height;
		VALIDATE(height_extra >= 0, null_str);
		clip.y += height_extra / 2;
		clip.h -= height_extra;
		if (clip.y > facerect.y) {
			// at leat display valid part(from left-y).
			clip.y = facerect.y;
		}
	}
	return clip;
}

void tkface::camera_work_frame(const surface& surf, const cv::Mat& argb)
{
	float face_threshold = 0.35f;

	faceprint::tface_wraper face(nullptr);
	faceprint::tfeature_wraper feature(nullptr);

	const int facestore_at = camera_at_;

	const int frame_area = argb.cols * argb.rows;
	int top_idx;
	float top_score = 0.0f;
	float hack_score = 1.0f;

	int ffreason = nposm;
	SDL_Rect valid_rect = empty_rect;
	SDL_Rect face_rect = empty_rect;
	float score = float_nposm;
	float quality = float_nposm;
	int thousands = nposm;

	VALIDATE(fpsdk_.get() != nullptr, null_str);
	if (fpsdk_.get() == nullptr) {
		fpsdk_.reset(faceprint::tface_sdk::create());
	}

	const uint32_t start_recognition_ticks = SDL_GetTicks();

	faceprint::tface_sdk& face_sdk = *fpsdk_.get();
	face.reset(face_sdk.track_get_face(argb, valid_rect));

	bool valid = face->valid() && face->best_face().score >= face_threshold;
	if (valid) {
		const faceprint::face_face_t& best_face = face->best_face();
		face_rect = ::create_rect(best_face.rect.left, best_face.rect.top, best_face.rect.right - best_face.rect.left, best_face.rect.bottom - best_face.rect.top);
		face_rect = tune_stsdk_facerect(face_rect);
		score = best_face.score;
		quality = best_face.quality;

		const int64_t this_area = face_rect.w * face_rect.h;
		thousands = (int)(this_area * 1000 / frame_area);
		// ocr_.current_thousands_ = thousands;
		if (thousands < areathreshold_) {
			// tracks.push_back(tst_track{trackcode_area, SDL_GetTicks(), matching_at, (uint32_t)thousands, in_b_frame});
			ffreason = faceprint::ffreason_area;
			valid = false;
		}
		if (valid) {
/*
			if (posix_abs(best_face.pitch) > game_config::camera_angle_threshold || posix_abs(best_face.yaw) > game_config::camera_angle_threshold || posix_abs(best_face.roll) > game_config::camera_angle_threshold) {
				tracks.push_back(tst_track{trackcode_angle, SDL_GetTicks(), 0, in_b_frame});
				ffreason = ffreason_angle;
				valid = false;
			}
*/
		}
	}

	if (valid) {
		const faceprint::face_face_t& best_face = face->best_face();

		threading::lock lock(variable_mutex_);
		ffreason_ = ffreason;
		current_face_rect_ = face_rect;
		score_ = score;
		quality_ = quality;
		thousands_ = thousands;

		feature.reset(face_sdk.verify_get_feature(argb, best_face));

		if (op_ == op_insert) {
			if (camera_feature_.get() == nullptr) {
				const double width_ratio_height = 3.0 / 4; // 3.0 / 4
				SDL_Rect facerect = calculate_portrait_clip_rect(argb.cols, argb.rows, face_rect, width_ratio_height, 2.0 / 3);

				// variable_mutex_ do sync, so not use Invoke.
				set_camera_feature(argb, feature->str, feature->size, facerect);
			}

		} else if (op_ == op_match || op_ == op_checking) {
			if (matched_faceid_ == nposm) {
				face_sdk.set_base_feature(feature.get());
				face_sdk.compare_features2(facestore_at, top_idx, top_score);

				matched_score_ = (int)(top_score * 100);
				if ((int)(top_score * 100) >= comparethreshold_) {
					if (liveness_) {
						hack_score = face_sdk.singleliveness_score(argb, best_face);
						valid = hack_score < hack_threshold_;
						if (!valid) {
							// tracks.push_back(tst_track{trackcode_liveness, SDL_GetTicks(), matching_at, (uint32_t)(hack_score * 100), in_b_frame});
							ffreason = faceprint::ffreason_singleliveness;
						}
					}

				} else {
					// const uint32_t similarity_fail_ticks = SDL_GetTicks();
					// tracks.push_back(tst_track{trackcode_similarity, similarity_fail_ticks, matching_at, (uint32_t)ocr_.matched_score_, in_b_frame});
					// ocr_.b_frame_.similarity_fails ++;
					// ocr_.b_frame_.similarity_fails_ms += similarity_fail_ticks - start_recognition_ticks;
					ffreason = faceprint::ffreason_similarity;
					valid = false;
				}
				// if (!valid) {
				//	request_sync = true;
				// }
				// request_beep = true;

				recognition_time_ = SDL_GetTicks() - start_recognition_ticks;

				{
					threading::lock lock(*faceprint::facestore_mutex.get());
					// during compare_features2 to this, facestore maybe changed.
					valid = top_idx >= 0 && top_idx < faceprint::list_cs_vsize;
					if (valid) {
						if (faceprint::use_facestore) {
							faceprint::tlist_feature& list = faceprint::list_features[facestore_at];
							const faceprint::tcs_feature* cs_feature = container_of(list.features[top_idx], faceprint::tcs_feature, feature);
							// ocr_.matched_camera_at_ = ocr_.matching_camera_at_;
							matched_faceid_ = cs_feature->faceid;
							if (game_config::os == os_windows) {
								// ocr_.matched_faceid_ = 365;
								int ii = 0;
							}

						} else {
							VALIDATE(game_config::os == os_windows, null_str);
							// ocr_.matched_camera_at_ = 0;
							matched_faceid_ = 1;
						}
						hack_score_ = hack_score * 100;

						// extra variable for debug.
						// ocr_.matched_faces_ = face->valid_count;
						// ocr_.matched_face_score_ = (int)(best_face.score * 100);
						// ocr_.matched_rect_ = ::create_rect(best_face.rect.left, best_face.rect.top, best_face.rect.right - best_face.rect.left, best_face.rect.bottom - best_face.rect.top);
					}
				}
			}
		}
	}
}

int impl_vcamera2_kface_reload(lua_State* L)
{
	tcamera2* v = *static_cast<tcamera2 **>(lua_touserdata(L, 1));

	// int max_disp_size = luaL_checkinteger(L, 2);

	// VALIDATE(max_disp_size <= 200, null_str);

	// lua_pushinteger(L, faceprint::list_cs_vsize);

	// positions
	int at = 0;
	lua_createtable(L, faceprint::list_cs_vsize, 0);

	{
		tstack_size_lock lock(L, 0);
		faceprint::tcs_person cs_person;
		for (int i = 0; i < faceprint::list_cs_vsize; i ++, at ++) {
			const faceprint::tcs_feature* cs_feature = cs_feature_ptr_from_list(i);
			faceprint::tcs_feature_2_cs_person(cs_feature, cs_person);

			const std::string& name = cs_person.name;
			VALIDATE(!name.empty(), null_str);

			lua_createtable(L, 3, 0);
			lua_pushinteger(L, cs_person.faceid);
			lua_rawseti(L, -2, 1); // <== 0: faceid

			lua_pushstring(L, name.c_str());
			lua_rawseti(L, -2, 2); // <== 1: name

			lua_pushstring(L, utils::format_time_date(cs_person.ts).c_str());
			lua_rawseti(L, -2, 3); // <== 2: timestamp

			lua_rawseti(L, -2, at + 1);
		}
	}

	return 1;
}

int impl_vcamera2_erase_face(lua_State* L)
{
	tcamera2* v = *static_cast<tcamera2 **>(lua_touserdata(L, 1));

	int faceid = luaL_checkinteger(L, 2);
	int idtype = luaL_checkinteger(L, 3);
	const std::string idnumber = luaL_checkstring(L, 4);

	if (faceid != nposm) {
		if (idtype != nposm) {
			return luaL_argerror(L, 2, "if faceid != nposm, idtype must be nposm");
		}
		if (!idnumber.empty()) {
			return luaL_argerror(L, 2, "if faceid != nposm, idnumber type must be empty");
		}
	} else {
		if (idtype == nposm) {
			return luaL_argerror(L, 2, "if faceid == nposm, idtype must not be nposm");
		}
		if (idnumber.empty()) {
			return luaL_argerror(L, 2, "if faceid == nposm, idnumber must not be empty");
		}
	}
	std::set<faceprint::terase_task> tasks;
	tasks.insert(faceprint::terase_task(faceid, idtype, idnumber));

	faceprint::facestore_erase(tasks, faceprint::facestore_ts);

	const std::string& filename = get_face_filename(curr_aplt->preferences_dir, faceid);
	SDL_DeleteFiles(filename.c_str());
	return 0;
}

int impl_vcamera2_modify_face(lua_State* L)
{
	tcamera2* v = *static_cast<tcamera2 **>(lua_touserdata(L, 1));

	int faceid = luaL_checkinteger(L, 2);
	if (faceprint::is_faceid_existed(faceid) == nullptr) {
		return luaL_argerror(L, 2, "faceid isn't existed");
	}
	const std::string field_name = luaL_checkstring(L, 3);
	int flags = 0;
	std::string str_val;
	int int64_val;

	if (field_name == "name") {
		flags = faceprint::tmodify_task::FLAG_NAME;
		str_val = luaL_checkstring(L, 4);

	} else if (field_name == "ts") {
		flags = faceprint::tmodify_task::FLAG_TS;
		int64_val = luaL_checkinteger(L, 4);

	} else {
		return luaL_argerror(L, 3, "unsupport modify field");
	}

	std::vector<faceprint::tmodify_task> tasks;
	if (flags == faceprint::tmodify_task::FLAG_NAME) {
		tasks.push_back(faceprint::tmodify_task(CAMERA_AT, faceid, flags, str_val));
	} else {
		tasks.push_back(faceprint::tmodify_task(CAMERA_AT, faceid, flags, int64_val));
	}

	faceprint::facestore_insert_or_modify_v9(tasks, nullptr);

	return 0;
}

#else

int impl_vcamera2_kface_reload(lua_State* L)
{
	tlua_camera* v = *static_cast<tlua_camera **>(lua_touserdata(L, 1));

	lua_createtable(L, 0, 0);
	return 1;
}

int impl_vcamera2_erase_face(lua_State* L)
{
	tlua_camera* v = *static_cast<tlua_camera **>(lua_touserdata(L, 1));
	return luaL_argerror(L, 2, "dface don't use");
}

int impl_vcamera2_modify_face(lua_State* L)
{
	tlua_camera* v = *static_cast<tlua_camera **>(lua_touserdata(L, 1));
	return luaL_argerror(L, 2, "dface don't use");
}


#endif

}