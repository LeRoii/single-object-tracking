#include "rknn_target.hpp"

#include <iostream>

int main() {
  int failures = 0;
  if (!nanotrack::is_supported_rknn_target("rk3566")) {
    std::cerr << "FAIL: rk3566 remains supported\n";
    ++failures;
  }
  if (!nanotrack::is_supported_rknn_target("rk3588")) {
    std::cerr << "FAIL: rk3588 is supported for Debug deployment\n";
    ++failures;
  }
  if (nanotrack::is_supported_rknn_target("rk3399pro")) {
    std::cerr << "FAIL: unsupported RKNN target is rejected\n";
    ++failures;
  }
  return failures == 0 ? 0 : 1;
}
