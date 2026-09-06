#include "rose_mediapipe_api.hpp"
#include <SDL.h>
#include "rose_exception.hpp"
#include "rose_string_utils.hpp"
#include "rose_filesystem_dll.hpp"
#include <iomanip>
#include <opencv2/imgproc.hpp>

using namespace std::placeholders;

namespace mediapipe {

fncreate_pose_tracking_api rose_create_pose_tracking_api = nullptr;

// It is possible to concentrate 'next_image' into a single thread, 
// but it is difficult to ensure that destruction also happens in that same thread. 
// For example, for posture detection, 'next_image' runs in the camera thread, but it cannot be destroyed in the camera thread. 
// Here, we only check that an instance can be destroyed only once.

static bool check_thread = false;
static std::map<SDL_threadID, const tpose_tracking_api*> thread_pose_tracking_apis;
static std::set<const tpose_tracking_api*> pose_tracking_apis;

tpose_tracking_api::tpose_tracking_api()
	: graph_initialized_(false)
	, min_next_interval_no_cpu_saver_(0)
	, next_next_new_ticks_(0)
{
    if (check_thread) {
	    SDL_threadID tid = SDL_ThreadID();
	    VALIDATE(thread_pose_tracking_apis.count(tid) == 0, "At any given time, only one thttp_api can be running");
	    thread_pose_tracking_apis.insert(std::make_pair(tid, this));

    } else {
        VALIDATE(pose_tracking_apis.count(this) == 0, null_str);
        pose_tracking_apis.insert(this);
    }

}

tpose_tracking_api::~tpose_tracking_api()
{
    if (check_thread) {
	    SDL_threadID tid = SDL_ThreadID();
	    VALIDATE(thread_pose_tracking_apis.count(tid) != 0, "The thttp_api of this thread has been destroyed");

	    std::map<SDL_threadID, const tpose_tracking_api*>::iterator it = thread_pose_tracking_apis.find(tid);
	    thread_pose_tracking_apis.erase(tid);

    } else {
        VALIDATE(pose_tracking_apis.count(this) != 0, "The tpose_tracking_api has been destroyed");

	    std::set<const tpose_tracking_api*>::iterator it = pose_tracking_apis.find(this);
	    pose_tracking_apis.erase(this);
    }
}

void rose_set_create_pose_tracking_api(fncreate_pose_tracking_api fcreate)
{
	rose_create_pose_tracking_api = fcreate;
}

enum {lkmcolor_nose, lkmcolor_left, lkmcolor_right, lkmcolor_count};
void overlay_pose_landmarks(const SDL_FPoint* norm_xy_landmarks, int count, bool flip_h, cv::Mat& output_mat, const SDL_Rect* clip)
{
	VALIDATE(count == mediapipe::kNumPoseLandmarks, null_str);
	const SDL_Point POSE_CONNECTIONS[] = {
		{0, 2}, {0, 5}, {2, 7}, {5, 8}, {9, 10},
		{11, 12}, {23, 24},
		{11, 13}, {13, 15}, {15, 17}, {15, 21}, {15, 19}, {17, 19},
		{12, 14}, {14, 16}, {16, 18}, {16, 22}, {16, 20}, {18, 20},
		{11, 23}, {23, 25}, {25, 27}, {27, 29}, {27, 31}, {29, 31},
		{12, 24}, {24, 26}, {26, 28}, {28, 30}, {28, 32}, {30, 32},
	};

	int width = output_mat.cols;
	int height = output_mat.rows;

    SDL_Point margin{0, 0};
    if (clip != nullptr) {
        VALIDATE(clip->x + clip->w <= width, null_str);
        VALIDATE(clip->y + clip->h <= height, null_str);
        margin.x = clip->x;
        margin.y = clip->y;
        width = clip->w;
        height = clip->h;
    }

	SDL_Point xy_landmarks[kNumPoseLandmarks];
	for (int at = 0; at < count; at ++) {
		float x = flip_h? 1.0 - norm_xy_landmarks[at].x: norm_xy_landmarks[at].x;
		xy_landmarks[at].x = static_cast<int>(x * width);
		xy_landmarks[at].y = static_cast<int>(norm_xy_landmarks[at].y * height);
	}

    int lines = sizeof(POSE_CONNECTIONS) / sizeof(POSE_CONNECTIONS[0]);
	const bool thin = true;
    for (int at = 0; at < lines; at ++) {
        const SDL_Point& connection = POSE_CONNECTIONS[at];

		if (std::isnan(norm_xy_landmarks[connection.x].x) || std::isnan(norm_xy_landmarks[connection.y].x)) {
			continue;
		}

        const SDL_Point& start = xy_landmarks[connection.x];
        const SDL_Point& end = xy_landmarks[connection.y];
        // if (start.x >= 0 && start.x < width && start.y >= 0 && start.y < height &&
		//	end.x >= 0 && end.x < width && end.y >= 0 && end.y < height) {
		if ((start.x >= 0 && start.x < width && start.y >= 0 && start.y < height) ||
			(end.x >= 0 && end.x < width && end.y >= 0 && end.y < height)) {
			cv::line(output_mat, cv::Point(margin.x + start.x, margin.y + start.y), cv::Point(margin.x + end.x, margin.y + end.y), 
                cv::Scalar(255, 255, 255, 255), thin? 3: 10); // white   
        }
    }

    SDL_Color lkm_def_color{0, 255, 0, 255};
    const SDL_Color colors[] = {
        SDL_Color{255, 0, 0, 255}, 
        SDL_Color{78, 175, 80, 255}, 
        SDL_Color{230, 179, 26, 255}
    };
	for (int at = 0; at < count; ++ at) {
		const int& x = xy_landmarks[at].x;
		const int& y = xy_landmarks[at].y;

        if (x >= 0 && x < width && y >= 0 && y < height) {
            const SDL_Color* color = &lkm_def_color;
            // if (colors != 0) {
                if (at == 0) {
                    color = colors + lkmcolor_nose;
                } else if (at < 7) {
                    color = colors + (at <= 3? lkmcolor_left: lkmcolor_right);

                } else if ((at & 1) == 1) {
                    color = colors + lkmcolor_left;
                } else {
                    color = colors + lkmcolor_right;
                }
            // }

            cv::circle(output_mat, cv::Point(margin.x + x, margin.y + y), thin? 5: 15, cv::Scalar(color->b, color->g, color->r, 255), -1);
        }
    }

}

/*
// 1692x2092
const SDL_Point def_imgcoors[33] = {
    {846, 300}, // 0
    {910, 201}, // 1
    {953, 201}, // 2
    {997, 201}, // 3
    {0, 0}, // 4
    {0, 0}, // 5
    {0, 0}, // 6
    {1060, 251},  // 7
    {0, 0},  // 8
    {907, 382},  // 9
    {0, 0},  // 10
    {1137, 586},  // 11
    {0, 0},  // 12
    {1324, 780},  // 13
    {0, 0},  // 14
    {1515, 690},  // 15
    {0, 0},  // 16
    {1669, 682}, // 17
    {0, 0}, // 18
    {1617, 554}, // 19
    {0, 0}, // 20
    {1515, 600}, // 21
    {0, 0}, // 22
    {1043, 1104}, // 23
    {0, 0}, // 24
    {1137, 1518}, // 25
    {0, 0}, // 26
    {1033, 1933}, // 27
    {0, 0}, // 28
    {973, 2041}, // 29
    {0, 0}, // 30
    {1185, 2065}, // 31
    {0, 0}, // 32
};

void generate_kDefaultPoseLandmarks_from_def_imgcoors()
{
    VALIDATE(sizeof(def_imgcoors) / sizeof(def_imgcoors[0]) == mediapipe::kNumPoseLandmarks, null_str);
    SDL_Size img_size{1692, 2092};
    mediapipe::imgcoor_to_landmarks(img_size, landmarks2, kDefaultPoseLandmarks, game_config::preferences_dir + "/1.cpp");
}
*/

void imgcoor_to_landmarks(const SDL_Size& img_size, const SDL_Point* imgcoors, SDL_FPoint* landmarks, const std::string& filename)
{
    char names[][24] = { "nose", "left_eye_inner", "left_eye", "left_eye_outer", 
        "right_eye_inner", "right_eye", "right_eye_outer", "left_ear", "right_ear",
        "mouth_left", "mouth_right", "left_shoulder", "right_shoulder", "left_elbow", "right_elbow",
        "left_wrist", "right_wrist", "left_pinky", "right_pinky", "left_index", "right_index", "left_thumb", "right_thumb",
        "left_hip", "right_hip", "left_knee", "right_knee",
        "left_ankle", "right_ankle", "left_heel", "right_heel", "left_foot_index", "right_foot_index", 
    };
    VALIDATE(sizeof(names) / sizeof(names[0]) == mediapipe::kNumPoseLandmarks, null_str);

    // To reduce measurement work, for image coordinates, 
    // you don't need to provide the left value; 
    // the left value will be automatically calculated from the right value.
    SDL_Point convert_map[16] = {{1, 4}, {2, 5}, {3, 6}};
    int count = 3;
    for (int at = 7; at < mediapipe::kNumPoseLandmarks; at += 2) {
        convert_map[count ++] = SDL_Point{at, at + 1};
    }
    VALIDATE(count == sizeof(convert_map) / sizeof(convert_map[0]), null_str);

    SDL_Point landmarks2[mediapipe::kNumPoseLandmarks];
    memcpy(landmarks2, imgcoors, sizeof(landmarks2));

    const int mid_x = img_size.w / 2;
    for (int at = 0; at < count; at ++) {
        const SDL_Point& m = convert_map[at];
        const SDL_Point& left = landmarks2[m.x];
        SDL_Point& right = landmarks2[m.y];
        int diff = left.x - mid_x;
        VALIDATE(diff > 0, null_str);
        right.x = mid_x - diff;

        right.y = left.y;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << img_size.w << " x " << img_size.h << "\n";
    ss << "pose_aspect = pose_width / pose_height = " << 1.0 * img_size.w / img_size.h << ";\n";
    ss << "SDL_Point landmarks2[33] = {";
    for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
        ss << "\n\t{" << landmarks2[at].x << ", " << landmarks2[at].y << "},\t// " << at << ": " << names[at];
    }
    ss << "\n};";
    ss << "\n\n";

    ss << "SDL_FPoint landmarks[33] = {";
    for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
        landmarks[at].x = 1.0f * landmarks2[at].x / img_size.w;
        landmarks[at].y = 1.0f * landmarks2[at].y / img_size.h;
        ss << "\n\t{" << landmarks[at].x << "f, " << landmarks[at].y << "f},\t// " << at << ": " << names[at];
    }
    ss << "\n};";
    if (!filename.empty()) {
        write_file(filename, ss.str().c_str(), ss.str().size());
    }
}

// how to get kDefaultPoseLandmarks?
//  --imgcoor_to_landmarks(SDL_Size{1692, 2092}, def_imgcoors, kDefaultPoseLandmarks, filename)
// 1692x2092
static const SDL_FPoint kDefaultPoseLandmarks[33] = {
	{0.5000f, 0.1434f},	// 0: nose
	{0.5378f, 0.0961f},	// 1: left_eye_inner
	{0.5632f, 0.0961f},	// 2: left_eye
	{0.5892f, 0.0961f},	// 3: left_eye_outer
	{0.4622f, 0.0961f},	// 4: right_eye_inner
	{0.4368f, 0.0961f},	// 5: right_eye
	{0.4108f, 0.0961f},	// 6: right_eye_outer
	{0.6265f, 0.1200f},	// 7: left_ear
	{0.3735f, 0.1200f},	// 8: right_ear
	{0.5361f, 0.1826f},	// 9: mouth_left
	{0.4639f, 0.1826f},	// 10: mouth_right
	{0.6720f, 0.2801f},	// 11: left_shoulder
	{0.3280f, 0.2801f},	// 12: right_shoulder
	{0.7825f, 0.3728f},	// 13: left_elbow
	{0.2175f, 0.3728f},	// 14: right_elbow
	{0.8954f, 0.3298f},	// 15: left_wrist
	{0.1046f, 0.3298f},	// 16: right_wrist
	{0.9864f, 0.3260f},	// 17: left_pinky
	{0.0136f, 0.3260f},	// 18: right_pinky
	{0.9557f, 0.2648f},	// 19: left_index
	{0.0443f, 0.2648f},	// 20: right_index
	{0.8954f, 0.2868f},	// 21: left_thumb
	{0.1046f, 0.2868f},	// 22: right_thumb
	{0.6164f, 0.5277f},	// 23: left_hip
	{0.3836f, 0.5277f},	// 24: right_hip
	{0.6720f, 0.7256f},	// 25: left_knee
	{0.3280f, 0.7256f},	// 26: right_knee
	{0.6105f, 0.9240f},	// 27: left_ankle
	{0.3895f, 0.9240f},	// 28: right_ankle
	{0.5751f, 0.9756f},	// 29: left_heel
	{0.4249f, 0.9756f},	// 30: right_heel
	{0.7004f, 0.9871f},	// 31: left_foot_index
	{0.2996f, 0.9871f},	// 32: right_foot_index
};

const SDL_FPoint* get_default_landmarks(SDL_Size& img_size)
{
    img_size = SDL_Size{1692, 2092};
    return kDefaultPoseLandmarks;
}

}

