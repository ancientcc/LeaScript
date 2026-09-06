
// #include <ZXing/ReadBarcode.h>
// #include <ZXing/WriteBarcode.h>


#include <zxing-cpp/core/src/WriteBarcode.h>
#include <zxing-cpp/core/src/BarcodeFormat.h>
#include <zxing-cpp/core/src/MultiFormatWriter.h>
#include <zxing-cpp/core/src/BitMatrix.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

#include "rose_sdl_utils.hpp"


// Generate QR code and return cv::Mat
surface generate_qr(const std::string& text, int size, int margin, int eccLevel)
{
    VALIDATE(!text.empty() && size >= 100, null_str);

    surface result;
    try {
        ZXing::MultiFormatWriter writer(ZXing::BarcodeFormat::QRCode);
        writer.setEncoding(ZXing::CharacterSet::UTF8);
        writer.setEccLevel(eccLevel);
        writer.setMargin(margin);

        // Since QR codes are always square, why does writer.encode() method accept two parameters (width and height)?
        // --This is because zxing-cpp's design needs to accommodate multiple barcode formats.
        //   MultiFormatWriter, as a universal encoder, must provide a generic interface. 
        //   However, for the specific QR Code format, its internal implementation ensures that a square matrix is generated.
        ZXing::BitMatrix bitMatrix = writer.encode(text, size, size);
        auto bitmap = ZXing::ToMatrix<uint8_t>(bitMatrix);

        int w1 = bitMatrix.width();
        int h1 = bitMatrix.height();

        int w2 = bitmap.width();
        int h2 = bitmap.height();

		if (bitmap.width() == size && bitmap.height() == size) {
			cv::Mat gray(bitmap.height(), bitmap.width(), CV_8UC1, (uint8_t*)bitmap.data());
			cv::Mat tmp;
			cvtColor(gray, tmp, cv::COLOR_GRAY2BGRA);
			result = clone_surface(tmp);
		}
            
    } catch (const std::exception& e) {
        SDL_Log("QR code Mat generated fail: %s", e.what());
    }
    return result;
}

/*
surface generate_qr_zxing_cpp(const std::string& text, int size, int margin, int eccLevel)
{
    VALIDATE(!text.empty() && size >= 100, null_str);

    int target_width = size;
    int target_height = size;

    // int eccLevel = 2;

    surface result;
    try {
        ZXing::MultiFormatWriter writer(ZXing::BarcodeFormat::QRCode);
        writer.setEncoding(ZXing::CharacterSet::UTF8);
        writer.setEccLevel(eccLevel);
        // set margin to 0 (default is 4 pixels). Ensure the compact QR code is compact with no border.
        writer.setMargin(0);

        // 1. Generate a compact QR code (without margins).
        ZXing::BitMatrix bitMatrix = writer.encode(text, 0, 0);  // Let ZXing automatically select the minimum size.
        
        // 2. Render as a grayscale pixel matrix.
        auto bitmap = ZXing::ToMatrix<uint8_t>(bitMatrix);
        int qr_width = bitmap.width();
        int qr_height = bitmap.height();
        
        // 3. Copy the compact QR code to an OpenCV Mat.
        cv::Mat qr_compact(qr_height, qr_width, CV_8UC1);
        memcpy(qr_compact.data, bitmap.data(), qr_width * qr_height);

        // 4. Calculate the position of the QR code in the final image (centered, with margins on all sides).
        int available_width = target_width - 2 * margin;
        int available_height = target_height - 2 * margin;
        int qr_target_size = std::min(available_width, available_height);
        
        // 5. Scale the QR code to the target size.
        cv::Mat qr_scaled;
        cv::resize(qr_compact, qr_scaled, cv::Size(qr_target_size, qr_target_size), 0, 0, cv::INTER_NEAREST);
        
        // 6. Calculate the centered position.
        int x_offset = (target_width - qr_target_size) / 2;
        int y_offset = (target_height - qr_target_size) / 2;
        
        // 7. Place the scaled QR code in the center of a white background.
        cv::Mat bg(target_height, target_width, CV_8UC1, cv::Scalar(255)); // First, create a white background.
        qr_scaled.copyTo(bg(cv::Rect(x_offset, y_offset, qr_target_size, qr_target_size)));

        cv::Mat tmp;
		cvtColor(bg, tmp, cv::COLOR_GRAY2BGRA);
		result = clone_surface(tmp);
        
    } catch (const std::exception& e) {
        SDL_Log("QR code Mat generated fail: %s", e.what());
    }
    return result;
}
*/
