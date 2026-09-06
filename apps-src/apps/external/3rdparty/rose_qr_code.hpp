#ifndef LIBROSE2_QR_CODE_HPP_INCLUDED
#define LIBROSE2_QR_CODE_HPP_INCLUDED

#include <string>
#include <opencv2/imgproc.hpp>

// Unfortunately, @corners obtained @find_qr is not very accurate, it can be indicated as a rough idea only.
// To be accurate, use cv::QRCodeDetector. But it's not as successful as @find_qr is.
//   cv::QRCodeDetector qrcode;
//   qrcode.detect(src, corners);
std::string find_qr(const cv::Mat& image, std::vector<cv::Point>* corners = nullptr);

std::string find_barcode(const cv::Mat& image, std::vector<cv::Point>* corners);

#endif
