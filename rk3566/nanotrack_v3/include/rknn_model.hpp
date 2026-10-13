#pragma once

#include <rknn_api.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nanotrack {

struct RknnInput {
  const void* data;
  std::size_t bytes;
  rknn_tensor_type type;
  rknn_tensor_format format;
};

class RknnModel {
 public:
  explicit RknnModel(const std::string& path);
  ~RknnModel();
  RknnModel(const RknnModel&) = delete;
  RknnModel& operator=(const RknnModel&) = delete;

  std::vector<std::vector<float>> run(const std::vector<RknnInput>& inputs);
  const std::vector<rknn_tensor_attr>& inputs() const { return inputs_; }
  const std::vector<rknn_tensor_attr>& outputs() const { return outputs_; }
  const std::string& api_version() const { return api_version_; }
  const std::string& driver_version() const { return driver_version_; }

 private:
  rknn_context context_ = 0;
  std::vector<std::uint8_t> model_bytes_;
  std::vector<rknn_tensor_attr> inputs_;
  std::vector<rknn_tensor_attr> outputs_;
  std::string api_version_;
  std::string driver_version_;
};

}  // namespace nanotrack

