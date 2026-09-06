/*****
Copyright @ 2017-2024, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef DFACE_BASE_H
#define DFACE_BASE_H
#include "dface/common.h"
#include <vector>

/**
 * @brief Base class API
 */
class API_EXPORTS FaceBase {
public:
  // virtual int init(void *in_config) = 0;
  virtual ~FaceBase() { return; };
};

#endif