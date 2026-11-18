#pragma once

#include "detector_adapter.hpp"

#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace nanotrack {

struct DetectorEngineResult {
  std::vector<DetectorCandidate> candidates;
  std::size_t raw_candidate_count = 0;
  std::size_t filtered_candidate_count = 0;
  DetectorTiming timing{};
};

class DetectorEngine {
 public:
  virtual ~DetectorEngine() = default;
  virtual DetectorEngineResult infer(const ImageView& image,
                                     const DetectorRequest& request) = 0;
};

class YoloDetectorAdapter final : public DetectorAdapter {
 public:
  explicit YoloDetectorAdapter(std::unique_ptr<DetectorEngine> engine);
  ~YoloDetectorAdapter() override;

  YoloDetectorAdapter(const YoloDetectorAdapter&) = delete;
  YoloDetectorAdapter& operator=(const YoloDetectorAdapter&) = delete;

  bool available() const override;
  bool can_submit() const override;
  void submit(const ImageView& image,
              const DetectorRequest& request) override;
  std::optional<DetectorBatch> poll() override;
  std::optional<DetectorBlockingResult> detect_blocking(
      const ImageView& image, const DetectorRequest& request) override;

 private:
  friend class YoloDetectorAdapterTestPeer;
  enum class RequestKind { kAsync, kInitialization };
  struct RequestIdentity {
    std::uint64_t request_id = 0;
    RequestKind kind = RequestKind::kAsync;
  };

  struct OwnedWork {
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;
    DetectorRequest request{};
    RequestIdentity identity{};

    ImageView view() const;
  };

  struct CompletedWork {
    RequestIdentity identity{};
    DetectorBatch batch{};
  };
  struct FailedWork {
    RequestIdentity identity{};
    std::exception_ptr error;
  };

  static OwnedWork copy_work(const ImageView& image,
                             const DetectorRequest& request);
  bool can_submit_locked() const;
  RequestIdentity next_identity_locked(RequestKind kind);
  void worker_loop();

  std::unique_ptr<DetectorEngine> engine_;
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  std::optional<OwnedWork> pending_;
  std::optional<CompletedWork> completed_;
  std::optional<FailedWork> pending_error_;
  std::thread worker_;
  std::optional<RequestIdentity> in_flight_;
  std::uint64_t next_request_id_ = 1;
  std::size_t blocking_callers_ = 0;
  bool initialization_exclusive_ = false;
  bool stopping_ = false;
  bool disabled_ = false;
};

}  // namespace nanotrack
