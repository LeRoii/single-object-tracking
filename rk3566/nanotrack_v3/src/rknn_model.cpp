#include "rknn_model.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace nanotrack {
namespace {

void check_rknn(int status, const std::string& operation) {
  if (status < 0) {
    throw std::runtime_error(operation + " failed with RKNN status " +
                             std::to_string(status));
  }
}

std::vector<std::uint8_t> read_file(const std::string& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) throw std::runtime_error("cannot open RKNN model: " + path);
  const std::streamsize size = stream.tellg();
  if (size <= 0) throw std::runtime_error("RKNN model is empty: " + path);
  stream.seekg(0, std::ios::beg);
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) {
    throw std::runtime_error("cannot read complete RKNN model: " + path);
  }
  return bytes;
}

void print_attr(const char* direction, const rknn_tensor_attr& attr) {
  std::cout << "RKNN_TENSOR direction=" << direction << " index=" << attr.index
            << " name=" << attr.name << " dims=";
  for (std::uint32_t index = 0; index < attr.n_dims; ++index) {
    if (index) std::cout << 'x';
    std::cout << attr.dims[index];
  }
  std::cout << " elems=" << attr.n_elems << " bytes=" << attr.size
            << " fmt=" << attr.fmt << " type=" << attr.type << '\n';
}

}  // namespace

RknnModel::RknnModel(const std::string& path) : model_bytes_(read_file(path)) {
  try {
    check_rknn(rknn_init(&context_, model_bytes_.data(), model_bytes_.size(), 0, nullptr),
               "rknn_init(" + path + ")");

    rknn_sdk_version version{};
    check_rknn(rknn_query(context_, RKNN_QUERY_SDK_VERSION, &version, sizeof(version)),
               "RKNN_QUERY_SDK_VERSION");
    api_version_ = version.api_version;
    driver_version_ = version.drv_version;

    rknn_input_output_num counts{};
    check_rknn(rknn_query(context_, RKNN_QUERY_IN_OUT_NUM, &counts, sizeof(counts)),
               "RKNN_QUERY_IN_OUT_NUM");
    inputs_.resize(counts.n_input);
    outputs_.resize(counts.n_output);
    for (std::uint32_t index = 0; index < counts.n_input; ++index) {
      inputs_[index] = {};
      inputs_[index].index = index;
      check_rknn(rknn_query(context_, RKNN_QUERY_INPUT_ATTR, &inputs_[index],
                            sizeof(rknn_tensor_attr)),
                 "RKNN_QUERY_INPUT_ATTR");
      print_attr("input", inputs_[index]);
    }
    for (std::uint32_t index = 0; index < counts.n_output; ++index) {
      outputs_[index] = {};
      outputs_[index].index = index;
      check_rknn(rknn_query(context_, RKNN_QUERY_OUTPUT_ATTR, &outputs_[index],
                            sizeof(rknn_tensor_attr)),
                 "RKNN_QUERY_OUTPUT_ATTR");
      print_attr("output", outputs_[index]);
    }
    std::cout << "RKNN_MODEL path=" << path << " api=" << api_version_
              << " driver=" << driver_version_ << '\n';
  } catch (...) {
    if (context_ != 0) rknn_destroy(context_);
    context_ = 0;
    throw;
  }
}

RknnModel::~RknnModel() {
  if (context_ != 0) rknn_destroy(context_);
}

std::vector<std::vector<float>> RknnModel::run(
    const std::vector<RknnInput>& inputs) {
  if (inputs.size() != inputs_.size()) {
    throw std::invalid_argument("RKNN input count does not match model metadata");
  }
  std::vector<rknn_input> native_inputs(inputs.size());
  for (std::size_t index = 0; index < inputs.size(); ++index) {
    if (inputs[index].data == nullptr || inputs[index].bytes == 0) {
      throw std::invalid_argument("RKNN input buffer is empty");
    }
    native_inputs[index] = {};
    native_inputs[index].index = static_cast<std::uint32_t>(index);
    native_inputs[index].buf = const_cast<void*>(inputs[index].data);
    native_inputs[index].size = static_cast<std::uint32_t>(inputs[index].bytes);
    native_inputs[index].pass_through = 0;
    native_inputs[index].type = inputs[index].type;
    native_inputs[index].fmt = inputs[index].format;
  }
  check_rknn(rknn_inputs_set(context_, native_inputs.size(), native_inputs.data()),
             "rknn_inputs_set");
  check_rknn(rknn_run(context_, nullptr), "rknn_run");

  std::vector<rknn_output> native_outputs(outputs_.size());
  for (auto& output : native_outputs) {
    output = {};
    output.want_float = 1;
    output.is_prealloc = 0;
  }
  check_rknn(rknn_outputs_get(context_, native_outputs.size(), native_outputs.data(), nullptr),
             "rknn_outputs_get");
  try {
    std::vector<std::vector<float>> result(outputs_.size());
    for (std::size_t index = 0; index < outputs_.size(); ++index) {
      const std::size_t expected_bytes =
          static_cast<std::size_t>(outputs_[index].n_elems) * sizeof(float);
      if (native_outputs[index].buf == nullptr || native_outputs[index].size != expected_bytes) {
        throw std::runtime_error("RKNN float output size does not match tensor metadata");
      }
      const float* begin = static_cast<const float*>(native_outputs[index].buf);
      result[index].assign(begin, begin + outputs_[index].n_elems);
    }
    check_rknn(rknn_outputs_release(context_, native_outputs.size(), native_outputs.data()),
               "rknn_outputs_release");
    return result;
  } catch (...) {
    rknn_outputs_release(context_, native_outputs.size(), native_outputs.data());
    throw;
  }
}

}  // namespace nanotrack

