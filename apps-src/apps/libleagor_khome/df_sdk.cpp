#define GETTEXT_DOMAIN "aplt_leagor_khome-lib"

#include "face_sdk.hpp"
#include "rose_filesystem.hpp"
#include "gettext.hpp"
#include "json/json.h"
#include "rose_sdl_utils.hpp"

#include <opencv2/imgproc.hpp>

#ifdef USE_DFACE

#include "dface/api.h"

#if defined(_WIN32) || defined(__APPLE__)
FaceDetector::FaceDetector(const std::string &modelPath, df_device_t device, int numThreads)
{
	VALIDATE(device == kDEVICE_CPU && numThreads == 1, null_str);

	std::string filename = modelPath + "/qgme.rknn";

	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);
	int fsize = posix_fsize(file.fp);
	VALIDATE(fsize == 3824537, null_str);
}

FaceDetector::~FaceDetector()
{
}

int FaceDetector::detectMaxFace(std::vector<df_box_t> &outputResult,
                    df_image_t &inputImage)
{
	VALIDATE(outputResult.empty(), null_str);

	VALIDATE(inputImage.data != nullptr && inputImage.width > 0 && inputImage.height > 0, null_str);
	VALIDATE(inputImage.format == kPIXEL_BGRA, null_str);

    int left = inputImage.width / 4;
	int width = inputImage.width * 2 / 4;
	int top = inputImage.height / 5;
	int height = inputImage.height * 3 / 5;

	outputResult.push_back(df_box_t(left, top, width, height));
	df_box_t& box = outputResult.back();
	box.score = 0.782f;
	box.id = 9;

	return outputResult.size();
}

FaceQualityAssessor::FaceQualityAssessor(const std::string &modelPath,
                      df_device_t device, int numThreads)
{
	VALIDATE(device == kDEVICE_CPU && numThreads == 1, null_str);
}

FaceQualityAssessor::~FaceQualityAssessor()
{
}

int FaceQualityAssessor::assessQuality(float &outputScore, df_image_t &inputImage,
                    df_box_t &faceBox)
{
	outputScore = 0.85f; // 0.34f, 0.85f
	return 0;
}

FaceRGBLivenessDetector::FaceRGBLivenessDetector(const std::string &modelPath,
                          bool lightMode,
                          df_device_t device, 
                          int numThreads)
{
	VALIDATE(!lightMode && device == kDEVICE_CPU && numThreads == 1, null_str);
}

FaceRGBLivenessDetector::~FaceRGBLivenessDetector()
{
}

int FaceRGBLivenessDetector::detectLiveness(float &outputResult, const df_image_t &inputImage,
                     const df_box_t &inputBox)
{
	outputResult = 0.79f;
	return 0;
}

FaceFeatureExtractor::FaceFeatureExtractor(df_feature_version_t featureVersion,
                       const std::string &modelPath,
                       df_device_t device, int numThreads)
{
	VALIDATE(featureVersion == kFEATURE_V3 && device == kDEVICE_CPU && numThreads == 1, null_str);
}

FaceFeatureExtractor::~FaceFeatureExtractor()
{
}

int FaceFeatureExtractor::extractFeature(std::vector<unsigned char> &outFeature,
                     df_image_t &inImage, df_box_t &faceBox)
{
	VALIDATE(outFeature.empty(), null_str);
	outFeature.resize(DF_FEATURE_SIZE);

    uint8_t* p_feature_str = &outFeature[0];
    memset(p_feature_str, 0x5a, DF_FEATURE_SIZE);

	{
		std::map<int, faceprint::tst_model>::const_iterator find_it = faceprint::st_models.find(DF_FEATURE_SIZE);
		const faceprint::tst_model& mode = find_it->second;
        VALIDATE(mode.example_feature_dat.size() == DF_FEATURE_SIZE, null_str);
		memcpy(p_feature_str, mode.example_feature_dat.c_str(), mode.example_feature_dat.size());
	}

	return 0;
}

FaceFeatureComparator::FaceFeatureComparator()
{
}

FaceFeatureComparator::~FaceFeatureComparator()
{
}

float FaceFeatureComparator::compareFeature(std::vector<unsigned char> &arr1,
                            std::vector<unsigned char> &arr2) const
{
    VALIDATE(arr1.size() == arr2.size() && arr1.size() == DF_FEATURE_SIZE, null_str);
	return 0.80f;
}

float FaceFeatureComparator::compareFeature(unsigned char *ptr1, unsigned char *ptr2,
                            int length) const
{
    VALIDATE(length == DF_FEATURE_SIZE, null_str);
	return 0.90f;
}

FaceMaskDetector::FaceMaskDetector(const std::string &modelPath,
                   df_device_t device, int numThreads)
{
	VALIDATE(device == kDEVICE_CPU && numThreads == 1, null_str);
}

FaceMaskDetector::~FaceMaskDetector(void)
{
}

FaceTracker::FaceTracker(const std::string &modelPath, df_device_t device,
              int numThreads)
{
	VALIDATE(device == kDEVICE_CPU && numThreads == 1, null_str);
}

FaceTracker::~FaceTracker()
{
}

FaceNIRLivenessDetector::FaceNIRLivenessDetector(const std::string &modelPath,
                          df_device_t device, int numThreads)
{
	VALIDATE(device == kDEVICE_CPU && numThreads == 1, null_str);
}

FaceNIRLivenessDetector::~FaceNIRLivenessDetector()
{
}

LicenseManager::LicenseManager()
{
	SDL_Log("---LicenseManager::LicenseManager()---");
}

LicenseManager::~LicenseManager()
{
	SDL_Log("---LicenseManager::~LicenseManager()---");
}

int LicenseManager::login(void)
{
	return 0;
}

int LicenseManager::updateOnline(char *)
{
	return 0;
}

int LicenseManager::updateOfline(char *)
{
	return 0;
}

int LicenseManager::logout(void)
{
	return 0;
}

int LicenseManager::getFingerPrint(char const *,char *,unsigned int *)
{
	return 0;
}

int LicenseManager::setRootPath(char *)
{
	return 0;
}

int LicenseManager::getVersion(void)
{
	return 0;
}

int LicenseManager::remove(char const *)
{
	return 0;
}

int LicenseManager::setLocalServer(char const *,int,int)
{
	return 0;
}

int LicenseManager::setProxy(char const *,int,char const *,char const *)
{
	return 0;
}

int LicenseManager::revokeOnline(char const *)
{
	return 0;
}

int LicenseManager::revokeOffline(char const *,char *,unsigned int *)
{
	return 0;
}

int LicenseManager::getInfo(char const *,int,char *,unsigned int *)
{
	return 0;
}

#endif

#define DF_OK 0

namespace faceprint {


// const std::string short_license_file_st_sdk2 = "st2.lic";

/*
// use for offline authorize
int add_license_activated(const std::string& license_file, const std::string& signed_code_file)
{
    cv_result_t ret;
    std::unique_ptr<tfile> in;
    in.reset(new tfile(signed_code_file, GENERIC_READ, OPEN_EXISTING));
    int fsize_in = in->read_2_data();
    if (fsize_in == 0) { // file not exists, get a signed code and save it.
        in.reset(nullptr);
        char *signed_code = NULL;
        ret = sdk_video_get_signed_code(license_file.c_str(), &signed_code);
        if (ret != CV_OK) {
            SDL_Log("{st_sdk2}sdk_video_get_signed_code(...) failed with %d", ret);
            return -1;
        }
        SDL_Log("{st_sdk2}sdk_video_get_signed_code(...) ok, signed code is : %s\nit has saved at %s", 
            signed_code, signed_code_file.c_str());

        {
            tfile out(signed_code_file, GENERIC_WRITE, CREATE_ALWAYS);
            VALIDATE(out.valid(), null_str);

            // + '1' is terminal '\0'.
            posix_fwrite(out.fp, signed_code, strlen(signed_code) + 1);
        }

        in.reset(new tfile(signed_code_file, GENERIC_READ, OPEN_EXISTING));
        fsize_in = in->read_2_data();
        VALIDATE(fsize_in == strlen(signed_code) + 1, null_str);

        sdk_video_release_signed_code(signed_code);
    }

    ret = sdk_video_add_license_activated(license_file.c_str(), in->data);
    if (ret != CV_OK) {
        SDL_Log("{st_sdk2}sdk_video_add_license_activated(...) failed with %d", ret);
        return -1;
    }
    SDL_Log("{st_sdk2}sdk_video_add_license_activated(...) successed!");
    return 0;
}
*/

static int last_authorize_result = DF_OK;
bool authorize_st_sdk2(bool onlinestsdk, const char* data)
{
    bool result = false;

    if (onlinestsdk) {
        // SDL_Log("{start_task}1.0, pre license.updateOnline");
        VALIDATE(data != nullptr && SDL_strlen(data) == 16, null_str);

		// char* active_code = "2OXVHY6WIM2BGLTG";
		std::string active_code = data;
		LicenseManager license;
		int ret = license.updateOnline((char*)(active_code.c_str()));
        last_authorize_result = ret;
        if (ret != DF_OK) {
            std::stringstream err;
            err << "{df_sdk}Failed to add license with " << ret;
            // gui2::show_message(null_str, err.str());
            return false;
        }
        result = true;
        SDL_Log("{df_sdk}license.updateOnline(%s) return %d", data, ret);

    } else {
        VALIDATE(false, null_str);
/*
        const std::string LICENSE_FILE = sn_path + "/" + short_license_file_st_sdk2;

        VALIDATE(data != nullptr, null_str);
        cv_result_t ret = sdk_video_add_license_activated(LICENSE_FILE.c_str(), data);
        last_authorize_result = ret;
        SDL_Log("{st_sdk2}sdk_video_add_license_activated(%s, ...) return %d", LICENSE_FILE.c_str(), ret);
        if (ret != CV_OK) {
            SDL_Log("{st_sdk2}sdk_video_add_license_activated() failed with %d", ret);
            return false;
        }
        result = true;
*/
    }

    return result;
}

std::string authorize_extra_err_msg_st_sdk2()
{
    std::stringstream ss;
    ss << "errcode(" << last_authorize_result << ") ";
/*
    const std::string LICENSE_FILE = sn_path + "/" + short_license_file_st_sdk2;
    int fsize = 0;
    {
        tfile file(LICENSE_FILE, GENERIC_READ, OPEN_EXISTING);
        if (file.valid()) {
            fsize = posix_fsize(file.fp);
        }
    }

    utils::string_map symbols;
    symbols["fsize"] = str_cast(fsize);

    std::stringstream ss;
    ss << "errcode(" << last_authorize_result << ") ";
    ss << vgettext2("lic file is $fsize bytes", symbols);
*/
	return ss.str();
}

#define EVALUDATE_FACE_FACE(ptr, box, quality2, to)	\
    to.face_ptr = (void*)(ptr);  \
    to.quality_ptr = to.face_ptr;   \
    EVALUDATE_FACE_RECT2(to.rect, box);    \
    to.score = quality2; \
    to.roll = float_nposm;   \
    to.pitch = float_nposm; \
    to.yaw = float_nposm; \
    to.quality = box.score;

class tface2: public tface
{
public:
    struct tdf_box2 
    {
        tdf_box2(const df_box_t& box, float quality)
            : box(box)
            , quality(quality)
        {}

        df_box_t box;
        float quality;
    };

    tface2(FaceQualityAssessor& qualityAssessor, df_image_t& frame, std::vector<df_box_t>* faceBoxes_ptr, const SDL_Rect& valid_rect, float quality_threshold)
    {
        if (faceBoxes_ptr == nullptr) {
            return;
        }
        int count = faceBoxes_ptr->size();
        
        int non_zero_bonus = 100;
        face_face_t* tmp_faces = (face_face_t*)malloc(count * sizeof(face_face_t));
        int valid_cnt = 0;
        for (int at = 0; at < count; at ++) {
            df_box_t& box = faceBoxes_ptr->at(at);
            
            float qualityScore = float_nposm; 
	        int errcode = qualityAssessor.assessQuality(qualityScore, frame, box);
	        if (qualityScore < quality_threshold) {
		        // SDL_Log("Face quality too low");
		        continue;
	        }

            faceBoxes.push_back(tdf_box2(box, qualityScore));
            face_face_t& tmp = tmp_faces[valid_cnt];
            // 1/2)std::vector's pushback() can modify existed item's address. here save 'at'.
            void* fake_ptr = (void*)(size_t)(valid_cnt + non_zero_bonus);
            EVALUDATE_FACE_FACE(fake_ptr, box, qualityScore, tmp);
            valid_cnt ++;
        }

        calculate_best_at(tmp_faces, valid_cnt, valid_rect);
        // 2/2)replace best_face_.face_ptr with pointer of tdf_box2.
        if (best_face_.valid()) {
            int best_at = size_t_2_int(best_face_.face_ptr) - non_zero_bonus;
            best_face_.face_ptr = &faceBoxes[best_at];
            best_face_.quality_ptr = best_face_.face_ptr;
        }
        VALIDATE((int)faceBoxes.size() >= valid_count, null_str);
        free(tmp_faces);
    }

	~tface2()
    {
    }

    bool valid() const override { return !faceBoxes.empty() && valid_count > 0; }

	// bool valid_ignore_valid_rect(float min_score) const override
	// {  
	//	return det_res != nullptr && quality_res != nullptr &&
    //        count_ignore_valid_rect_ > 0 && best_score_ignore_valid_rect_ >= min_score; 
	// } 

private:
	void blend_write(surface& surf) const override;

public:
    std::vector<tdf_box2> faceBoxes;
};

void tface2::blend_write(surface& surf) const
{
	tsurface_2_mat_lock lock(surf);

    const tdf_box2* best = nullptr;
	if (valid_count != 0) {
		best = reinterpret_cast<const tdf_box2*>(best_face_.quality_ptr);
	}

    face_face_t tmp;
    int at = 0;
	for (std::vector<tdf_box2>::const_iterator it = faceBoxes.begin(); it != faceBoxes.end(); ++ it, at ++) {
		const tdf_box2& box2 = *it;

        // SDL_Log("#%i det{id: %i} quality{id: %i, clarity: %.3f, occlusion_ratio: %.3f}", at, det->id,
        //    quality->id, quality->clarity, quality->occlusion_ratio);

        EVALUDATE_FACE_FACE(&box2, box2.box, box2.quality, tmp);
        bool is_best = &box2 == best;

		blend_write_face(surf, lock.mat, tmp, at, is_best);
/*
		for (int point = 0; point < quality->points_count; point ++) {
			const cv_pointf_t& pt = quality->point_array[point];
			cv::Scalar color = cv::Scalar(0, 0, 255, 255);
            cv::rectangle(lock.mat, cv::Rect(pt.x - 1, pt.y - 1, 3, 3), color, cv::FILLED);
			// cv::rectangle(lock.mat, cv::Rect(pt.x - 2, pt.y - 2, 5, 5), color, cv::FILLED);
		}
*/
	}
}

struct tfeature2: public tfeature
{
	explicit tfeature2(uint8_t* _feature, int size)
        : tfeature((const char*)_feature, size)
        , feature_str(_feature)
    {}
	~tfeature2();

	uint8_t* feature_str;
};

tfeature2::~tfeature2()
{
	if (feature_str != nullptr) {
	    free(feature_str);
        feature_str = nullptr;
    }
}

class tst_sdk2: public tface_sdk
{
public:
	tst_sdk2();
	~tst_sdk2();

	tface* track_get_face(const cv::Mat& argb, const SDL_Rect& valid_rect) override;

	tfeature* verify_get_feature(const cv::Mat& argb, const face_face_t& face) override;
	// std::unique_ptr<char[]> verify_get_feature_str(const surface& surf, float min_score, int min_width, int min_height);

	// void set_base_feature(const tfeature* feature) { base_feature_ = feature; }
	float compare_feature(const tfeature& that) override;
/*
	bool compare_features(char* const *list_feature, int list_count, int top_k, int* top_idxs, float* top_scores) const;
*/
    bool compare_features2(int facestore_at, int& top_idx, float& top_score) const override;
	float singleliveness_score(const cv::Mat& argb, const face_face_t& face) const override;

private:
    const std::string modelPath_;
    const float quality_threshold_;
    const float liveness_threshold_;

	FaceDetector* faceDetector_;
	FaceQualityAssessor* qualityAssessor_;
	FaceRGBLivenessDetector* rgbLiveness_;
	FaceFeatureExtractor* featureExtractor_;
	FaceFeatureComparator* faceFeatureComparator_;
};

tface_sdk* create_st_sdk2()
{
    // VALIDATE(use_st_sdk2, null_str);
	return new tst_sdk2();
}

tst_sdk2::tst_sdk2()
	: modelPath_(preferences_dir + "/tflites/model")
    , quality_threshold_(0.35f)
    , liveness_threshold_(0.5f)
    , faceDetector_(nullptr)
	, qualityAssessor_(nullptr)
	, rgbLiveness_(nullptr)
	, featureExtractor_(nullptr)
	, faceFeatureComparator_(nullptr)
{
	int errcode = -1;

	if (false) {
        SDL_Log("{start_task}1.0, pre license.updateOnline");

		// char* active_code = "2OXVHY6WIM2BGLTG";
		std::string active_code = "2OXVHY6WIM2BGLTG";
		LicenseManager license;
		errcode = license.updateOnline((char*)(active_code.c_str()));

	} else {
        SDL_Log("{start_task}1.0, don't call license.updateOnline");
        errcode = DF_OK;
    }

    df_device_t CPU = kDEVICE_CPU;
	int numThreads = 1;
	SDL_Log("{start_task}1.1, pre new FaceDetector, updateOnline errcode: %i", errcode);
	faceDetector_ = new FaceDetector(modelPath_, CPU, numThreads);

	SDL_Log("{start_task}1.2, pre new FaceQualityAssessor");
	qualityAssessor_ = new FaceQualityAssessor(modelPath_, CPU, numThreads);

	bool lightMode = false;
	SDL_Log("{start_task}1.3, pre new FaceRGBLivenessDetector");
	rgbLiveness_ = new FaceRGBLivenessDetector(modelPath_, lightMode, CPU, numThreads);

	SDL_Log("{start_task}1.4, pre new FaceRGBLivenessDetector");
	df_feature_version_t featureVersion = kFEATURE_V3;

	SDL_Log("{start_task}1.5, pre new FaceFeatureExtractor");
	featureExtractor_ = new FaceFeatureExtractor(featureVersion, modelPath_, CPU, numThreads);

	SDL_Log("{start_task}1.6, pre new FaceFeatureComparator");
	faceFeatureComparator_ = new FaceFeatureComparator;
}

tst_sdk2::~tst_sdk2()
{
    VALIDATE_IN_THIS_THREAD(tid_);

    if (faceFeatureComparator_ != nullptr) {
	    delete faceFeatureComparator_;
    }

    if (featureExtractor_ != nullptr) {
	    delete featureExtractor_;
    }

    if (rgbLiveness_ != nullptr) {
	    delete rgbLiveness_;
    }

    if (qualityAssessor_ != nullptr) {
	    delete qualityAssessor_;
    }

    if (faceDetector_ != nullptr) {
	    delete faceDetector_;
    }
}

tface* tst_sdk2::track_get_face(const cv::Mat& argb, const SDL_Rect& valid_rect)
{
    if (same_thread_) {
        VALIDATE_IN_THIS_THREAD(tid_);
    }
	// RTC_DCHECK(thread_checker_.CalledOnValidThread());
	// VALIDATE(is_neutral_surface(src), null_str);
    VALIDATE(argb.channels() == 4, null_str);

	int count = 0;

	VALIDATE(!argb.empty(), null_str);
	// tsurface_2_mat_lock lock(src);

    df_image_t frame;
    frame.width = argb.cols;
    frame.height = argb.rows;
    frame.format = kPIXEL_BGRA; // ST_PIX_FMT_BGRA8888;
    frame.data = argb.data;

	// df_image_t frame(lock.mat.data, surf->w, surf->h, kPIXEL_BGRA);
	std::vector<df_box_t> faceBoxes;

	// Detect maximum face
	int boxes = faceDetector_->detectMaxFace(faceBoxes, frame);
	// SDL_Log("---(%ix%i) => boxes: %i, outputResult.size: %i---", frame.width, frame.height, boxes, (int)faceBoxes.size());
	VALIDATE(boxes == (int)faceBoxes.size(), null_str);
    if (faceBoxes.empty()) {
        return new tface2(*qualityAssessor_, frame, nullptr, valid_rect, quality_threshold_);
    }
/*
	int at = 0;
	for (std::vector<df_box_t>::const_iterator it = faceBoxes.begin(); it != faceBoxes.end(); ++ it, at ++) {
		const df_box_t& box = *it;
		SDL_Log("%i/%i rect(%i, %i, %i, %i), score: %.5f, id: %i", at, boxes, 
			box.x, box.y, box.width, box.height, box.score, box.id);
	}
	SDL_Log("---------");
*/
    return new tface2(*qualityAssessor_, frame, &faceBoxes, valid_rect, quality_threshold_);
}

tfeature* tst_sdk2::verify_get_feature(const cv::Mat& argb, const face_face_t& face)
{
    if (same_thread_) {
        VALIDATE_IN_THIS_THREAD(tid_);
    }
	// RTC_DCHECK(thread_checker_.CalledOnValidThread());

    VALIDATE(face.valid(), null_str);
    VALIDATE(!argb.empty(), null_str);
    tface2::tdf_box2& box2 = *reinterpret_cast<tface2::tdf_box2*>(face.quality_ptr);

    // tsurface_2_mat_lock lock(surf);
    df_image_t frame;
    frame.width = argb.cols;
    frame.height = argb.rows;
    frame.format = kPIXEL_BGRA; // ST_PIX_FMT_BGRA8888;
    frame.data = argb.data;

    std::vector<unsigned char> outFeature;
    uint32_t start_ticks = SDL_GetTicks();
	int ret = featureExtractor_->extractFeature(outFeature, frame, box2.box);
    VALIDATE(ret == 0, null_str);
    int size = outFeature.size();
	VALIDATE(size == DF_FEATURE_SIZE, null_str);

    // SDL_Log("{st_sdk2}featureExtractor_->extractFeature(...) return: %i, used: %i ms", 
    //        ret, SDL_GetTicks() - start_ticks);

    const uint8_t* outFeature_ptr = &outFeature[0];
    // const std::string hexed = utils::hex_encode_cstyle((const char*)outFeature_ptr, outFeature.size(), ' ');
	// SDL_Log("%s", hexed.c_str());

    uint8_t* feature = (uint8_t*)malloc(size);
    memcpy(feature, outFeature_ptr, size);
    return new tfeature2(feature, size);
}

float tst_sdk2::compare_feature(const tfeature& _that)
{
    if (same_thread_) {
        VALIDATE_IN_THIS_THREAD(tid_);
    }
	// RTC_DCHECK(thread_checker_.CalledOnValidThread());

	VALIDATE(base_feature_ != nullptr, null_str);
    VALIDATE(base_feature_->size == _that.size, null_str);

    const tfeature2& that = *static_cast<const tfeature2*>(&_that);

    float cmp_score = faceFeatureComparator_->compareFeature((uint8_t*)const_cast<char*>(base_feature_->str), (uint8_t*)const_cast<char*>(_that.str), base_feature_->size);
	return cmp_score;
}

bool tst_sdk2::compare_features2(int facestore_at, int& top_idx, float& top_score) const
{
    if (same_thread_) {
        VALIDATE_IN_THIS_THREAD(tid_);
    }

	threading::lock lock(*facestore_mutex.get());
	tlist_feature& list = list_features[facestore_at];

	// should place lock before list.vsize = 0.
	// why? clear_list_feature() maybe execute.
	if (list.vsize == 0) {
#ifdef _WIN32
		if (!use_facestore) {
			top_idx = 0;
			// top_score = 0.9f;
			top_score = 0.7f;
			return true;
		}
#endif
		top_idx = nposm;
		top_score = 0;
		return false;
	}

	// const int top_k = 1;
	// int top_idxs[top_k];
	// float top_scores[top_k];

	top_idx = nposm;
	top_score = 0;

    const int bolb_size = 512;
    // const int float_feature_len = bolb_size / 4;
    top_idx = nposm;
    uint32_t now = SDL_GetTicks();
    float score = 0;
    for (int at = 0; at < list.vsize; at ++) {
        uint8_t *feature2 = (uint8_t*)(list.features[at]);

        score = faceFeatureComparator_->compareFeature((uint8_t*)base_feature_->str, feature2, 512);

        // cv_result_t ret = sdk_video_comparator_feature_compare(
        //    compare_handle_, (const float*)base_feature_->str, feature2, float_feature_len, &score);
        // if (verbose_) {
        //    VALIDATE(ret == CV_OK, null_str);
        // }
        if (score > top_score) {
            top_score = score;
            top_idx = at;
        }
    }

#ifdef _WIN32
	top_idx = list.vsize - 1; // first top_index is last index of list_feature
    top_score = 0.921f; // <==
	// top_score = 0.85f;
	// top_score = 0.78f; // <==
	// top_score = 0.71f;
#endif

    if (verbose_) {
        SDL_Log("sdk_video_comparator_feature_compare * (n:%i), id: %i, score: %.5f, used: %u ms", 
                list.vsize, top_idx, top_score, SDL_GetTicks() - now);
    }

	return true;
}

float tst_sdk2::singleliveness_score(const cv::Mat& argb, const face_face_t& face) const
{
    if (same_thread_) {
        VALIDATE_IN_THIS_THREAD(tid_);
    }

    VALIDATE(!argb.empty(), null_str);
    VALIDATE(face.valid(), null_str);
    tface2::tdf_box2& box2 = *reinterpret_cast<tface2::tdf_box2*>(face.quality_ptr);
	// tsurface_2_mat_lock lock(surf);

    df_image_t frame;
    frame.width = argb.cols;
    frame.height = argb.rows;
    frame.format = kPIXEL_BGRA; // ST_PIX_FMT_BGRA8888;
    frame.data = argb.data;

    float score = float_nposm;
    uint32_t now = SDL_GetTicks();
	int ret = rgbLiveness_->detectLiveness(score, frame, box2.box);
	if (score < 0.76) {
		SDL_Log("Fake face detected");
		// return;
	}
    SDL_Log("rgbLiveness_->detectLiveness, score: %.2f, ret: %i, used: %i ms", score, ret, SDL_GetTicks() - now);
	return score;
}

void delete_st_sdk2(tface_sdk* sdk)
{
    VALIDATE(sdk != nullptr, null_str);
    tst_sdk2* ptr = static_cast<tst_sdk2*>(sdk);
    delete ptr;
}

void delete_face2(tface* face)
{
    VALIDATE(face != nullptr, null_str);
    tface2* ptr = static_cast<tface2*>(face);
    delete ptr;
}

void delete_feature2(tfeature* feature)
{
    VALIDATE(feature != nullptr, null_str);
    tfeature2* ptr = static_cast<tfeature2*>(feature);
    delete ptr;
}

} // namespace faceprint

#endif