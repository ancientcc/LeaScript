/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#ifndef LEAGOR_KFACE_HPP_INCLUDED
#define LEAGOR_KFACE_HPP_INCLUDED

#include "face_sdk.hpp"
#include "so_aplt_task_helper.hpp"

#ifdef USE_DFACE
#include "dface/api.h"

namespace aplt {

//
// kface
//
class tkface: public thelper_camera_task_slot
{
public:
	tkface(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars);
	~tkface();

private:
	std::string start_task() override;
	void task_finished(const tapplet::ttask& cfg_task) override;

	bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) override;

	void camera_work_frame(const surface& surf, const cv::Mat& argb) override;

	void clear_kface();
	void set_camera_feature(const cv::Mat& argb, const char* feature_str, uint32_t feature_size, const SDL_Rect& facerect);
	void reset_camera_feature();

	struct tface_item
	{
		tface_item(const std::string& filename)
			: filename(filename)
			, score(float_nposm)
			, quality(float_nposm)
		{
			VALIDATE(!filename.empty(), null_str);
		}

		const std::string filename;
		float score;
		float quality;
		float livenessScore;
		uint8_t feature[512];
	};
	void calculate_face_item(tface_item& item);
	void test_image_compare();

private:
	faceprint::tsdk_wraper fpsdk_;

	enum {op_insert, op_match, op_checking};
	int op_;
	std::map<std::string, int> ops_;

	const std::string var_name_faceid_;
	const std::string var_name_face_name_;
	const std::string var_name_face_ts_;

	const int camera_at_;
	const int areathreshold_;
	const int comparethreshold_;
	const bool liveness_;
	const float hack_threshold_;

	int ffreason_;
	SDL_Rect current_face_rect_;
	float score_;
	float quality_;
	int thousands_;

	int matched_score_;
	int recognition_time_;
	int matched_faceid_;
	int hack_score_;

	cv::Mat captured_surf_;
	std::unique_ptr<char[]> camera_feature_;
};

}

#endif

#endif

