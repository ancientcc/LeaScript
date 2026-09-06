/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_FEATURE_COMPARATOR_H
#define FACE_FEATURE_COMPARATOR_H

#include "dface/common.h"
#include "face_base.h"
#include <string>

/**
 * @brief FaceFeatureComparator API
 */
class API_EXPORTS FaceFeatureComparator : public FaceBase {
public:
  float compareFeature(std::vector<unsigned char> &arr1,
                            std::vector<unsigned char> &arr2) const;

  float compareFeature(unsigned char *ptr1, unsigned char *ptr2,
                            int length) const;

  float compareFeature(std::vector<float> &arr1,
                             std::vector<float> &arr2) const;

  float compareFeature(const float *ptr1, const float *ptr2,
                             int len) const;

  void base64Encode(unsigned char const *in_chars,
                           int in_len, std::string &encoded_string);

  void base64Encode(std::vector<unsigned char> &in_chars,
                           std::string &encoded_string);

  void base64Decode(std::string const &encoded_string,
                           unsigned char *out_chars, int &out_len);

  void base64Decode(std::string const &encoded_string,
                           std::vector<unsigned char> &out_chars);

  explicit FaceFeatureComparator();
  virtual ~FaceFeatureComparator();

private:
  bool initialize();
};

#endif // FACE_COMPARE_H
