#ifndef FACE_SDK_HPP_INCLUDED
#define FACE_SDK_HPP_INCLUDED


#include "rose_exception.hpp"
#include "faceprint_util.hpp"
// #include "wml_exception.hpp"
// #include <rtc_base/thread_checker.h>

#ifdef USE_DFACE

namespace faceprint {

extern int face_sdk;
extern std::map<int, std::string> face_sdks;

/// st rectangle definition
struct face_rect_t {
  int left;   ///< 矩形最左边的坐标
              ///< The value of left direction of the rectangle
  int top;    ///< 矩形最上边的坐标
              ///< The value of top direction of the rectangle
  int right;  ///< 矩形最右边的坐标
              ///< The value of right direction of the rectangle
  int bottom; ///< 矩形最下边的坐标
              ///< The value of bottom direction of the rectangle
};

#define EVALUDATE_FACE_RECT(to, from)   \
    (to).left = (from).left;    \
    (to).top = (from).top;    \
    (to).right = (from).right;    \
    (to).bottom = (from).bottom

#define EVALUDATE_FACE_RECT2(to, from)   \
    (to).left = (from).x;    \
    (to).top = (from).y;    \
    (to).right = (from).x + (from).width;    \
    (to).bottom = (from).y + (from).height

/// @brief struct for face info
struct face_face_t {
    face_face_t()
        : score(0)
        , quality(0)
        , face_ptr(nullptr)
        , quality_ptr(nullptr)
    {}

    bool valid() const { return score > 0 && face_ptr != nullptr && quality_ptr != nullptr; }
  
    face_rect_t rect; ///< 代表面部的矩形区域
                    ///< the rectangular area of the face
    float score;    ///<
                    ///置信度，用于筛除负例，与人脸照片质量无关，值越高表示置信度越高。
                    ///< The length of the face's key points array
    float yaw;      ///< 水平转角，真实度量的左负右正
                    ///< face's yaw angle
    float pitch;    ///< 俯仰角，真实度量的上负下正
                    ///< face's pitch angle
    float roll;     ///< 旋转角，真实度量的左负右正
                    ///< face's rotation angle
    float quality;  ///< 目标的综合质量
    // derived class reserved
    void* face_ptr;
    void* quality_ptr;
};

struct tface
{
public:
    tface()
        : valid_count(0)
        , count_ignore_valid_rect_(0)
		, best_score_ignore_valid_rect_(-1.0f)
	{}

	virtual bool valid() const = 0;
    const face_face_t& best_face() const
    {
        VALIDATE(best_face_.valid(), null_str);
        return best_face_;
    }

	// virtual bool valid_ignore_valid_rect(float min_score) const = 0;
    virtual void blend_write(surface& surf) const = 0;

protected:
    void calculate_best_at(face_face_t* faces, int count, const SDL_Rect& valid_rect)
    {
        VALIDATE(count_ignore_valid_rect_ == 0 && valid_count == 0, null_str);

        count_ignore_valid_rect_ = count;
        const bool require_verify = !SDL_RectEmpty(&valid_rect);
        int best_at = nposm;
		float best_score = -1.0f;
		int best_area = 0;

        for (int i = 0; i < count_ignore_valid_rect_; i ++) {
            face_face_t& face = faces[i];
            if (face.score > best_score_ignore_valid_rect_) {
				best_score_ignore_valid_rect_ = face.score;
			}
			if (require_verify) {
				if (!point_in_rect(face.rect.left, face.rect.top, valid_rect)) {
					continue;
				}
				if (!point_in_rect(face.rect.right, face.rect.top, valid_rect)) {
					continue;
				}
				if (!point_in_rect(face.rect.left, face.rect.bottom, valid_rect)) {
					continue;
				}
				if (!point_in_rect(face.rect.right, face.rect.bottom, valid_rect)) {
					continue;
				}
			}
			const int area = (face.rect.right - face.rect.left) * (face.rect.bottom - face.rect.top);
			if (area > best_area && (face.score >= 0.995 || face.score > best_score)) {
				best_at = i;
				best_score = face.score;
				best_area = area;
			}
			valid_count ++;
		}

        if (best_at != nposm) {
            best_face_ = faces[best_at];
        }
    }

    void blend_write_face(surface& surf, cv::Mat& mat, const face_face_t& face, int index, bool is_best) const;

public:
    int valid_count;

protected:
    face_face_t best_face_;

	int count_ignore_valid_rect_;
	float best_score_ignore_valid_rect_;
};

struct tfeature
{
	tfeature(const char* str, uint32_t size)
		: str(str)
		, size(size)
	{
        VALIDATE(str != nullptr && size > 0, null_str);
    }

	bool valid() const { return str != nullptr && size > 0; }

	const char* str;
	uint32_t size;
};

// bool authorize_st_sdk1(bool onlinestsdk, const char* data);
std::string authorize_extra_err_msg_st_sdk1();

bool authorize_st_sdk2(bool onlinestsdk, const char* data);
std::string authorize_extra_err_msg_st_sdk2();

class tface_sdk
{
public:
    static tface_sdk* create();
    static bool authorize(bool onlinestsdk, const char* data = nullptr)
    {
        bool ret = false;
        if (face_sdk == st_sdk1) {
            // ret = authorize_st_sdk1(onlinestsdk, data);

        } else if (face_sdk == st_sdk2) {
            ret = authorize_st_sdk2(onlinestsdk, data);

        } else {
            VALIDATE(false, null_str);
        }
        return ret;
    }
    static std::string authorize_extra_err_msg()
    {
        std::string msg;
        if (face_sdk == st_sdk1) {
            msg = authorize_extra_err_msg_st_sdk1();

        } else if (face_sdk == st_sdk2) {
            msg = authorize_extra_err_msg_st_sdk2();

        } else {
            VALIDATE(false, null_str);
        }
        return msg;
    }

    tface_sdk()
        : same_thread_(true)
        , verbose_(false)
        , tid_(SDL_ThreadID())
        , base_feature_(nullptr)
    {}

    void enable_same_thread(bool enable) 
    {
        if (enable == same_thread_) {
		    return;
	    }
	    same_thread_ = enable;
    }
    
	virtual tface* track_get_face(const cv::Mat& argb, const SDL_Rect& valid_rect) = 0;

	virtual tfeature* verify_get_feature(const cv::Mat& argb, const face_face_t& face) = 0;
	std::unique_ptr<char[]> verify_get_feature_str(const surface& surf, float min_score, int min_width, int min_height);

	void set_base_feature(const tfeature* feature) { base_feature_ = feature; }
	virtual float compare_feature(const tfeature& that) = 0;

	virtual bool compare_features2(int facestore_at, int& top_idx, float& top_score) const = 0;
	virtual float singleliveness_score(const cv::Mat& argb, const face_face_t& face) const = 0;

protected:
    bool same_thread_;
    bool verbose_;
    SDL_threadID tid_;
	// rtc::ThreadChecker thread_checker_;
    const tfeature* base_feature_;
};

// tface_sdk* create_st_sdk1();
void delete_st_sdk1(tface_sdk* sdk);
void delete_face1(tface* face);
void delete_feature1(tfeature* feature);

tface_sdk* create_st_sdk2();
void delete_st_sdk2(tface_sdk* sdk);
void delete_face2(tface* face);
void delete_feature2(tfeature* feature);

// why use tsdk_wraper/tface_wraper/tfeature_wraper?
// --I want use std::unique_ptr<tface_sdk>/std::unique_ptr<tface>/std::unique_ptr<tfeature>,
//   but std::unique_ptr require <class> must be final class. I require a <base_class> verion's 'std::unique_ptr'.
struct tsdk_wraper
{
public:
	explicit tsdk_wraper(tface_sdk* sdk)
		: sdk_(sdk)
	{}
    ~tsdk_wraper()
    {
        reset(nullptr);
    }

    // must not use: tsdk_wraper wraper2(wraper);
    tsdk_wraper(const tsdk_wraper& wraper) = delete;
    // must not use: wrapper = tsdk_wrapper(...);
    const tsdk_wraper& operator=(const tsdk_wraper& wraper) = delete;

    tface_sdk* get() const { return sdk_; }
    tface_sdk* operator->() const { return sdk_; }

	void reset(tface_sdk* sdk)
    {
        if (sdk != sdk_ && sdk_ != nullptr) {
            if (face_sdk == st_sdk1) {
                // delete_st_sdk1(sdk_);
            } else if (face_sdk == st_sdk2) {
                delete_st_sdk2(sdk_);
            } else {
                VALIDATE(false, null_str);
            }
        }
        sdk_ = sdk;
    }

private:
	tface_sdk* sdk_;
};

struct tface_wraper
{
public:
	explicit tface_wraper(tface* face)
		: face_(face)
	{}
    ~tface_wraper()
    {
        reset(nullptr);
    }

    tface_wraper(const tface_wraper& wraper) = delete;
    const tface_wraper& operator=(const tface_wraper& wraper) = delete;

    tface* get() const { return face_; }
    tface* operator->() const { return face_; }

	void reset(tface* face)
    {
        if (face != face_ && face_ != nullptr) {
            if (face_sdk == st_sdk1) {
               // delete_face1(face_);
            } else if (face_sdk == st_sdk2) {
                delete_face2(face_);
            } else {
                VALIDATE(false, null_str);
            }
        }
        face_ = face;
    }

private:
	tface* face_;
};

struct tfeature_wraper
{
public:
	explicit tfeature_wraper(tfeature* feature)
		: feature_(feature)
	{}
    ~tfeature_wraper()
    {
        reset(nullptr);
    }

    tfeature_wraper(const tfeature_wraper& wraper) = delete;
    const tfeature_wraper& operator=(const tfeature_wraper& wraper) = delete;

    tfeature* get() const { return feature_; }
    tfeature* operator->() const { return feature_; }

	void reset(tfeature* feature)
    {
        if (feature != feature_ && feature_ != nullptr) {
            if (face_sdk == st_sdk1) {
                // delete_feature1(feature_);
            } else if (face_sdk == st_sdk2) {
                delete_feature2(feature_);
            } else {
                VALIDATE(false, null_str);
            }
        }
        feature_ = feature;
    }

private:
	tfeature* feature_;
};

} // namespace namespace faceprint

#endif

#endif