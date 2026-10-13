#include "nanotrack_v3.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace nanotrack {
namespace {

std::size_t tensor_elements(const rknn_tensor_attr& attr) {
  return static_cast<std::size_t>(attr.n_elems);
}

int rounded_positive(float value) {
  if (!std::isfinite(value) || value <= 0.0F) {
    throw std::runtime_error("crop size is not finite and positive");
  }
  return static_cast<int>(std::floor(value + 0.5F));
}

std::vector<float> feature_for_nhwc_input(const std::vector<float>& nchw,
                                          const rknn_tensor_attr& input) {
  if (input.fmt != RKNN_TENSOR_NHWC || input.n_dims != 4 || input.dims[0] != 1) {
    throw std::runtime_error("head feature input must be rank-4 NHWC with batch size one");
  }
  return nchw_to_nhwc(nchw, static_cast<int>(input.dims[3]),
                      static_cast<int>(input.dims[1]),
                      static_cast<int>(input.dims[2]));
}

}  // namespace

NanoTrackV3::NanoTrackV3(const std::string& model_dir) {
  namespace fs = std::filesystem;
  template_model_ = std::make_unique<RknnModel>(
      (fs::path(model_dir) / "template_backbone.rknn").string());
  search_model_ = std::make_unique<RknnModel>(
      (fs::path(model_dir) / "search_backbone.rknn").string());
  head_model_ =
      std::make_unique<RknnModel>((fs::path(model_dir) / "head.rknn").string());
  validate_model_interfaces();
}

void NanoTrackV3::validate_model_interfaces() {
  if (template_model_->inputs().size() != 1 || template_model_->outputs().size() != 1 ||
      search_model_->inputs().size() != 1 || search_model_->outputs().size() != 1 ||
      head_model_->inputs().size() != 2 || head_model_->outputs().size() != 2) {
    throw std::runtime_error("unexpected NanoTrack V3 RKNN input/output counts");
  }
  const std::size_t template_elements = tensor_elements(template_model_->outputs()[0]);
  const std::size_t search_elements = tensor_elements(search_model_->outputs()[0]);
  if (tensor_elements(head_model_->inputs()[0]) != template_elements ||
      tensor_elements(head_model_->inputs()[1]) != search_elements) {
    throw std::runtime_error("backbone feature sizes do not match head input metadata");
  }
  const std::size_t cls_elements = tensor_elements(head_model_->outputs()[0]);
  const std::size_t loc_elements = tensor_elements(head_model_->outputs()[1]);
  if (cls_elements % 2 != 0 || loc_elements != 2 * cls_elements) {
    throw std::runtime_error("head output channel counts are not classification=2 and box=4");
  }
  const std::size_t positions = cls_elements / 2;
  const int output_size = static_cast<int>(std::lround(std::sqrt(positions)));
  if (static_cast<std::size_t>(output_size * output_size) != positions) {
    throw std::runtime_error("head classification output is not a square grid");
  }
  config_.output_size = output_size;
  config_.stride = 16;
  config_.penalty_k = 0.138F;
  config_.window_influence = 0.455F;
  config_.learning_rate = 0.348F;
}

cv::Mat NanoTrackV3::crop(const cv::Mat& frame, float center_x, float center_y,
                          int output_size, int original_size) const {
  const float half = (original_size + 1.0F) / 2.0F;
  int xmin = static_cast<int>(std::floor(center_x - half + 0.5F));
  int ymin = static_cast<int>(std::floor(center_y - half + 0.5F));
  int xmax = xmin + original_size - 1;
  int ymax = ymin + original_size - 1;
  const int left = std::max(0, -xmin);
  const int top = std::max(0, -ymin);
  const int right = std::max(0, xmax - frame.cols + 1);
  const int bottom = std::max(0, ymax - frame.rows + 1);
  xmin += left;
  xmax += left;
  ymin += top;
  ymax += top;

  cv::Mat padded;
  if (left || top || right || bottom) {
    cv::copyMakeBorder(frame, padded, top, bottom, left, right,
                       cv::BORDER_CONSTANT, channel_average_);
  } else {
    padded = frame;
  }
  cv::Mat patch = padded(cv::Rect(xmin, ymin, xmax - xmin + 1, ymax - ymin + 1));
  if (patch.cols != output_size || patch.rows != output_size) {
    cv::resize(patch, patch, cv::Size(output_size, output_size), 0, 0,
               cv::INTER_LINEAR);
  } else {
    patch = patch.clone();
  }
  if (!patch.isContinuous()) patch = patch.clone();
  return patch;
}

void NanoTrackV3::initialize(const cv::Mat& frame, const BBox& bbox) {
  if (frame.empty() || frame.type() != CV_8UC3) {
    throw std::invalid_argument("initial frame must be a non-empty 8-bit BGR image");
  }
  validate_bbox(bbox, frame.cols, frame.rows);
  frame_width_ = frame.cols;
  frame_height_ = frame.rows;
  state_.center_x = bbox.x + (bbox.width - 1.0F) / 2.0F;
  state_.center_y = bbox.y + (bbox.height - 1.0F) / 2.0F;
  state_.width = bbox.width;
  state_.height = bbox.height;
  channel_average_ = cv::mean(frame);
  anchor_feature_ = extract_template_feature(frame, bbox);
  recent_feature_ = anchor_feature_;
  recent_backup_feature_.clear();
  recent_matches_anchor_ = true;
  recent_backup_matches_anchor_ = true;
  recent_staged_ = false;
  search_token_ = 0;
  initialized_ = true;
}

std::vector<float> NanoTrackV3::extract_template_feature(const cv::Mat& frame,
                                                         const BBox& bbox) {
  validate_bbox(bbox, frame.cols, frame.rows);
  const float center_x = bbox.x + (bbox.width - 1.0F) / 2.0F;
  const float center_y = bbox.y + (bbox.height - 1.0F) / 2.0F;
  const float context_width = bbox.width + 0.5F * (bbox.width + bbox.height);
  const float context_height = bbox.height + 0.5F * (bbox.width + bbox.height);
  const int original_size = rounded_positive(std::sqrt(context_width * context_height));
  const cv::Mat patch = crop(frame, center_x, center_y, 127, original_size);
  auto outputs = template_model_->run(
      {{patch.data, patch.total() * patch.elemSize(), RKNN_TENSOR_UINT8,
        RKNN_TENSOR_NHWC}});
  if (outputs.size() != 1 || outputs[0].empty()) {
    throw std::runtime_error("template backbone returned no feature tensor");
  }
  return feature_for_nhwc_input(outputs[0], head_model_->inputs()[0]);
}

DecodeResult NanoTrackV3::run_head(const std::vector<float>& template_feature,
                                   const std::vector<float>& search_feature,
                                   const TrackerState& reference_state,
                                   float scale_z) {
  auto head_outputs = head_model_->run(
      {{template_feature.data(), template_feature.size() * sizeof(float),
        RKNN_TENSOR_FLOAT32, RKNN_TENSOR_NHWC},
       {search_feature.data(), search_feature.size() * sizeof(float),
        RKNN_TENSOR_FLOAT32, RKNN_TENSOR_NHWC}});
  if (head_outputs.size() != 2) {
    throw std::runtime_error("head returned an unexpected output count");
  }
  return decode_head(head_outputs[0], head_outputs[1], reference_state,
                     config_, scale_z, frame_width_, frame_height_);
}

cv::Mat NanoTrackV3::mat_view(const ImageView& image) const {
  if (image.data == nullptr || image.width <= 0 || image.height <= 0 ||
      image.channels != 3 ||
      image.stride_bytes < static_cast<std::size_t>(image.width * 3)) {
    throw std::invalid_argument("NanoTrack image view must be valid BGR8");
  }
  return cv::Mat(image.height, image.width, CV_8UC3,
                 const_cast<std::uint8_t*>(image.data), image.stride_bytes);
}

void NanoTrackV3::initialize(const ImageView& image, const BBox& bbox) {
  initialize(mat_view(image), bbox);
}

std::vector<TrackerObservation> NanoTrackV3::infer(const ImageView& image,
                                                   const SearchPlan& plan) {
  if (!initialized_) throw std::logic_error("tracker has not been initialized");
  const cv::Mat frame = mat_view(image);
  if (frame.empty() || frame.type() != CV_8UC3 || frame.cols != frame_width_ ||
      frame.rows != frame_height_) {
    throw std::invalid_argument("tracking frame must match the initialized BGR frame size");
  }
  validate_bbox(plan.predicted_bbox, frame_width_, frame_height_);
  if (!std::isfinite(plan.search_scale) || plan.search_scale < 1.0F) {
    throw std::invalid_argument("search scale must be finite and at least one");
  }
  TrackerState reference_state{
      plan.predicted_bbox.x + (plan.predicted_bbox.width - 1.0F) / 2.0F,
      plan.predicted_bbox.y + (plan.predicted_bbox.height - 1.0F) / 2.0F,
      plan.predicted_bbox.width,
      plan.predicted_bbox.height};
  const float context_width = reference_state.width +
                              0.5F * (reference_state.width + reference_state.height);
  const float context_height = reference_state.height +
                               0.5F * (reference_state.width + reference_state.height);
  const float size_z = std::sqrt(context_width * context_height);
  const float base_scale_z = 127.0F / size_z;
  const float padding = ((255.0F - 127.0F) / 2.0F) / base_scale_z;
  const int original_size = rounded_positive(
      (size_z + 2.0F * padding) * plan.search_scale);
  const float effective_scale_z = 255.0F / static_cast<float>(original_size);
  const cv::Mat patch = crop(frame, reference_state.center_x,
                             reference_state.center_y, 255, original_size);

  auto search_outputs = search_model_->run(
      {{patch.data, patch.total() * patch.elemSize(), RKNN_TENSOR_UINT8,
        RKNN_TENSOR_NHWC}});
  if (search_outputs.size() != 1 || search_outputs[0].empty()) {
    throw std::runtime_error("search backbone returned no feature tensor");
  }
  const std::vector<float> search_feature =
      feature_for_nhwc_input(search_outputs[0], head_model_->inputs()[1]);
  const DecodeResult anchor = run_head(anchor_feature_, search_feature,
                                       reference_state, effective_scale_z);
  const DecodeResult recent =
      recent_matches_anchor_
          ? anchor
          : run_head(recent_feature_, search_feature, reference_state,
                     effective_scale_z);
  const std::uint64_t token = ++search_token_;
  return {
      TrackerObservation{plan.frame_id, plan.timestamp_ms, anchor.bbox,
                         anchor.metrics, TemplateKind::kAnchor, token},
      TrackerObservation{plan.frame_id, plan.timestamp_ms, recent.bbox,
                         recent.metrics, TemplateKind::kRecent, token}};
}

void NanoTrackV3::commit_bbox(const BBox& bbox) {
  if (!initialized_) throw std::logic_error("tracker has not been initialized");
  validate_bbox(bbox, frame_width_, frame_height_);
  state_.center_x = bbox.x + (bbox.width - 1.0F) / 2.0F;
  state_.center_y = bbox.y + (bbox.height - 1.0F) / 2.0F;
  state_.width = bbox.width;
  state_.height = bbox.height;
}

void NanoTrackV3::stage_recent(const ImageView& image, const BBox& bbox) {
  if (recent_staged_) {
    throw std::logic_error("recent template is already in probation");
  }
  const cv::Mat frame = mat_view(image);
  if (frame.cols != frame_width_ || frame.rows != frame_height_) {
    throw std::invalid_argument("template frame size changed");
  }
  recent_backup_feature_ = recent_feature_;
  recent_backup_matches_anchor_ = recent_matches_anchor_;
  recent_feature_ = extract_template_feature(frame, bbox);
  recent_matches_anchor_ = false;
  recent_staged_ = true;
}

void NanoTrackV3::commit_recent() {
  if (!recent_staged_) throw std::logic_error("no staged recent template to commit");
  recent_backup_feature_.clear();
  recent_staged_ = false;
}

void NanoTrackV3::rollback_recent() {
  if (!recent_staged_) throw std::logic_error("no staged recent template to roll back");
  recent_feature_ = recent_backup_feature_;
  recent_matches_anchor_ = recent_backup_matches_anchor_;
  recent_backup_feature_.clear();
  recent_staged_ = false;
}

}  // namespace nanotrack
