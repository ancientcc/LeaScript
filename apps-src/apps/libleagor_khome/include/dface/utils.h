/*****
Copyright @ 2017-2024, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef DFACE_UTILS_H
#define DFACE_UTILS_H

#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

union FloatType {
  float FloatNum;
  int IntNum;
};

typedef union {
  int i;
  char c;
} my_union;

static int checkSystem(void) {
  my_union u;
  u.i = 1;
  return (u.i == u.c);
}

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

static std::string base64_encode(unsigned char const *bytes_to_encode,
                                 unsigned int in_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] =
          ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] =
          ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i < 4); i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] =
        ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] =
        ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while ((i++ < 3))
      ret += '=';
  }
  return ret;
}

static void base64_decode(std::string const &encoded_string,
                          std::vector<unsigned char> &out_chars) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  //   std::vector<unsigned char> ret;

  while (in_len-- && (encoded_string[in_] != '=') &&
         is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_];
    in_++;
    if (i == 4) {
      for (i = 0; i < 4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);

      char_array_3[0] =
          (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] =
          ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        out_chars.push_back(char_array_3[i]);
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++)
      char_array_4[j] = 0;

    for (j = 0; j < 4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] =
        ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) {
      out_chars.push_back(char_array_3[j]);
    }
  }
}

static void convertByteArray2FloatArray(unsigned char *bytes, int length,
                                        std::vector<float> &feature) {
  int feathreLen = length / 4;
  for (int i = 0; i < feathreLen; i++) {
    FloatType Number;
    Number.IntNum = 0;
    Number.IntNum = bytes[4 * i + 3];
    Number.IntNum = (Number.IntNum << 8) | bytes[4 * i + 2];
    Number.IntNum = (Number.IntNum << 8) | bytes[4 * i + 1];
    Number.IntNum = (Number.IntNum << 8) | bytes[4 * i + 0];
    feature.push_back(Number.FloatNum);
  }
}

static unsigned char *convertFloatArray2ByteArray(std::vector<float> &feature,
                                                  int &length) {
  if (feature.size() == 0) {
    length = 0;
    return NULL;
  }
  length = 4 * feature.size();
  unsigned char *DataBuf = new unsigned char[length];
  for (int i = 0; i < feature.size(); i++) {
    FloatType Number;
    Number.FloatNum = feature[i];
    DataBuf[4 * i + 0] = (unsigned char)Number.IntNum;
    DataBuf[4 * i + 1] = (unsigned char)(Number.IntNum >> 8);
    DataBuf[4 * i + 2] = (unsigned char)(Number.IntNum >> 16);
    DataBuf[4 * i + 3] = (unsigned char)(Number.IntNum >> 24);
  }
  return DataBuf;
}

static void convertFloatArray2String(std::vector<float> &feature,
                                     std::string &str) {
  if (feature.size() == 0) {
    return;
  }
  std::stringstream stmstr;
  for (int i = 0; i < feature.size(); i++) {
    stmstr << feature[i] << ',';
  }
  std::string restr = stmstr.str();
  str = restr.substr(0, restr.length() - 1);
}

static std::vector<std::string> explodeString(const std::string &s,
                                              const char &c) {
  std::string buff;
  std::vector<std::string> v;
  char tmp;
  for (int i = 0; i < s.size(); i++) {
    tmp = s[i];
    if (tmp != c) {
      buff += tmp;
    } else {
      if (tmp == c && buff != "") {
        v.push_back(buff);
        buff = "";
      }
    } // endif
  }
  if (buff != "") {
    v.push_back(buff);
  }
  return v;
}

static void convertString2FloatArray(std::string &str,
                                     std::vector<float> &feature) {
  std::vector<std::string> fstrs = explodeString(str, ',');
  if (fstrs.size() == 0) {
    return;
  }
  for (int i = 0; i < fstrs.size(); i++) {
    float f = atof(fstrs[i].c_str());
    feature.push_back(f);
  }
}

// vector arrayNorm
static inline float arrayNorm(const std::vector<float> &vec) {
  int n = vec.size();
  float sum = 0.0;
  for (int i = 0; i < n; ++i)
    sum += vec[i] * vec[i];
  return sqrt(sum);
}

/**
* Compare byte array cosine similarity.
* @param[in] arr1 First array
* @param[in] arr2  Second array
* @return similarity volume
* @note  cosine = (v1 dot v2)/(||v1|| * ||v2||)
*/
static float byteArraySimilarity(std::vector<unsigned char> &arr1,
                                 std::vector<unsigned char> &arr2) {
  std::vector<float> feature_arr1;
  std::vector<float> feature_arr2;
  convertByteArray2FloatArray(arr1.data(), arr1.size(), feature_arr1);
  convertByteArray2FloatArray(arr2.data(), arr2.size(), feature_arr2);

  int n = feature_arr1.size();
  float tmp = 0.0;
  for (int i = 0; i < n; ++i) {
    tmp += feature_arr1[i] * feature_arr2[i];
  }
  float cosine = tmp / (arrayNorm(feature_arr1) * arrayNorm(feature_arr2));
  float norm_cosine = 0.5 + 0.5 * cosine;
  return norm_cosine;
}

/**
* Compare float array cosine similarity.
* @param[in] arr1 First array
* @param[in] arr2  Second array
* @return similarity volume
* @note  cosine = (v1 dot v2)/(||v1|| * ||v2||)
*/
static float floatrraySimilarity(std::vector<float> &arr1,
                                 std::vector<float> &arr2) {
  int n = arr1.size();
  float tmp = 0.0;
  for (int i = 0; i < n; ++i) {
    tmp += arr1[i] * arr2[i];
  }
  float cosine = tmp / (arrayNorm(arr1) * arrayNorm(arr2));
  float norm_cosine = 0.5 + 0.5 * cosine;
  return norm_cosine;
}

static std::string getPathName(const std::string &s) {
  char sep = '/';
#ifdef _WIN32
  sep = '\\';
#endif
  size_t i = s.rfind(sep, s.length());
  if (i != std::string::npos) {
    return (s.substr(0, i));
  }
  return ("");
}

#endif // DFACE_UTILS_H
