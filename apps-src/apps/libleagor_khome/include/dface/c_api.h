#ifndef DFACE_C_API_H
#define DFACE_C_API_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Face detector functions
API_EXPORTS void* CreateFaceDetector(const char* model_path, df_device_t device, int num_threads);
API_EXPORTS int DetectFace(void* detector, df_box_t* output_result, df_image_t* input_image);
API_EXPORTS int DetectMaxFace(void* detector, df_box_t* output_result, df_image_t* input_image);
API_EXPORTS void SetMinFaceSize(void* detector, int size);
API_EXPORTS void DestroyFaceDetector(void* detector);

// Face Feature Extractor functions
API_EXPORTS void* CreateFaceFeatureExtractor(df_feature_version_t feature_version, const char* model_path, df_device_t device, int num_threads);
API_EXPORTS int ExtractFeature(void* extractor, unsigned char* output_feature, int* feature_size, df_image_t* face_image, df_box_t* face_box);
API_EXPORTS void DestroyFaceFeatureExtractor(void* extractor);

// NIR Liveness Detector functions
API_EXPORTS void* CreateNIRLivenessDetector(const char* model_path, df_device_t device, int num_threads);
API_EXPORTS int DetectNIRLiveness(void* detector, float* output_result, df_image_t* input_nir_image, df_box_t* input_nir_box);
API_EXPORTS void DestroyNIRLivenessDetector(void* detector);

// RGB Liveness Detector functions
API_EXPORTS void* CreateRGBLivenessDetector(const char* model_path, df_device_t device, int num_threads, bool light_mode);
API_EXPORTS int DetectRGBLiveness(void* detector, float* output_result, df_image_t* input_rgb_image, df_box_t* input_rgb_box);
API_EXPORTS void DestroyRGBLivenessDetector(void* detector);

// Face Quality Assessor functions
API_EXPORTS void* CreateFaceQualityAssessor(const char* model_path, df_device_t device, int num_threads);
API_EXPORTS int AssessBlur(void* assessor, float* output_result, df_image_t* input_image, df_box_t* input_box);
API_EXPORTS int AssessBrightness(void* assessor, int* output_result, df_image_t* input_image, df_box_t* input_box);
API_EXPORTS int AssessOcclusion(void* assessor, float* output_score, df_image_t* input_image, df_box_t* input_box, df_face_part_t face_part);
API_EXPORTS int AssessPose(void* assessor, df_point_t* output_landmarks, int* num_landmarks, float* output_pose, int* num_pose, df_image_t* input_image, df_box_t* input_box);
API_EXPORTS void SetCameraParameter(void* assessor, float focal_length, float pixel_size, int resolution_width, int resolution_height);
API_EXPORTS int AssessQuality(void* assessor, float* output_score, df_image_t* face_image, df_box_t* face_box);
API_EXPORTS void DestroyFaceQualityAssessor(void* assessor);

// Face Mask Detector functions
API_EXPORTS void* CreateMaskDetector(const char* model_path, df_device_t device, int num_threads);
API_EXPORTS int DetectMask(void* detector, float* output_score, df_image_t* face_image, df_box_t* face_box);
API_EXPORTS void DestroyMaskDetector(void* detector);

// Face Tracker functions
API_EXPORTS void* CreateFaceTracker(const char* model_path, df_device_t device, int num_threads);
API_EXPORTS int InitFaceTracker(void* tracker, df_image_t* input_image);
API_EXPORTS int TrackFace(void* tracker, df_box_t* output_boxes, int* num_faces, df_image_t* input_image);
API_EXPORTS void DestroyFaceTracker(void* tracker);

// Feature comparison functions
API_EXPORTS void* CreateFaceFeatureComparator();
API_EXPORTS void DestroyFaceFeatureComparator(void* comparator);
API_EXPORTS float CompareFeature(void* comparator, const float* ptr1, const float* ptr2, int len);
API_EXPORTS float CompareFeatureBytes(void* comparator, const unsigned char* ptr1, const unsigned char* ptr2, int len);
API_EXPORTS void Base64Encode(void* comparator, const unsigned char* inChars, int inLen, char* outString, int* outLen);
API_EXPORTS void Base64Decode(void* comparator, const char* encodedString, unsigned char* outChars, int* outLen);

// Image processing functions
API_EXPORTS int CreateImage(df_image_t* out_image, int width, int height, int format, int is_malloc);
API_EXPORTS int CreateEmptyImage(df_image_t* out_image);
API_EXPORTS void FreeImage(df_image_t* img);
API_EXPORTS int CloneImage(df_image_t* out_image, const df_image_t* source);
API_EXPORTS void CopyImage(const df_image_t* source, df_image_t* dest);
API_EXPORTS void CropImage(const df_image_t* original_image, int x, int y, int cropped_width, int cropped_height, df_image_t* cropped_image);
API_EXPORTS void RGB2BGR(const df_image_t* rgb_image, df_image_t* bgr_image);
API_EXPORTS void BGR2RGB(const df_image_t* bgr_image, df_image_t* rgb_image);
API_EXPORTS int CopyMakeBorder(df_image_t* out_image, const df_image_t* src, int top, int bottom, int left, int right);

// License management functions
API_EXPORTS void* CreateLicenseManager();
API_EXPORTS void DestroyLicenseManager(void* manager);
API_EXPORTS int Login(void* manager);
API_EXPORTS int UpdateOnline(void* manager, const char* authCode);
API_EXPORTS int UpdateOffline(void* manager, const char* acFilePath);
API_EXPORTS int Logout(void* manager);
API_EXPORTS int GetFingerPrint(void* manager, const char* authCode, char* fingerPrintInfo, unsigned int* fingerPrintSize);
API_EXPORTS int SetRootPath(void* manager, const char* path);
API_EXPORTS int GetVersion(void* manager);
API_EXPORTS int RemoveLicense(void* manager, const char* authCode);
API_EXPORTS int SetLocalServer(void* manager, const char* hostName, int port, int timeoutSeconds);
API_EXPORTS int SetProxy(void* manager, const char* hostName, int port, const char* userId, const char* password);
API_EXPORTS int RevokeOnline(void* manager, const char* authCode);
API_EXPORTS int RevokeOffline(void* manager, const char* authCode, char* revocationInfo, unsigned int* revocationInfoSize);
API_EXPORTS int GetLicenseInfo(void* manager, const char* authCode, int type, char* pInfo, unsigned int* pInfoSize);

#ifdef __cplusplus
}
#endif

#endif // DFACE_C_API_H
