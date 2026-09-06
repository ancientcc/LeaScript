//______________________________________________________________________________________
// Program : OpenCV based QR code Detection and Retrieval
// Author  : Bharath Prabhuswamy
// Github  : https://github.com/bharathp666/opencv_qr
//______________________________________________________________________________________


#include "rose_global.hpp"

#include <iostream>
#include <cmath>
#include "sdl_utils.hpp"
#include "wml_exception.hpp"
#include "qr_code.hpp"

#include "rose_config.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
// #include "filesystem.hpp"


#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
/*
#include "zxing/BarcodeFormat.h"
#include "zxing/MultiFormatWriter.h"
#include "zxing/TextUtfEncoding.h"
#include "zxing/BitMatrix.h"
#include "zxing/ByteMatrix.h"

surface generate_qr(const std::string& text, int size)
{
	VALIDATE(!text.empty() && size >= 100, null_str);

	surface result;
	int margin = 10;
	int eccLevel = 2; // -1
	std::string format = "QR_CODE";

	try {
		auto barcodeFormat = ZXing::BarcodeFormatFromString(format);
		VALIDATE(barcodeFormat != ZXing::BarcodeFormat::FORMAT_COUNT, null_str);

		ZXing::MultiFormatWriter writer(barcodeFormat);
		if (margin >= 0) {
			writer.setMargin(margin);
		}
		if (eccLevel >= 0) {
			writer.setEccLevel(eccLevel);
		}

		auto bitmap = writer.encode(ZXing::TextUtfEncoding::FromUtf8(text), size, size).toByteMatrix();
		if (bitmap.width() == size && bitmap.height() == size) {
			cv::Mat gray(bitmap.height(), bitmap.width(), CV_8UC1, (uint8_t*)bitmap.data());
			cv::Mat tmp;
			cvtColor(gray, tmp, cv::COLOR_GRAY2BGRA);
			result = clone_surface(tmp);
		}

	} catch (const std::exception& e) {
		SDL_Log("generate_qr: %s", e.what());
	}
	return result;
}
*/