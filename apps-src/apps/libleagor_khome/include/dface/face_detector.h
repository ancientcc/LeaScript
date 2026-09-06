/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_DETECTOR_H
#define FACE_DETECTOR_H
#include "face_base.h"
#include <string>
#include <vector>

/**
 * @brief FaceDetector API
 */
class API_EXPORTS FaceDetector : public FaceBase {
public:
  /**
   * Constructor
   * @param[in] modelPath input model path
   * @param[in] device device type
   */
  FaceDetector(const std::string &modelPath, df_device_t device = kDEVICE_CPU, int numThreads = 1);

  /**
   * Detect faces in image
   * @param[out] outputResult output face boxes
   * @param[in] inputImage input image
   * @return number of faces detected
   */
  int detectFace(std::vector<df_box_t> &outputResult, df_image_t &inputImage);

  /**
   * Detect largest face in image
   * @param[out] outputResult output face box
   * @param[in] inputImage input image
   * @return number of faces detected
   */
  int detectMaxFace(std::vector<df_box_t> &outputResult,
                    df_image_t &inputImage);

  /**
   * Set minimum face size for detection
   * @param[in] size minimum face size
   */
  void setMinFaceSize(int size);


  virtual ~FaceDetector();

private:
  int initialize(const std::string &modelPath,
                 df_device_t device = kDEVICE_CPU, int numThreads = 1);
  df_detect_mode_t detect_mode = kFAST_DETECT;
  int min_face_size = 80;
  void *net = nullptr;
  int mxt = 4;
};

#endif