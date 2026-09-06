/*****
Copyright @ 2017-2025, Hang Zhou Xiao Kong Cheng Xiang Technology Co.,Ltd, All
Rigihts Reserved
*****/

#ifndef FACE_IMAGE_UTILS_H
#define FACE_IMAGE_UTILS_H

#include "dface/common.h"
#include <stdint.h>
#include <stdlib.h>
#include <vector>

#define DF_COLOR_RGB 1

/**
 * @brief Create an image.
 * @param width The width of the image.
 * @param height The height of the image.
 * @param format The format of the image.
 * @param is_malloc If true, allocate memory for the image.
 * @return A new image.
 */
API_EXPORTS df_image_t df_create_image(int width, int height,
                                       df_img_format_t format, int is_malloc);

/*
 * @brief Create an empty image.
 * @return A new empty image.
 */
API_EXPORTS df_image_t df_create_empty_image();

/*
 * @brief Free an image.
 * @param img The image to free.
 */
API_EXPORTS void df_free_image(df_image_t &img);

/*
 * @brief Free a vector of images.
 * @param images The vector of images to free.
 */
API_EXPORTS void df_free_images(std::vector<df_image_t> &images);

/**
 * @brief Clone an image.
 * @param source The source image to clone.
 * @return A new image that is a clone of the source.
 */
API_EXPORTS df_image_t df_clone_image(const df_image_t &source);

/**
 * @brief Copy an image.
 * @param source The source image to copy from.
 * @param dest The destination image to copy to.
 */
API_EXPORTS void df_copy_image(const df_image_t &source, df_image_t &dest);

/**
 * @brief Crop an image.
 * @param original_image The original image to crop from.
 * @param x The x-coordinate of the top-left corner of the crop region.
 * @param y The y-coordinate of the top-left corner of the crop region.
 * @param cropped_width The width of the crop region.
 * @param cropped_height The height of the crop region.
 * @param cropped_image The cropped image output.
 */
API_EXPORTS void df_crop_image(const df_image_t &original_image, int x, int y,
                               int cropped_width, int cropped_height,
                               df_image_t &cropped_image);

/**
 * @brief Convert an RGB image to BGR format.
 * @param rgb_image The source RGB image.
 * @param bgr_image The destination BGR image.
 */
API_EXPORTS void df_rgb2bgr(const df_image_t &rgb_image, df_image_t &bgr_image);

/**
 * @brief Convert a BGR image to RGB format.
 * @param bgr_image The source BGR image.
 * @param rgb_image The destination RGB image.
 */
API_EXPORTS void df_bgr2rgb(const df_image_t &bgr_image, df_image_t &rgb_image);

/**
 * @brief Add a border around an image.
 * @param src The source image.
 * @param top The size of the top border.
 * @param bottom The size of the bottom border.
 * @param left The size of the left border.
 * @param right The size of the right border.
 * @return A new image with the specified border.
 */
API_EXPORTS df_image_t df_copy_make_border(const df_image_t &src, int top,
                                           int bottom, int left, int right);

#endif