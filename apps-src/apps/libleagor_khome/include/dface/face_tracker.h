/*****
Copyright @ 2017-2024, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_TRACKER_H
#define FACE_TRACKER_H
#include "face_base.h"
#include <string>

class API_EXPORTS FaceTracker : public FaceBase {
public:
  /**
   * Constructor
   * @param[in] modelPath input model path
   * @param[in] device device type
   * @param[in] numThreads number of threads to use
   */
  FaceTracker(const std::string &modelPath, df_device_t device = kDEVICE_CPU,
              int numThreads = 1);

  /**
   * Initialize tracker with first frame
   * @param[in] inputImage first frame image
   * @return status
   */
  int init(df_image_t &inputImage);

  /**
   * Track faces, every face get an unique track id during tracking.
   * @param[out] outputBoxes output track face box (In order of face size from
   * largest to smallest)
   * @param[in] inputImage input image
   * @return status
   */
  int track(std::vector<df_box_t> &outputBoxes, df_image_t &inputImage);

  /**
   * Set track mode(kFAST_TRACK, kNORMAL_TRACK, kSLOW_TRACK)
   * @param[in] mode track mode
   * kFAST_TRACK: fast speed, low precesion;
   * kNORMAL_TRACK: normal speed, normal precesion;
   * kSLOW_TRACK: slow speed, high precesion;
   */
  void setTrackMode(df_track_mode_t mode);

  /**
   * Set minimum face size for tracking
   * @param[in] size minimum face size
   */
  void setMinFaceSize(int size);

  /**
   * Set num threads
   * @param[in] num_threads num threads
   */
  void setNumThreads(int num_threads);

  /**
   * Set maximum face detection interval
   * @param[in] interval maximum interval in milliseconds
   */
  void setMaxDetectionInterval(int interval);

  virtual ~FaceTracker();

private:
  int initialize(const std::string &modelPath,
                 df_device_t device = kDEVICE_CPU, int num_threads = 1);
  df_track_mode_t track_mode = kFAST_TRACK;
  int min_face_size = 80;
  void *hd;
  int max_track_num = 50;
  int max_age = 200;
  float max_cosine_distance = 0.2f;
  float max_iou_distance = 0.7f;
  int min_hit = 3;
  void *h_tracker = nullptr;
  long since_update_time_ = 0;
  bool on_start = true;
};

#endif