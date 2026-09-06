/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_NIRLIVENESS_DETECTOR_H
#define FACE_NIRLIVENESS_DETECTOR_H
#include "face_base.h"
#include <string>

/**
 * @brief FaceNIRLivenessDetector API
 */
class API_EXPORTS FaceNIRLivenessDetector : public FaceBase {
public:
  /**
   * Construct fun
   * @param[in] modelPath input model path
   * @param[in] device device type
   * @param[in] numThreads number of threads to use
   */
  FaceNIRLivenessDetector(const std::string &modelPath,
                          df_device_t device = kDEVICE_CPU, int numThreads = 1);
  /**
   * IR-liveness check
   * @param[out] outResult output liveness score (higher score,more liveness)
   * @param[in] inputNIRImage input nir image
   * @param[in] inputNIRBox input face box(detect on nir image)
   * @return status
   */
  int detectLiveness(float &outResult, df_image_t &inputNIRImage,
                     df_box_t &inputNIRBox);

  /**
   * preprocess image and output the preprocess image
   * @param[out] outputImage output preprocess image
   * @param[in] inputNIRImage input nir image
   * @param[in] inputNIRBox input face box(detect on nir image)
   * @return status
   */
  int preprocess(std::vector<df_image_t> &outputImage,
                 df_image_t &inputNIRImage, df_box_t &inputNIRBox);

  /**
   * IR-liveness check by preprocess image
   * @param[out] outResult output liveness score (higher score, more liveness)
   * @param[in] inputPreprocessImg input preprocess image
   * @return status
   */
  int detectLivenessFromPreprocess(float &outResult,
                     std::vector<df_image_t> &inputPreprocessImg);

  virtual ~FaceNIRLivenessDetector();

private:
  int initialize(const std::string &modelPath, df_device_t device = kDEVICE_CPU,
                 int numThreads = 1);
  void *net = nullptr;
  int mxt = 8;
  int uid;
};

#endif