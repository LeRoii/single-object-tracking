#include "sha256.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
  const std::filesystem::path path = "/tmp/nanotrack_sha256_fixture";
  {
    std::ofstream stream(path, std::ios::binary);
    stream << "abc";
  }
  const std::string actual = nanotrack::sha256_file(path.string());
  std::filesystem::remove(path);
  if (actual !=
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
    std::cerr << "FAIL: SHA-256 abc mismatch: " << actual << '\n';
    return 1;
  }
  std::cout << "sha256 tests passed\n";
  return 0;
}
