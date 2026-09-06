//______________________________________________________________________________________
// Program : OpenCV based QR code Detection and Retrieval
// Author  : Bharath Prabhuswamy
// Github  : https://github.com/bharathp666/opencv_qr
//______________________________________________________________________________________


#include "rose_global.hpp"

#include <iostream>
#include <cmath>
#include "rose_util.hpp"
#include "rose_qr_code.hpp"
#include "rose_config_3rdparty.hpp"
#include "rose_filesystem.hpp"
#include "rose_exception.hpp"

#include "gettext.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <opencv2/wechat_qrcode.hpp>
#include <opencv2/objdetect/barcode.hpp>

#include <SDL.h>
// #include "zxing/BarcodeFormat.h"
// #include "zxing/MultiFormatWriter.h"
// #include "zxing/TextUtfEncoding.h"
// #include "zxing/BitMatrix.h"
// #include "zxing/ByteMatrix.h"


std::string find_qr(const cv::Mat& image, std::vector<cv::Point>* corners)
{
	VALIDATE(!image.empty(), null_str);

	std::string result;
	std::stringstream ss;

	std::vector<cv::Mat> points;

    // can not find the model file
    // so we temporarily comment it out
	// const std::string prefix = game_config::preferences_dir + "/tflites/wechat_qrcode/";
	const std::string prefix = game_config::path + "/data/core/tflites/wechat_qrcode/";

	tfile detect_prototxt(prefix + "detect.prototxt", GENERIC_READ, OPEN_EXISTING);
	size_t detect_prototxt_len = detect_prototxt.read_2_data();
	VALIDATE(detect_prototxt_len != 0, null_str);

	tfile detect_caffemodel(prefix + "detect.caffemodel", GENERIC_READ, OPEN_EXISTING);
	size_t detect_caffemodel_len = detect_caffemodel.read_2_data();
	VALIDATE(detect_caffemodel_len != 0, null_str);

	tfile sr_prototxt(prefix + "sr.prototxt", GENERIC_READ, OPEN_EXISTING);
	size_t sr_prototxt_len = sr_prototxt.read_2_data();
	VALIDATE(sr_prototxt_len != 0, null_str);

	tfile sr_caffemodel(prefix + "sr.caffemodel", GENERIC_READ, OPEN_EXISTING);
	size_t sr_caffemodel_len = sr_caffemodel.read_2_data();
	VALIDATE(sr_caffemodel_len != 0, null_str);

	uint32_t start_ticks = SDL_GetTicks();
    auto detector = cv::wechat_qrcode::WeChatQRCode(
         detect_prototxt.data, detect_prototxt_len, detect_caffemodel.data, detect_caffemodel_len, 
		 sr_prototxt.data, sr_prototxt_len, sr_caffemodel.data, sr_caffemodel_len);
	uint32_t end_step1_ticks = SDL_GetTicks();
    std::vector<std::string> decoded_info = detector.detectAndDecode(image, points);

	uint32_t stop_ticks = SDL_GetTicks();
	if (!decoded_info.empty()) {
		// Even with the same value, for example "2283901501"), two decoded_info will appear.
		// Only take the first one, and the same 'points' has to take the first one.
		result = decoded_info.at(0);

	} else if (!points.empty()) {
		int ii = 0;
	}

	ss << image.cols << "x" << image.rows << ": find_qr, used: ";
	ss << (stop_ticks - start_ticks) << "(WeChatQRCode:" << (end_step1_ticks - start_ticks) << " + detectAndDecode:" << (stop_ticks - end_step1_ticks) << ")";

	if (corners != nullptr) {
		corners->clear();
		bool found_outside = false;
		for (std::vector<cv::Mat>::const_iterator it = points.begin(); !found_outside && it != points.end(); ++ it) {
			const cv::Mat& mat = *it;
			VALIDATE(mat.cols == 2, null_str);
			for (int row = 0; row < mat.rows; row ++) {
				cv::Point pt((int)mat.at<float>(row, 0), (int)mat.at<float>(row, 1));
				if (pt.x < 0 || pt.x >= image.cols) {
					// Although you get a QR code, but some of the four corners may fall outside the image.
					found_outside = true;
					break;
				}
				if (pt.y < 0 || pt.y >= image.rows) {
					found_outside = true;
					break;
				}
				corners->push_back(pt);
			}
			ss << "points[n]:\n" << mat << "\n";
			// 'decoded_info' only take the first one, and the same 'points' has to take the first one.
			break;
		}
		if (found_outside) {
			corners->clear();
			result.clear();
		}
	}
	if (points.size() > 1) {
		int ii = 0;
	}
	// SDL_Log("%s", ss.str().c_str());

	return result;
}


std::string find_barcode(const cv::Mat& image, std::vector<cv::Point>* corners)
{
	VALIDATE(!image.empty(), null_str);

	// const std::string code = find_qr(image, corners);
	// SDL_Log("find_qr => code: %s", code.c_str());

	cv::barcode::BarcodeDetector det;
    std::vector<cv::Point2f> points;
    std::vector<std::string> types;
    std::vector<std::string> lines;
	std::string result;

	bool res = det.detect(image, points);
	if (!res) {
		return null_str;
	}

	VALIDATE(points.size() >0 && points.size() % 4 == 0, null_str);
	if (corners != nullptr) {
		for (int at = 0; at < 4; at ++) {
			const cv::Point2f& point = points[at];
			corners->push_back(cv::Point(point.x, point.y));
		}
	}
	return "12345678";

	// result = det.decode(image, points);

	// before call decode()/decodeWithType(), @points must be valid.
	res = det.decodeWithType(image, points, lines, types);
	if (!res) {
		SDL_Log("{find_barcode}detect barcode, but decode fail");
		return null_str;
	}
	VALIDATE(points.size() == lines.size() * 4, null_str);
	VALIDATE(lines.size() >= 1 && lines.size() == types.size(), null_str);

	const cv::Point2f& point = points[0];
	if (corners != nullptr) {
		corners->push_back(cv::Point(point.x, point.y));
	}

	return lines[0];

    // common interface (single)
    {
        bool res = det.detect(image, points);
		VALIDATE(res, null_str);
        // ASSERT_TRUE(res);
        // EXPECT_EQ(expected_count * 4, points.size());
    }

	{
        result = det.decode(image, points);
        // VALIDATE(!res.empty(), null_str);
        // EXPECT_EQ(1u, expected_lines.count(res));
    }
/*
	// common interface (multi)
    {
        bool res = det.detectMulti(image, points);
		VALIDATE(res, null_str);
        // ASSERT_TRUE(res);
        // EXPECT_EQ(expected_count * 4, points.size());
    }

    {
        bool res = det.decodeMulti(image, points, lines);
		VALIDATE(res, null_str);
        // ASSERT_TRUE(res);
        // EXPECT_EQ(expected_lines, toSet(lines));
    }
*/
    // specific interface
    {
        bool res = det.decodeWithType(image, points, lines, types);
		VALIDATE(res, null_str);

        // ASSERT_TRUE(res);
        // EXPECT_EQ(expected_types, toSet(types));
        // EXPECT_EQ(expected_lines, toSet(lines));
    }

    {
        bool res = det.detectAndDecodeWithType(image, lines, types, points);
		VALIDATE(res, null_str);

        // ASSERT_TRUE(res);
        // EXPECT_EQ(expected_types, toSet(types));
        // EXPECT_EQ(expected_lines, toSet(lines));
    }

	return result;
}