// Copyright 2018 Slightech Co., Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <iomanip>
#include <iostream>
#include <mutex>

#include <opencv2/highgui/highgui.hpp>

#include "mynteyed/camera.h"
#include "mynteyed/utils.h"
#include "mynteyed/util/times.h"

#include "dataset/dataset.h"

MYNTEYE_USE_NAMESPACE

int main(int argc, char const *argv[]) {
  Camera cam;
  DeviceInfo dev_info;
  if (!util::select(cam, &dev_info)) {
    return 1;
  }
  util::print_stream_infos(cam, dev_info.index);

  std::cout << "Open device: " << dev_info.index << ", "
      << dev_info.name << std::endl << std::endl;

  // output file path
  const char *outdir = nullptr;
  if (argc >= 2) {
    outdir = argv[1];
  } else {
    outdir = "./dataset";
  }
  tools::Dataset dataset(outdir);

  OpenParams params(dev_info.index);
    // Color mode: raw(default), rectified
  params.color_mode = MYNTEYE_NAMESPACE::ColorMode::COLOR_RECTIFIED;
  params.depth_mode = MYNTEYE_NAMESPACE::DepthMode::DEPTH_RAW;
  params.stream_mode = MYNTEYE_NAMESPACE::StreamMode::STREAM_1280x480;  // vga
  params.ir_intensity = 0;
  params.framerate = 30;

  // Enable image infos
  cam.EnableImageInfo(true);

  bool is_imu_ok = cam.IsMotionDatasSupported();
  // Enable motion datas until you get them
  if (is_imu_ok) cam.EnableMotionDatas();



    // Callbacks
    std::mutex mutex;
    {
        // Set image info callback
        cam.SetImgInfoCallback([&mutex](const std::shared_ptr<ImgInfo>& info) {
            std::lock_guard<std::mutex> _(mutex);
//            std::cout << " [img_] stamp: "
//                      << std::setiosflags(std::ios::fixed) << std::setprecision(5)
//                      <<  (double)info->timestamp / 100000.0
//                      <<"  [img_info] fid: " << info->frame_id
//                      << ", expos: " << info->exposure_time << std::endl
//                      << std::flush;
        });

        std::vector<ImageType> types{
                ImageType::IMAGE_LEFT_COLOR,
                ImageType::IMAGE_RIGHT_COLOR,
                ImageType::IMAGE_DEPTH,
        };
        for (auto&& type : types) {
            // Set stream data callback
            cam.SetStreamCallback(type,
                    [&mutex,&dataset = dataset](const StreamData& data) {
                std::lock_guard<std::mutex> _(mutex);


                if (data.img) {
                    if (data.img->type() ==  MYNTEYE_NAMESPACE::ImageType::IMAGE_LEFT_COLOR) {
//                             std::cout   << "  [" << data.img->type() << "] fid: "
//                                         << data.img->frame_id() << std::endl
//                                         << std::flush;
                             dataset.SaveStreamData(ImageType::IMAGE_LEFT_COLOR, data);
                    }
//                    else if (data.img->type() ==  MYNTEYE_NAMESPACE::ImageType::IMAGE_RIGHT_COLOR) {
////                             std::cout   << "  [" << data.img->type() << "] fid: "
////                                         << data.img->frame_id() << std::endl
////                                         << std::flush;
//                             dataset.SaveStreamData(ImageType::IMAGE_RIGHT_COLOR, data);
//                    }
                    else if (data.img->type() ==  MYNTEYE_NAMESPACE::ImageType::IMAGE_DEPTH) {
//                             std::cout   << "  [" << data.img->type() << "] fid: "
//                                         << data.img->frame_id() << std::endl
//                                         << std::flush;
                             dataset.SaveStreamData(ImageType::IMAGE_DEPTH, data);
                    }
                }
            });
        }

        // Set motion data callback
        cam.SetMotionCallback([&mutex,&dataset = dataset](const MotionData& data) {
            std::lock_guard<std::mutex> _(mutex);
            dataset.SaveMotionData(data);
//            if (data.imu->flag == MYNTEYE_IMU_ACCEL) {
////        std::cout << "[accel] stamp: " << data.imu->timestamp
////          << ", x: " << data.imu->accel[0]
////          << ", y: " << data.imu->accel[1]
////          << ", z: " << data.imu->accel[2]
////          << ", temp: " << data.imu->temperature
////          << std::endl;
//            } else if (data.imu->flag == MYNTEYE_IMU_GYRO) {
//                std::cout << "[gyro] stamp: "
//                          << std::setiosflags(std::ios::fixed) << std::setprecision(5)
//                          << (double)data.imu->timestamp / 100000.0
//                          << ", x: " << data.imu->gyro[0]
//                          << ", y: " << data.imu->gyro[1]
//                          << ", z: " << data.imu->gyro[2]
//                          << ", temp: " << data.imu->temperature
//                          << std::endl;
//            }
//            std::cout << std::flush;
        });
    }


  cam.Open(params);

  std::cout << std::endl;
  if (!cam.IsOpened()) {
    std::cerr << "Error: Open camera failed" << std::endl;
    return 1;
  }
  std::cout << "Open device success" << std::endl << std::endl;

  std::cout << "Press ESC/Q on Windows to terminate" << std::endl;

  cv::namedWindow("left");
  cv::namedWindow("right");
  cv::namedWindow("depth");


  std::size_t img_count = 0;
  std::size_t accel_count = 0, gyro_count = 0;
  auto &&time_beg = times::now();
  for (;;) {
    auto &&left_color = cam.GetStreamDatas(ImageType::IMAGE_LEFT_COLOR);
    //auto &&right_color = cam.GetStreamDatas(ImageType::IMAGE_RIGHT_COLOR);
    auto &&depth_color = cam.GetStreamDatas(ImageType::IMAGE_DEPTH);

      img_count += left_color.size();

//      if (!left_color.empty() && !depth_color.empty()) {
////      auto &&left = left_color.back();
////      cv::Mat image_left =
////        left.img->To(ImageFormat::COLOR_BGR)->ToMat();
//          //cv::imshow("left", image_left);
//          for (auto &&left : left_color) {
//              dataset.SaveStreamData(ImageType::IMAGE_LEFT_COLOR, left);
//          }
//
//          for (auto &&depth : depth_color) {
//              dataset.SaveStreamData(ImageType::IMAGE_DEPTH, depth);
//          }
//      }

//    if (!left_color.empty()) {
////      auto &&left = left_color.back();
////      cv::Mat image_left =
////        left.img->To(ImageFormat::COLOR_BGR)->ToMat();
//      //cv::imshow("left", image_left);
//      for (auto &&left : left_color) {
//        dataset.SaveStreamData(ImageType::IMAGE_LEFT_COLOR, left);
//      }
//    }

//    if (!right_color.empty()) {
////      auto &&right = right_color.back();
////      cv::Mat image_right =
////        right.img->To(ImageFormat::COLOR_BGR)->ToMat();
//      //cv::imshow("right", image_right);
//      for (auto &&right : right_color) {
//        dataset.SaveStreamData(ImageType::IMAGE_RIGHT_COLOR, right);
//      }
//    }

//      if (!depth_color.empty()) {
////          auto &&depth = depth_color.back();
////          cv::Mat image_depth =
////                  depth.img->To(ImageFormat::DEPTH_RAW)->ToMat();
//
//          //cv::imshow("depth", image_depth);
//          for (auto &&depth : depth_color) {
//              dataset.SaveStreamData(ImageType::IMAGE_DEPTH, depth);
//          }
//      }
//
//    if (is_imu_ok) {
//      auto &&motion_data = cam.GetMotionDatas();
//
//      for (auto &&motion : motion_data) {
//        if (!motion.imu) continue;
//        if (motion.imu->flag == MYNTEYE_IMU_ACCEL) {
//          ++accel_count;
//        } else if (motion.imu->flag == MYNTEYE_IMU_GYRO) {
//          ++gyro_count;
//        } else {
//          continue;
//        }
//        dataset.SaveMotionData(motion);
//      }
//    }
//
//    std::cout << "\rSaved " << img_count << " imgs";
//    if (is_imu_ok) {
//      std::cout << ", " << accel_count << " accels"
//          << ", " << gyro_count << " gyros";
//    }
//    std::cout << std::flush;
//
    char key = static_cast<char>(cv::waitKey(1));
    if (key == 27 || key == 'q' || key == 'Q') {  // ESC/Q
      break;
    }
  }
  std::cout << " to " << outdir << std::endl;
  auto &&time_end = times::now();

  cam.Close();

  float elapsed_ms =
      times::count<times::microseconds>(time_end - time_beg) *
      0.001f;
  std::cout << "Time beg: " << times::to_local_string(time_beg)
    << ", end: " << times::to_local_string(time_end)
    << ", cost: " << elapsed_ms << "ms" << std::endl;
  std::cout << "Img count: " << img_count
    << ", fps: " << (1000.f * img_count / elapsed_ms) << std::endl;
  if (is_imu_ok) {
    std::cout << "Accel count: " << accel_count
      << ", hz: " << (1000.f * accel_count / elapsed_ms) << std::endl;
    std::cout << "Gryo count: " << gyro_count
      << ", hz: " << (1000.f * gyro_count / elapsed_ms) << std::endl;
  }

  cv::destroyAllWindows();
  return 0;
}
