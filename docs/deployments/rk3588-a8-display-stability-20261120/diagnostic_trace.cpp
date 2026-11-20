// Link-time observation only: neither callback results nor UART bytes are changed.
#include "native-lib.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <thread>
#include <vector>

extern "C" bool __real_tracker_detector_init(AI_TRACK_INFO*);
extern "C" bool __real_do_tracker_detector(unsigned char*, AI_TRACK_INFO*, RECT_SIZE*);
extern "C" int __real_write_serial(int, unsigned char*, int);

namespace {
using Clock = std::chrono::steady_clock;
constexpr int kImageWindowMs = 8000;
constexpr int kMaxFramesPerEpisode = 240;
constexpr std::size_t kImageBudgetBytes = 1024U * 1024U * 1024U;
long long now_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      Clock::now().time_since_epoch()).count();
}
struct ErrnoGuard { int value = errno; ~ErrnoGuard() { errno = value; } };
struct ImageJob {
  unsigned long long call = 0, episode = 0;
  long long begin = 0, end = 0;
  int index = 0, crop_x = 0, crop_y = 0, input_w = 0, input_h = 0;
  cv::Mat overview, crop;
};

class Trace {
 public:
  Trace() {
    const char* path = std::getenv("AI_DEBUG_DIR");
    if (!path || !*path) return;
    root_ = path;
    fd_ = open((root_ + "/events.log").c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd_ < 0) {
      std::fprintf(stderr, "AI_DEBUG disabled: cannot create events.log errno=%d\n", errno);
      return;
    }
    images_ = std::getenv("AI_DEBUG_IMAGES") == nullptr ||
              std::strcmp(std::getenv("AI_DEBUG_IMAGES"), "0") != 0;
    if (mkdir((root_ + "/images").c_str(), 0755) != 0 && errno != EEXIST) images_ = false;
    emit("DEBUG_READY mono_us=%lld images=%d window_ms=%d max_frames=%d max_episodes=6 "
         "queue_max=4 image_budget_bytes=%zu clock=CLOCK_MONOTONIC\n",
         now_us(), images_, kImageWindowMs, kMaxFramesPerEpisode, kImageBudgetBytes);
    if (images_) worker_ = std::thread([this] { work(); });
  }
  ~Trace() {
    { std::lock_guard<std::mutex> lock(queue_mutex_); stopping_ = true; }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    if (fd_ >= 0) close(fd_);
  }
  bool enabled() const { return fd_ >= 0; }
  template <class... Args> void emit(const char* fmt, Args... args) noexcept {
    ErrnoGuard guard;
    if (fd_ < 0) return;
    char line[4096];
    const int n = std::snprintf(line, sizeof(line), fmt, args...);
    if (n <= 0) return;
    const std::size_t len = std::min<std::size_t>(n, sizeof(line) - 1);
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_failed_) return;
    if (log_bytes_ + len > 64U * 1024U * 1024U) {
      log_failed_ = true;
      std::fprintf(stderr, "AI_DEBUG events limit reached; further events omitted\n");
      return;
    }
    std::size_t done = 0;
    while (done < len) {
      const ssize_t wrote = write(fd_, line + done, len - done);
      if (wrote < 0 && errno == EINTR) continue;
      if (wrote <= 0) {
        log_failed_ = true;
        std::fprintf(stderr, "AI_DEBUG event write failed errno=%d\n", errno);
        return;
      }
      done += static_cast<std::size_t>(wrote);
    }
    log_bytes_ += done;
  }
  void capture(unsigned char* rgb, const AI_TRACK_INFO& info, const RECT_SIZE& display,
               unsigned long long call, unsigned long long episode, int index,
               long long begin, long long end, long long episode_begin) noexcept {
    ErrnoGuard guard;
    if (!images_ || !rgb || episode > 6 || index >= kMaxFramesPerEpisode ||
        begin - episode_begin > static_cast<long long>(kImageWindowMs) * 1000) return;
    try {
      // All pixels are copied before returning the caller-owned buffer. No asynchronous reads of it.
      std::unique_lock<std::mutex> lock(queue_mutex_, std::try_to_lock);
      if (!lock.owns_lock()) { emit("IMAGE_DROP call=%llu reason=queue_busy\n", call); return; }
      if (queue_.size() >= 4) { emit("IMAGE_DROP call=%llu reason=queue_full\n", call); return; }
      const int w = info.rgb_img_size.width, h = info.rgb_img_size.height;
      if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || info.is_ir_model) return;
      const int ow = std::min(w, 960), oh = std::max(1, h * ow / w);
      const int cw = std::min(w, 512), ch = std::min(h, 512);
      const std::size_t cost = static_cast<std::size_t>(ow) * (oh + 48) +
                               static_cast<std::size_t>(cw) * (ch + 48) + 128;
      if (image_bytes_ + cost > kImageBudgetBytes) {
        emit("IMAGE_DROP call=%llu reason=image_budget\n", call); return;
      }
      ImageJob job;
      job.call = call; job.episode = episode; job.index = index;
      job.begin = begin; job.end = end; job.input_w = w; job.input_h = h;
      const RECT_SIZE& box = display.width > 0 && display.height > 0 ? display : info.init_track_box;
      job.crop_x = std::clamp(box.x + box.width / 2 - cw / 2, 0, w - cw);
      job.crop_y = std::clamp(box.y + box.height / 2 - ch / 2, 0, h - ch);
      job.overview.create(oh, ow, CV_8UC1);
      job.crop.create(ch, cw, CV_8UC1);
      // Nearest-grid full-scene sample; full-resolution crop, both grayscale RGB luminance.
      const auto gray = [](const unsigned char* p) {
        return static_cast<unsigned char>((77U * p[0] + 150U * p[1] + 29U * p[2]) >> 8);
      };
      for (int y = 0; y < oh; ++y) {
        auto* dst = job.overview.ptr<unsigned char>(y);
        const auto* src = rgb + static_cast<std::size_t>(y * h / oh) * w * 3;
        for (int x = 0; x < ow; ++x) dst[x] = gray(src + (x * w / ow) * 3);
      }
      for (int y = 0; y < ch; ++y) {
        auto* dst = job.crop.ptr<unsigned char>(y);
        const auto* src = rgb + (static_cast<std::size_t>(y + job.crop_y) * w + job.crop_x) * 3;
        for (int x = 0; x < cw; ++x) dst[x] = gray(src + x * 3);
      }
      image_bytes_ += cost;
      queue_.push_back(std::move(job));
      lock.unlock();
      cv_.notify_one();
      emit("IMAGE_QUEUED call=%llu capture_us=%lld\n", call, now_us() - end);
    } catch (...) { emit("IMAGE_DROP call=%llu reason=capture_exception\n", call); }
  }
  std::atomic<unsigned long long> calls{0}, episode{0}, last_returned_call{0}, uart_seq{0};
  // Callback sequence fields are touched only by the (serialized) algorithm callback.
  long long episode_begin = 0;
  int episode_index = 0;
 private:
  bool save(const std::string& path, const cv::Mat& pixels, const std::string& title,
            const std::string& timing) {
    cv::Mat out(pixels.rows + 48, pixels.cols, CV_8UC1, cv::Scalar(0));
    pixels.copyTo(out(cv::Rect(0, 48, pixels.cols, pixels.rows)));
    cv::putText(out, title, cv::Point(6, 17), cv::FONT_HERSHEY_SIMPLEX, .4, cv::Scalar(255), 1);
    cv::putText(out, timing, cv::Point(6, 37), cv::FONT_HERSHEY_SIMPLEX, .4, cv::Scalar(255), 1);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool header_ok = std::fprintf(f, "P5\n%d %d\n255\n", out.cols, out.rows) > 0;
    const bool data_ok = header_ok && std::fwrite(out.data, 1, out.total(), f) == out.total();
    const bool close_ok = std::fclose(f) == 0;
    return data_ok && close_ok;
  }
  void work() noexcept {
    for (;;) {
      ImageJob job;
      {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (queue_.empty() && stopping_) break;
        job = std::move(queue_.front()); queue_.pop_front();
      }
      try {
        struct statvfs disk{};
        if (statvfs(root_.c_str(), &disk) != 0 ||
            static_cast<unsigned long long>(disk.f_bavail) * disk.f_frsize < 256ULL * 1024 * 1024) {
          emit("IMAGE_DROP call=%llu reason=disk_space\n", job.call); continue;
        }
        const auto stem = "e" + std::to_string(job.episode) + "_n" + std::to_string(job.index) +
                          "_c" + std::to_string(job.call);
        const auto title = "episode=" + std::to_string(job.episode) + " frame=" +
                           std::to_string(job.index) + " call=" + std::to_string(job.call);
        const auto timing = "mono_us=" + std::to_string(job.begin);
        const auto start = now_us();
        const bool overview = save(root_ + "/images/" + stem + "_scene.pgm", job.overview, title, timing);
        const bool crop = save(root_ + "/images/" + stem + "_crop.pgm", job.crop, title, timing);
        emit("IMAGE_SAVED call=%llu episode=%llu index=%d begin_us=%lld end_us=%lld "
             "input=%dx%d scene=%dx%d crop=(%d,%d,%d,%d) header_rows=48 "
             "scene_ok=%d crop_ok=%d write_us=%lld stem=%s\n",
             job.call, job.episode, job.index, job.begin, job.end, job.input_w, job.input_h,
             job.overview.cols, job.overview.rows, job.crop_x, job.crop_y, job.crop.cols,
             job.crop.rows, overview, crop, now_us() - start, stem.c_str());
      } catch (...) { emit("IMAGE_DROP call=%llu reason=writer_exception\n", job.call); }
    }
  }
  std::string root_;
  int fd_ = -1;
  bool images_ = false, stopping_ = false, log_failed_ = false;
  std::size_t log_bytes_ = 0, image_bytes_ = 0;
  std::mutex log_mutex_, queue_mutex_;
  std::condition_variable cv_;
  std::deque<ImageJob> queue_;
  std::thread worker_;
};
Trace* trace() noexcept {
  // Created once, intentionally process-lifetime: vendor threads do not join on exit.
  static Trace* instance = []() noexcept -> Trace* {
    try { return new Trace; } catch (...) { return nullptr; }
  }();
  return instance;
}
int signed_be16(const unsigned char* p) {
  const unsigned v = (static_cast<unsigned>(p[0]) << 8) | p[1];
  return v >= 32768 ? static_cast<int>(v) - 65536 : static_cast<int>(v);
}
}  // namespace

extern "C" bool __wrap_tracker_detector_init(AI_TRACK_INFO* info) {
  const int incoming_errno = errno;
  Trace* t = trace();
  errno = incoming_errno;
  const bool result = __real_tracker_detector_init(info);
  ErrnoGuard guard;
  if (t) t->emit("MODEL_INIT mono_us=%lld result=%d\n", now_us(), result);
  return result;
}

extern "C" bool __wrap_do_tracker_detector(unsigned char* rgb, AI_TRACK_INFO* info,
                                            RECT_SIZE* show) {
  const int incoming_errno = errno;
  Trace* t = trace();
  if (!t || !t->enabled() || !info) {
    errno = incoming_errno;
    return __real_do_tracker_detector(rgb, info, show);
  }
  const auto call = ++t->calls;
  const auto begin = now_us();
  const int command = info->cmd;
  if (command == STATE_INIT_TRACK) {
    ++t->episode; t->episode_begin = begin; t->episode_index = 0;
  }
  const auto ep = t->episode.load();
  const bool active = command == STATE_INIT_TRACK || command == STATE_DO_TRACK || command == STATE_TRACK_CANCEL;
  const int index = t->episode_index;
  if (active) t->emit("CALL_BEGIN call=%llu episode=%llu index=%d mono_us=%lld cmd=%d "
      "init=(%d,%d,%d,%d) input=%dx%d center=(%d,%d) yaw=%.6f pitch=%.6f fov=%.6f hov=%.6f "
      "ir=%d pip=%d bad_frame=%d\n", call, ep, index, begin, command,
      info->init_track_box.x, info->init_track_box.y, info->init_track_box.width, info->init_track_box.height,
      info->rgb_img_size.width, info->rgb_img_size.height, info->center_x, info->center_y,
      info->yaw_angle, info->pitch_angle, info->fov, info->hov, info->is_ir_model, info->is_pip, info->is_bad_frame);
  errno = incoming_errno;
  const bool result = __real_do_tracker_detector(rgb, info, show);
  ErrnoGuard guard;
  const auto end = now_us();
  if (active) {
    const auto& c = info->track_box_result.rect[0];
    const auto& d = info->track_box_result.rect[1];
    t->emit("CALL_END call=%llu episode=%llu index=%d mono_us=%lld begin_us=%lld "
        "callback_us=%lld cmd_in=%d cmd_after=%d result=%d control=(%d,%d,%d,%d) "
        "display=(%d,%d,%d,%d) init_after=(%d,%d,%d,%d)\n",
        call, ep, index, end, begin, end - begin, command, info->cmd, result,
        c.x, c.y, c.width, c.height, d.x, d.y, d.width, d.height,
        info->init_track_box.x, info->init_track_box.y, info->init_track_box.width, info->init_track_box.height);
    t->last_returned_call = call;
    if (command != STATE_TRACK_CANCEL) {
      t->capture(rgb, *info, d, call, ep, index, begin, end, t->episode_begin);
      ++t->episode_index;
    }
    t->emit("CALL_RETURN call=%llu mono_us=%lld debug_after_us=%lld\n", call, now_us(), now_us() - end);
  }
  return result;
}

extern "C" int __wrap_write_serial(int fd, unsigned char* data, int len) {
  const int incoming_errno = errno;
  Trace* t = trace();
  const auto begin = now_us();
  // Snapshot only; pass the exact original pointer and length to the real function.
  unsigned char bytes[128];
  const int captured = data && len > 0 ? std::min(len, 128) : 0;
  if (captured) std::memcpy(bytes, data, captured);
  const auto seq = t ? ++t->uart_seq : 0;
  const auto preceding = t ? t->last_returned_call.load() : 0;
  errno = incoming_errno;
  const int result = __real_write_serial(fd, data, len);
  ErrnoGuard guard;
  const auto end = now_us();
  if (t && t->enabled()) {
    char hex[257];
    for (int i = 0; i < captured; ++i) std::snprintf(hex + 2 * i, 3, "%02x", bytes[i]);
    hex[2 * captured] = 0;
    // Protocol verified against this exact libviewlink.a: 55 aa dc LEN ID PAYLOAD CHECKSUM.
    const bool tracking = captured == 15 && len == 15 && bytes[0] == 0x55 &&
        bytes[1] == 0xaa && bytes[2] == 0xdc && bytes[3] == 12 && bytes[4] == 0x66;
    t->emit("UART_WRITE seq=%llu begin_us=%lld end_us=%lld fd=%d len=%d returned=%d errno=%d "
        "latest_algorithm_call=%llu track66=%d x=%d y=%d flags=%u valid=%d hex=%s\n",
        seq, begin, end, fd, len, result, guard.value, preceding, tracking,
        tracking ? signed_be16(bytes + 5) : 0, tracking ? signed_be16(bytes + 7) : 0,
        tracking ? bytes[9] : 0, tracking ? ((bytes[9] >> 1) & 1) : 0, hex);
  }
  return result;
}

#ifdef DIAGNOSTIC_TRACE_TEST
// Only used by the standalone tests to drain and close the writer deterministically.
extern "C" void diagnostic_test_finish() { delete trace(); }
#endif
