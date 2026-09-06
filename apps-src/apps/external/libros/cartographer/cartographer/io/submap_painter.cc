/*
 * Copyright 2017 The Cartographer Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cartographer/io/submap_painter.h"

#include "cartographer/mapping/2d/submap_2d.h"
#include "cartographer/mapping/3d/submap_3d.h"

namespace cartographer {
namespace io {
namespace {

Eigen::Affine3d ToEigen(const ::cartographer::transform::Rigid3d& rigid3) {
  return Eigen::Translation3d(rigid3.translation()) * rigid3.rotation();
}

void CairoPaintSubmapSlices(
    const double scale,
    const std::map<::cartographer::mapping::SubmapId, SubmapSlice>& submaps,
    cairo_t* cr, std::function<void(const SubmapSlice&)> draw_callback) {
  cairo_scale(cr, scale, scale);

  const int s = submaps.size();
  int at = 0;
  for (auto& pair : submaps) {
    const auto& submap_slice = pair.second;
    if (submap_slice.surface == nullptr) {
      return;
    }
    const Eigen::Matrix4d homo =
        ToEigen(submap_slice.pose * submap_slice.slice_pose).matrix();

    cairo_save(cr);
    cairo_matrix_t matrix;
    cairo_matrix_init(&matrix, homo(1, 0), homo(0, 0), -homo(1, 1), -homo(0, 1),
                      homo(0, 3), -homo(1, 3));
    cairo_transform(cr, &matrix);

    const double submap_resolution = submap_slice.resolution;
    cairo_scale(cr, submap_resolution, submap_resolution);

    // Invokes caller's callback to utilize slice data in global cooridnate
    // frame. e.g. finds bounding box, paints slices.
    if (cartographer_sdl_log) {
      SDL_Log("CairoPaintSubmapSlices[%i/%i]sid:%i, ver: %i, (%ix%i) %s * %s = %s, (%.5f, %.5f, %.5f, %.5f, %.5f, %.5f)",
        at, s - 1, pair.first.submap_index, submap_slice.version, submap_slice.width, submap_slice.height, 
        submap_slice.pose.DebugString().c_str(),
        submap_slice.slice_pose.DebugString().c_str(),
        (submap_slice.pose * submap_slice.slice_pose).DebugString().c_str(),
        homo(1, 0), homo(0, 0), -homo(1, 1), -homo(0, 1), homo(0, 3), -homo(1, 3));
    }
    draw_callback(submap_slice);
    cairo_restore(cr);
    at ++;
  }
}

void CairoPaintSubmapSlices2(
    const double scale,
    const std::map<::cartographer::mapping::SubmapId, SubmapSlice>& submaps,
    cairo_t* cr, std::function<void(int sid, const SubmapSlice&)> draw_callback) {
  cairo_scale(cr, scale, scale);

  const int s = submaps.size();
  int at = 0;
  for (auto& pair : submaps) {
    const auto& submap_slice = pair.second;
    if (submap_slice.surface == nullptr) {
      // return;
    }
    const Eigen::Matrix4d homo =
        ToEigen(submap_slice.pose * submap_slice.slice_pose).matrix();

    cairo_save(cr);
    cairo_matrix_t matrix;
    cairo_matrix_init(&matrix, homo(1, 0), homo(0, 0), -homo(1, 1), -homo(0, 1),
                      homo(0, 3), -homo(1, 3));
    cairo_transform(cr, &matrix);

    const double submap_resolution = submap_slice.resolution;
    cairo_scale(cr, submap_resolution, submap_resolution);

    // Invokes caller's callback to utilize slice data in global cooridnate
    // frame. e.g. finds bounding box, paints slices.
    SDL_Log("CairoPaintSubmapSlices[%i/%i]sid:%i, ver: %i, (%ix%i) %s * %s = %s, (%.5f, %.5f, %.5f, %.5f, %.5f, %.5f)",
        at, s - 1, pair.first.submap_index, submap_slice.version, submap_slice.width, submap_slice.height, 
        submap_slice.pose.DebugString().c_str(),
        submap_slice.slice_pose.DebugString().c_str(),
        (submap_slice.pose * submap_slice.slice_pose).DebugString().c_str(),
        homo(1, 0), homo(0, 0), -homo(1, 1), -homo(0, 1), homo(0, 3), -homo(1, 3));
    draw_callback(pair.first.submap_index, submap_slice);
    cairo_restore(cr);
    at ++;
  }
}

bool Has2DGrid(const mapping::proto::Submap& submap) {
  return submap.has_submap_2d() && submap.submap_2d().has_grid();
}

bool Has3DGrids(const mapping::proto::Submap& submap) {
  return submap.has_submap_3d() &&
         submap.submap_3d().has_low_resolution_hybrid_grid() &&
         submap.submap_3d().has_high_resolution_hybrid_grid();
}

}  // namespace

PaintSubmapSlicesResult PaintSubmapSlices(
    const std::map<::cartographer::mapping::SubmapId, SubmapSlice>& submaps,
    const double resolution) {
  Eigen::AlignedBox2f bounding_box;
  {
    auto surface = MakeUniqueCairoSurfacePtr(
        cairo_image_surface_create(kCairoFormat, 1, 1));
    auto cr = MakeUniqueCairoPtr(cairo_create(surface.get()));
    const auto update_bounding_box = [&bounding_box, &cr](double x, double y) {
      cairo_user_to_device(cr.get(), &x, &y);
      bounding_box.extend(Eigen::Vector2f(x, y));
    };

    CairoPaintSubmapSlices(
        1. / resolution, submaps, cr.get(),
        [&update_bounding_box](const SubmapSlice& submap_slice) {
          update_bounding_box(0, 0);
          update_bounding_box(submap_slice.width, 0);
          update_bounding_box(0, submap_slice.height);
          update_bounding_box(submap_slice.width, submap_slice.height);
        });
  }

  const int kPaddingPixel = 5;
  const Eigen::Array2i size(
      std::ceil(bounding_box.sizes().x()) + 2 * kPaddingPixel,
      std::ceil(bounding_box.sizes().y()) + 2 * kPaddingPixel);
  const Eigen::Array2f origin(-bounding_box.min().x() + kPaddingPixel,
                              -bounding_box.min().y() + kPaddingPixel);

  if (cartographer_sdl_log) {
    SDL_Log("PaintSubmapSlices, size:(%i x %i)[%.5f x %.5f], origin:(%.5f, %.5f)[%.5f x %.5f]", 
      size[0], size[1], std::ceil(bounding_box.sizes().x()), std::ceil(bounding_box.sizes().y()),
      origin[0], origin[1], -bounding_box.min().x(), -bounding_box.min().y());
  }

  auto surface = MakeUniqueCairoSurfacePtr(
      cairo_image_surface_create(kCairoFormat, size.x(), size.y()));
  {
    auto cr = MakeUniqueCairoPtr(cairo_create(surface.get()));
    cairo_set_source_rgba(cr.get(), 0.5, 0.0, 0.0, 1.);
    cairo_paint(cr.get());
    cairo_translate(cr.get(), origin.x(), origin.y());
    CairoPaintSubmapSlices(1. / resolution, submaps, cr.get(),
                           [&cr](const SubmapSlice& submap_slice) {
                             cairo_set_source_surface(
                                 cr.get(), submap_slice.surface.get(), 0., 0.);
                             cairo_paint(cr.get());
                           });
    cairo_surface_flush(surface.get());
  }

  bool verbose = false;
  // cairo_surface_write_to_png(surface.get(), "c:/ddksample/rectangle.png");

  return PaintSubmapSlicesResult(std::move(surface), origin);
}

#define DEG2RAD(x) ((x)*M_PI / 180.)

void PaintSubmapSlices2() {
  std::map<::cartographer::mapping::SubmapId, SubmapSlice> submaps;
  double resolution = 0.05;
  SubmapSlice& submap_slice0 = submaps[::cartographer::mapping::SubmapId(0, 0)];
  SubmapSlice& submap_slice1 = submaps[::cartographer::mapping::SubmapId(0, 1)];
  SubmapSlice& submap_slice2 = submaps[::cartographer::mapping::SubmapId(0, 2)];
  SubmapSlice& submap_slice3 = submaps[::cartographer::mapping::SubmapId(0, 3)];

  // [0/3](204x76){ t: [1.9, 4.85, 0], q: [0] }
  submap_slice0.width = 204;
  submap_slice0.height = 76;
  submap_slice0.resolution = 0.05;
  submap_slice0.version = 70;
  submap_slice0.pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0, 0, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  submap_slice0.slice_pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(1.9, 4.85, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  // submap_slice0.slice_pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0, 0, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  // [1/3](205x104){ t: [2.76015, 4.87334, 0], q: [-0.825494] }
  submap_slice1.width = 205;
  submap_slice1.height = 104;
  submap_slice1.resolution = 0.05;
  submap_slice1.version = 70;
  submap_slice1.pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0.636171, -0.246591, 0), 
      Eigen::AngleAxisd(DEG2RAD(-0.825494), Eigen::Vector3d(0, 0, 1)));
      // Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  submap_slice1.slice_pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(2.05, 5.15, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1))); 
  // [2/3](204x103){ t: [2.63235, 4.93761, 0], q: [0.057486] }
  submap_slice2.width = 204;
  submap_slice2.height = 103;
  submap_slice2.resolution = 0.05;
  submap_slice2.version = 70;
  submap_slice2.pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0.638476, -1.16439, 0), 
      Eigen::AngleAxisd(DEG2RAD(0.057486), Eigen::Vector3d(0, 0, 1)));
      // Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  submap_slice2.slice_pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(2, 6.1, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1))); 
  // [3/3](203x97){ t: [2.48882, 4.93342, 0], q: [0.0481507] }
/*
  submap_slice3.width = 203;
  submap_slice3.height = 97;
  submap_slice3.resolution = 0.05;
  submap_slice3.version = 70;
  submap_slice3.pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0.14437, -1.66856, 0), 
      Eigen::AngleAxisd(DEG2RAD(0.0481507), Eigen::Vector3d(0, 0, 1)));
      // Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  submap_slice3.slice_pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(2.35, 6.6, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1))); 
*/

  submap_slice3.width = 203;
  submap_slice3.height = 97;
  submap_slice3.resolution = 0.05;
  submap_slice3.version = 70;
  submap_slice3.pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0.15, -1.75, 0), 
      // Eigen::AngleAxisd(DEG2RAD(0.0481507), Eigen::Vector3d(0, 0, 1)));
      Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  submap_slice3.slice_pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(2.35, 6.6, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1))); 

/*
  submap_slice3.width = 203;
  submap_slice3.height = 97;
  submap_slice3.resolution = 0.05;
  submap_slice3.version = 70;
  submap_slice3.pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0, 0, 0), 
      // Eigen::AngleAxisd(DEG2RAD(0.0481507), Eigen::Vector3d(0, 0, 1)));
      Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
  submap_slice3.slice_pose = ::cartographer::transform::Rigid3d(Eigen::Vector3d(0, 0, 0), Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1))); 
*/
  Eigen::AlignedBox2f bounding_box;
  {
    auto surface = MakeUniqueCairoSurfacePtr(
        cairo_image_surface_create(kCairoFormat, 1, 1));
    auto cr = MakeUniqueCairoPtr(cairo_create(surface.get()));
    const auto update_bounding_box = [&bounding_box, &cr](int sid, double x, double y) {
      double x1 = x;
      double y1 = y;
      cairo_user_to_device(cr.get(), &x, &y);
      SDL_Log("sid: %i, (%.5f, %.5f) --> (%.5f, %.5f)", sid, x1, y1, x, y);
      bounding_box.extend(Eigen::Vector2f(x, y));
    };

    CairoPaintSubmapSlices2(
        1. / resolution, submaps, cr.get(),
        [&bounding_box, &update_bounding_box](int sid, const SubmapSlice& submap_slice) {
          update_bounding_box(sid, 0, 0);
          update_bounding_box(sid, submap_slice.width, 0);
          update_bounding_box(sid, 0, submap_slice.height);
          update_bounding_box(sid, submap_slice.width, submap_slice.height);

          const int kPaddingPixel = 5;
          const Eigen::Array2i size(
          std::ceil(bounding_box.sizes().x()) + 2 * kPaddingPixel,
          std::ceil(bounding_box.sizes().y()) + 2 * kPaddingPixel);
          const Eigen::Array2f origin(-bounding_box.min().x() + kPaddingPixel,
                              -bounding_box.min().y() + kPaddingPixel);

          SDL_Log("PaintSubmapSlices2[sid#%i]size:(%i x %i)[%.5f x %.5f], origin:(%.5f, %.5f)[%.5f x %.5f]", 
            sid, size[0], size[1], std::ceil(bounding_box.sizes().x()), std::ceil(bounding_box.sizes().y()),
            origin[0], origin[1], -bounding_box.min().x(), -bounding_box.min().y());
        });
  }

  const int kPaddingPixel = 5;
  const Eigen::Array2i size(
      std::ceil(bounding_box.sizes().x()) + 2 * kPaddingPixel,
      std::ceil(bounding_box.sizes().y()) + 2 * kPaddingPixel);
  const Eigen::Array2f origin(-bounding_box.min().x() + kPaddingPixel,
                              -bounding_box.min().y() + kPaddingPixel);

  SDL_Log("PaintSubmapSlices2, size:(%i x %i)[%.5f x %.5f], origin:(%.5f, %.5f)[%.5f x %.5f]", 
      size[0], size[1], std::ceil(bounding_box.sizes().x()), std::ceil(bounding_box.sizes().y()),
      origin[0], origin[1], -bounding_box.min().x(), -bounding_box.min().y());
  int ii = 0;
}

void FillSubmapSlice(
    const ::cartographer::transform::Rigid3d& global_submap_pose,
    const ::cartographer::mapping::proto::Submap& proto,
    SubmapSlice* const submap_slice,
    mapping::ValueConversionTables* conversion_tables) {
  ::cartographer::mapping::proto::SubmapQuery::Response response;
  ::cartographer::transform::Rigid3d local_pose;
  if (proto.has_submap_3d()) {
    mapping::Submap3D submap(proto.submap_3d());
    local_pose = submap.local_pose();
    submap.ToResponseProto(global_submap_pose, &response);
  } else {
    ::cartographer::mapping::Submap2D submap(proto.submap_2d(),
                                             conversion_tables);
    local_pose = submap.local_pose();
    submap.ToResponseProto(global_submap_pose, &response);
  }
  submap_slice->pose = global_submap_pose;

  auto& texture_proto = response.textures(0);
  const SubmapTexture::Pixels pixels = UnpackTextureData(
      texture_proto.cells(), texture_proto.width(), texture_proto.height());
  submap_slice->width = texture_proto.width();
  submap_slice->height = texture_proto.height();
  submap_slice->resolution = texture_proto.resolution();
  submap_slice->slice_pose =
      ::cartographer::transform::ToRigid3(texture_proto.slice_pose());
  submap_slice->surface =
      DrawTexture(pixels.intensity, pixels.alpha, texture_proto.width(),
                  texture_proto.height(), &submap_slice->cairo_data);
}

void DeserializeAndFillSubmapSlices(
    ProtoStreamDeserializer* deserializer,
    std::map<mapping::SubmapId, SubmapSlice>* submap_slices,
    mapping::ValueConversionTables* conversion_tables) {
  std::map<mapping::SubmapId, transform::Rigid3d> submap_poses;
  for (const auto& trajectory : deserializer->pose_graph().trajectory()) {
    for (const auto& submap : trajectory.submap()) {
      submap_poses[mapping::SubmapId(trajectory.trajectory_id(),
                                     submap.submap_index())] =
          transform::ToRigid3(submap.pose());
    }
  }
  mapping::proto::SerializedData proto;
  while (deserializer->ReadNextSerializedData(&proto)) {
    if (proto.has_submap() &&
        (Has2DGrid(proto.submap()) || Has3DGrids(proto.submap()))) {
      const auto& submap = proto.submap();
      const mapping::SubmapId id{submap.submap_id().trajectory_id(),
                                 submap.submap_id().submap_index()};
      FillSubmapSlice(submap_poses.at(id), submap, &(*submap_slices)[id],
                      conversion_tables);
    }
  }
}

SubmapTexture::Pixels UnpackTextureData(const std::string& compressed_cells,
                                        const int width, const int height) {
  SubmapTexture::Pixels pixels;
  std::string cells;
  ::cartographer::common::FastGunzipString(compressed_cells, &cells);
  const int num_pixels = width * height;
  CHECK_EQ(cells.size(), 2 * num_pixels);
  pixels.intensity.reserve(num_pixels);
  pixels.alpha.reserve(num_pixels);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      pixels.intensity.push_back(cells[(i * width + j) * 2]);
      pixels.alpha.push_back(cells[(i * width + j) * 2 + 1]);
    }
  }
  return pixels;
}

UniqueCairoSurfacePtr DrawTexture(const std::vector<char>& intensity,
                                  const std::vector<char>& alpha,
                                  const int width, const int height,
                                  std::vector<uint32_t>* const cairo_data) {
  CHECK(cairo_data->empty());

  // Properly dealing with a non-common stride would make this code much more
  // complicated. Let's check that it is not needed.
  const int expected_stride = 4 * width;
  CHECK_EQ(expected_stride, cairo_format_stride_for_width(kCairoFormat, width));
  for (size_t i = 0; i < intensity.size(); ++i) {
    // We use the red channel to track intensity information. The green
    // channel we use to track if a cell was ever observed.
    const uint8_t intensity_value = intensity.at(i);
    const uint8_t alpha_value = alpha.at(i);
    const uint8_t observed =
        (intensity_value == 0 && alpha_value == 0) ? 0 : 255;
    cairo_data->push_back((alpha_value << 24) | (intensity_value << 16) |
                          (observed << 8) | 0);
  }

  auto surface = MakeUniqueCairoSurfacePtr(cairo_image_surface_create_for_data(
      reinterpret_cast<unsigned char*>(cairo_data->data()), kCairoFormat, width,
      height, expected_stride));
  CHECK_EQ(cairo_surface_status(surface.get()), CAIRO_STATUS_SUCCESS)
      << cairo_status_to_string(cairo_surface_status(surface.get()));
  return surface;
}

}  // namespace io
}  // namespace cartographer
