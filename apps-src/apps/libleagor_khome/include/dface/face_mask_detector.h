/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_MASK_DETECTOR_H
#define FACE_MASK_DETECTOR_H
#include "face_base.h"
#include <string>

/**
 * @brief FaceMaskDetector API
 */
class API_EXPORTS FaceMaskDetector : public FaceBase {
public:
  /**
   * Constructor
   * @param[in] modelPath input model path
   * @param[in] device device type
   */
  FaceMaskDetector(const std::string &modelPath,
                   df_device_t device = kDEVICE_CPU, int numThreads = 1);

  /**
   * Detect face mask
   * @param[out] outputScore output mask score(>0.5:wear mask)
   * @param[in] inputImage input image
   * @param[in] faceBox input face box
   * @return status
   */
  int detectMask(float &outputScore, df_image_t &inputImage, df_box_t &faceBox);

  virtual ~FaceMaskDetector();

private:
  int initialize(const std::string &modelPath,
                 df_device_t device = kDEVICE_CPU, int numThreads = 1);
  void *net = nullptr;
};

#endif