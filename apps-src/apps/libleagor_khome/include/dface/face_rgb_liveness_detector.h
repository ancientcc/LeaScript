/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_RGBLIVENESS_DETECTOR_H
#define FACE_RGBLIVENESS_DETECTOR_H
#include "face_base.h"
#include <string>

/**
 * @brief FaceRGBLivenessDetector API
 */
class API_EXPORTS FaceRGBLivenessDetector : public FaceBase {
public:
  /**
   * Construct fun
   * @param[in] modelPath input model path
   * @param[in] lightMode if true, uses light version of the model; if false, uses full version with enhanced security
   * @param[in] device device type
   * @param[in] numThreads number of threads to use
   */
  FaceRGBLivenessDetector(const std::string &modelPath,
                          bool lightMode = false,
                          df_device_t device = kDEVICE_CPU, 
                          int numThreads = 1);
  /**
   * RGB-liveness check
   * @param[out] outputResult output liveness score (higher score,more liveness)
   * @param[in] inputImage input rgb camera frame image
   * @param[in] inputBox input face box(detect on rgb image)
   * @return status
   */
  int detectLiveness(float &outputResult, const df_image_t &inputImage,
                     const df_box_t &inputBox);

  /**
   * preprocess image and output the preprocess image
   * @param[out] outputImage output preprocess image
   * @param[in] inputImage input image
   * @param[in] inputBox input face box
   * @return status
   */
  int preprocess(std::vector<df_image_t> &outputImage,
                 const df_image_t &inputImage, const df_box_t &inputBox);

  /**
   * RGB-liveness check by preprocess image
   * @param[out] outputResult output liveness score (higher score,more liveness)
   * @param[in] inputPreprocessImg input preprocess image
   * @return status
   */
  int detectLivenessFromPreprocess(float &outputResult,
                     const std::vector<df_image_t> &inputPreprocessImg);

  virtual ~FaceRGBLivenessDetector();

private:
  int initialize(const std::string &modelPath,
                 bool lightMode = true,
                 df_device_t device = kDEVICE_CPU,
                 int numThreads = 1);
  void *net = nullptr;
  int mxt = 8;
  int uid;
};

#endif