/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef DFACE_COMMON_HPP
#define DFACE_COMMON_HPP

#include <stddef.h>

#if (defined WIN32 || defined _WIN32 || defined WINCE || defined __CYGWIN__)
#define API_EXPORTS __declspec(dllexport)
#elif defined __GNUC__ && __GNUC__ >= 4
#define API_EXPORTS __attribute__((visibility("default")))
#else
#define API_EXPORTS
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief dface image format */
typedef enum df_img_format {
  kPIXEL_RGB = 1,
  kPIXEL_BGR = 1 << 1,
  kPIXEL_GRAY = 1 << 2,
  kPIXEL_RGBA = 1 << 3,
  kPIXEL_BGRA = 1 << 4,
  kPIXEL_NV21 = 1 << 5,
  kPIXEL_YUY2 = 1 << 6,
} df_img_format_t;

/** @brief dface face part */
typedef enum df_face_part {
  kFACE_EYE = 0,
  kFACE_NOSE = 1,
  kFACE_MOUTH = 2,
  kFACE_LEFT_EYE = 3,
  kFACE_RIGHT_EYE = 4,
} df_face_part_t;

/** @brief dface face box */
typedef struct df_box {
  int x;
  int y;
  int width;
  int height;
  float score;
  int id;
  float landmark[10];
  df_box() {}
  df_box(int x, int y, int width, int height)
      : x(x), y(y), width(width), height(height) {}
} df_box_t;

/** @brief dface point */
typedef struct df_point {
  int x;
  int y;
} df_point_t;

/** @brief dface image */
typedef struct df_image {
  void *data;
  int width;
  int height;
  df_img_format_t format;
  int mem_aligned;
  df_image() {
    data = NULL;
    width = 0;
    height = 0;
    mem_aligned = 0;
    format = kPIXEL_RGB;
  }
  df_image(void *data, int width, int height, df_img_format_t format)
      : data(data), width(width), height(height), format(format) {
    mem_aligned = 0;
  }
  df_image(void *data, int width, int height, df_img_format_t format,
           int mem_aligned)
      : data(data), width(width), height(height), format(format),
        mem_aligned(mem_aligned) {}
} df_image_t;

/** @brief dface face detect mode */
typedef enum df_device {
  kDEVICE_CPU = 0,
  kDEVICE_GPU = 1,
  kDEVICE_NPU = 2,
} df_device_t;

/** @brief face feature version */
typedef enum df_feature_version {
  kFEATURE_V3 = 0,       /**< feature version v3 */
  kFEATURE_V8_TINY = 1,  /**< feature version v8 tiny */
  kFEATURE_V8_SMALL = 2, /**< feature version v8 small  */
  kFEATURE_V8_BIG = 3,   /**< feature version v8 big  */
} df_feature_version_t;


/** @brief dface get licence info type */
typedef enum df_licence_info {
  INFO_SERVER_ADDRESS = 0,
  INFO_SN = 1,
  INFO_SN_FEATURE = 2,
  INFO_SN_LICENSE = 3,
  INFO_UPDATE_ERROR = 4,
  INFO_CONFIG = 5
} df_licence_info_t;

/** @brief dface face detect mode */
typedef enum df_detect_mode {
  kFAST_DETECT = 0,
  kNORMAL_DETECT = 1,
  kSLOW_DETECT = 2,
} df_detect_mode_t;

/** @brief dface face brightness type */
typedef enum df_brightness_type {
  kBRIGHTNESS_NORMAL = 0,
  kBRIGHTNESS_DARK = 1,
  kBRIGHTNESS_LIGHT = 2,
} df_brightness_type_t;

/** @brief dface tracker working mode */
typedef enum df_track_mode {
  kFAST_TRACK = 0,
  kNORMAL_TRACK = 1,
  kSLOW_TRACK = 2
} df_track_mode_t;

#ifdef __cplusplus
}
#endif

#endif