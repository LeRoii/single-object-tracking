#pragma once

#include "yolo_detector_adapter.hpp"

#include <memory>
#include <string>
#include <vector>

namespace nanotrack {

struct YoloRknnEngineConfig {
  std::string model_path;
  std::string manifest_path;
  std::string labels_path;
  float confidence_threshold = 0.25F;
  float nms_threshold = 0.45F;
  int class_id = -1;
};

class YoloRknnEngine final : public DetectorEngine {
 public:
  explicit YoloRknnEngine(const YoloRknnEngineConfig& config);
  ~YoloRknnEngine() override;

  YoloRknnEngine(const YoloRknnEngine&) = delete;
  YoloRknnEngine& operator=(const YoloRknnEngine&) = delete;

  DetectorEngineResult infer(const ImageView& image,
                             const DetectorRequest& request) override;

  const std::string& model_sha256() const;
  const std::string& manifest_sha256() const;
  const std::string& labels_sha256() const;
  const std::string& api_version() const;
  const std::string& driver_version() const;
  const std::string& input_tensor_description() const;
  const std::vector<std::string>& output_tensor_descriptions() const;
  const std::vector<std::string>& labels() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace nanotrack
