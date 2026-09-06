/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_FEATURE_EXTRACTOR_H
#define FACE_FEATURE_EXTRACTOR_H
#include "face_base.h"
#include <string>

/**
 * @brief FaceFeatureExtractor API
 */
class API_EXPORTS FaceFeatureExtractor : public FaceBase {
public:
  /**
   * Constructor
   * @param[in] featureVersion input feature version
   * @param[in] modelPath input model path
   * @param[in] device device type
   * @param[in] numThreads number of threads to use
   */
  FaceFeatureExtractor(df_feature_version_t featureVersion,
                       const std::string &modelPath,
                       df_device_t device = kDEVICE_CPU, int numThreads = 1);

  /**
   * Extract face features from image and face box
   * @param[out] outFeature output feature data
   * @param[in] inImage input image
   * @param[in] faceBox input face box
   * @return status
   */
  int extractFeature(std::vector<unsigned char> &outFeature,
                     df_image_t &inImage, df_box_t &faceBox);

  /**
   * Preprocess image for feature extraction
   * @param[out] outPreprocessedImage output preprocessed image
   * @param[in] inImage input image
   * @param[in] faceBox input face box
   * @return status
   */
  int preprocess(df_image_t &outPreprocessedImage, df_image_t &inImage,
                 df_box_t &faceBox);

  /**
   * Extract face features from preprocessed image
   * @param[out] outFeature output feature data
   * @param[in] inPreprocessedImage input preprocessed image
   * @return status
   */
  int extractFeatureFromPreprocess(std::vector<unsigned char> &outFeature,
                     df_image_t &inPreprocessedImage);

  int getVersion();
  virtual ~FaceFeatureExtractor();

private:
  int initialize(df_feature_version_t featureVersion,
                 const std::string &modelPath, df_device_t device,
                 int numThreads);
  void *net = nullptr;
  void *net2 = nullptr;
  int version;
  int mxt = 8;
};

#endif