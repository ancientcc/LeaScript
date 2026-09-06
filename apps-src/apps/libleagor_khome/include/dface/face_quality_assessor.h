/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_QUALITY_ASSESSOR_H
#define FACE_QUALITY_ASSESSOR_H
#include "face_base.h"
#include <string>

/**
 * @brief FaceQualityAssessor API
 */
class API_EXPORTS FaceQualityAssessor : public FaceBase {
public:
  /**
   * Constructor
   * @param[in] modelPath input model path
   * @param[in] device device type
   * @param[in] numThreads number of threads to use
   */
  FaceQualityAssessor(const std::string &modelPath,
                      df_device_t device = kDEVICE_CPU, int numThreads = 1);

  /**
   * Assess face quality
   * @param[out] outputScore output quality score (higher score,better quality)
   * @param[in] inputImage input image
   * @param[in] faceBox input face box
   * @return status
   */
  int assessQuality(float &outputScore, df_image_t &inputImage,
                    df_box_t &faceBox);

  /**
   * Assess face blur
   * @param[out] out_result output face blur value (lower score, more blur)
   * @param[in] in_image input image
   * @param[in] in_box input face box
   * @return void
   */
  void assessBlur(float &out_result, df_image_t &in_image, df_box_t &in_box);

  /**
   * Assess face Brightness
   * @param[out] out_result output face light type
   * @param[in] in_image input image
   * @param[in] in_box input face box
   * @return void
   */
  void assessBrightness(int &out_result, df_image_t &in_image,
                        df_box_t &in_box);


  /**
   * Assess face occlusion
   * @param[out] outputScore output occlusion score (higher score, more occlusion)
   * @param[in] inputImage input image
   * @param[in] faceBox input face box
   * @param[in] facePart input face part (eye, nose, mouth)
   * @return void
   */
  void assessOcclusion(float &outputScore, df_image_t &inputImage,
                      df_box_t &faceBox, df_face_part_t facePart);
  

    /**
   * Assess face pose
   * @param[out] outputLandmarks output landmarks
   * [x0,y0,x1,y1,x2,y2...x105,y105]
   * @param[out] out_pose output pose [yaw, pitch, row, x_offset, y_offset,
   * z_offset]
   * @param[in] inputImage input image
   * @param[in] faceBox input face box
   * @return status
   */
  void assessPose(std::vector<df_point_t> &outputLandmarks,
                                    std::vector<float> &out_pose,
                                    df_image_t &inputImage, df_box_t &faceBox);

  /**
   * Set camera parameter
   * @param[in] focal_length input focal length
   * @param[in] pixel_size input pixel size
   * @param[in] resolution_width input resolution width
   * @param[in] resolution_height input resolution height
   * @return void
   */
  void setCameraParameter(float focal_length, float pixel_size,
                          int resolution_width, int resolution_height);

  virtual ~FaceQualityAssessor();

private:
  int initialize(const std::string &modelPath,
                 df_device_t device = kDEVICE_CPU, int numThreads = 1);
  void *net = nullptr;
  void *net2 = nullptr;
  void *net3 = nullptr;
  void *net4 = nullptr;
  void *net5 = nullptr;
  float focal_length;
  float pixel_size;
  int resolution_width;
  int resolution_height;
  int mxt = 8;
};

#endif