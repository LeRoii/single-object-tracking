#include "production_deployment_config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

void rejects(const std::function<void()>& action) {
  try {
    action();
  } catch (const std::invalid_argument&) {
    return;
  }
  throw std::runtime_error("expected invalid_argument");
}

struct Fixture {
  fs::path root;
  std::vector<std::pair<std::string, fs::path>> entries;

  Fixture() {
    std::string pattern = (fs::temp_directory_path() / "nanotrack-deployment-XXXXXX").string();
    char* directory = ::mkdtemp(pattern.data());
    if (!directory) throw std::runtime_error("cannot create fixture directory");
    root = directory;
    fs::create_directory(root / "tracker models");
    entries = {{"tracker_model_dir", root / "tracker models"},
               {"controller_config_path", root / "controller.conf"},
               {"detector_model_path", root / "detector.rknn"},
               {"detector_manifest_path", root / "manifest.json"},
               {"detector_labels_path", root / "labels.txt"}};
    for (const char* name : {"template_backbone.rknn", "search_backbone.rknn", "head.rknn"}) {
      write(entries[0].second / name, "placeholder");
    }
    for (std::size_t i = 1; i < entries.size(); ++i) write(entries[i].second, "placeholder");
  }

  ~Fixture() {
    std::error_code ignored;
    fs::remove_all(root, ignored);
  }

  void write(const fs::path& path, const std::string& text) const {
    std::ofstream stream(path);
    stream << text;
    if (!stream) throw std::runtime_error("cannot write fixture " + path.string());
  }

  std::string text() const {
    std::string result = "  # deployment\r\n\n";
    for (const auto& entry : entries) {
      result += " \t" + entry.first + " = " + entry.second.string() + " \t # artifact\r\n";
    }
    return result;
  }

  nanotrack::ProductionDeploymentConfig load(const std::string& text) const {
    const fs::path path = root / "deployment.conf";
    write(path, text);
    return nanotrack::load_production_deployment_config(path.string());
  }
};

void loads_complete_config() {
  Fixture f;
  const auto config = f.load(f.text());
  require(config.tracker_model_dir == (f.root / "tracker models").string(), "tracker path");
  require(config.controller_config_path == (f.root / "controller.conf").string(), "controller path");
  require(config.detector_model_path == (f.root / "detector.rknn").string(), "detector path");
  require(config.detector_manifest_path == (f.root / "manifest.json").string(), "manifest path");
  require(config.detector_labels_path == (f.root / "labels.txt").string(), "labels path");
  nanotrack::validate_production_deployment_config(config);
}

void rejects_unknown_key() {
  Fixture f;
  rejects([&] { f.load(f.text() + "unknown=/tmp\n"); });
}

void rejects_duplicate_key() {
  Fixture f;
  for (const auto& entry : f.entries) {
    rejects([&] { f.load(f.text() + entry.first + "=" + entry.second.string() + "\n"); });
  }
}

void rejects_missing_key() {
  Fixture f;
  for (std::size_t missing = 0; missing < f.entries.size(); ++missing) {
    std::string text;
    for (std::size_t i = 0; i < f.entries.size(); ++i) {
      if (i != missing) text += f.entries[i].first + "=" + f.entries[i].second.string() + "\n";
    }
    rejects([&] { f.load(text); });
  }
}

void rejects_empty_value() {
  Fixture f;
  for (std::size_t empty = 0; empty < f.entries.size(); ++empty) {
    std::string text;
    for (std::size_t i = 0; i < f.entries.size(); ++i) {
      text += f.entries[i].first + "=" +
              (i == empty ? " \t # empty" : f.entries[i].second.string()) + "\n";
    }
    rejects([&] { f.load(text); });
  }
}

void rejects_missing_tracker_model() {
  for (const char* name : {"template_backbone.rknn", "search_backbone.rknn", "head.rknn"}) {
    Fixture f;
    fs::remove(f.entries[0].second / name);
    rejects([&] { f.load(f.text()); });
  }
}

void rejects_missing_detector_artifact() {
  for (std::size_t i = 2; i < 5; ++i) {
    Fixture f;
    fs::remove(f.entries[i].second);
    rejects([&] { f.load(f.text()); });
  }
}

void rejects_missing_controller_config() {
  Fixture f;
  fs::remove(f.entries[1].second);
  rejects([&] { f.load(f.text()); });
}

void rejects_non_regular_artifacts() {
  for (std::size_t i = 1; i < 5; ++i) {
    Fixture f;
    fs::remove(f.entries[i].second);
    fs::create_directory(f.entries[i].second);
    rejects([&] { f.load(f.text()); });
  }
  Fixture f;
  fs::remove(f.entries[0].second / "head.rknn");
  fs::create_directory(f.entries[0].second / "head.rknn");
  rejects([&] { f.load(f.text()); });
}

void rejects_malformed_line() {
  Fixture f;
  rejects([&] { f.load(f.text() + "tracker_model_dir\n"); });
  rejects([&] { f.load(f.text() + "=value\n"); });
}

void rejects_unreadable_config() {
  Fixture f;
  rejects([&] { nanotrack::load_production_deployment_config((f.root / "missing.conf").string()); });
  rejects([&] { nanotrack::load_production_deployment_config(f.root.string()); });
}

void validates_direct_config() {
  Fixture f;
  auto config = f.load(f.text());
  config.tracker_model_dir.clear();
  rejects([&] { nanotrack::validate_production_deployment_config(config); });
  config = f.load(f.text());
  config.detector_model_path.clear();
  rejects([&] { nanotrack::validate_production_deployment_config(config); });
}
}  // namespace

int main() {
  const std::pair<const char*, void (*)()> tests[] = {
      {"loads_complete_config", loads_complete_config},
      {"rejects_unknown_key", rejects_unknown_key},
      {"rejects_duplicate_key", rejects_duplicate_key},
      {"rejects_missing_key", rejects_missing_key},
      {"rejects_empty_value", rejects_empty_value},
      {"rejects_missing_tracker_model", rejects_missing_tracker_model},
      {"rejects_missing_detector_artifact", rejects_missing_detector_artifact},
      {"rejects_missing_controller_config", rejects_missing_controller_config},
      {"rejects_non_regular_artifacts", rejects_non_regular_artifacts},
      {"rejects_malformed_line", rejects_malformed_line},
      {"rejects_unreadable_config", rejects_unreadable_config},
      {"validates_direct_config", validates_direct_config}};
  int failures = 0;
  for (const auto& test : tests) {
    try {
      test.second();
      std::cout << "PASS: " << test.first << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}
