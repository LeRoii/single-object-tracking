#pragma once

#include "tracker_math.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace nanotrack {

struct Options {
  std::string video;
  BBox bbox{};
  std::string model_dir;
  std::string output_video;
  std::string output_boxes;
  std::string controller_config;
  std::string sensor = "visible";
  std::string session_metadata;
  std::string detector_model;
  std::string detector_manifest;
  std::string detector_labels;
  int detector_class_id = -1;
  float detector_confidence = 0.25F;
  float detector_nms = 0.45F;
  std::size_t max_frames = 0;
  std::size_t start_frame = 0;
  std::size_t end_frame = std::numeric_limits<std::size_t>::max();
  int output_width = 0;
  int output_height = 0;
  bool interactive = false;
  bool has_bbox = false;
  bool help = false;
};

Options parse_options(const std::vector<std::string>& arguments);
void validate_input_paths(const Options& options);
std::string usage();

}  // namespace nanotrack
