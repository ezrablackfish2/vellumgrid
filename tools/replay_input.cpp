#include "vellumgrid/engine.h"
#include "vellumgrid/job_format.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: replay_input <input> [mode]\n";
    return 64;
  }
  std::ifstream stream(argv[1], std::ios::binary);
  if (!stream) {
    std::cerr << "cannot open input\n";
    return 66;
  }
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),
                             std::istreambuf_iterator<char>());
  auto batch = vellumgrid::read_job_batch(bytes.data(), bytes.size());
  if (!batch) {
    std::cerr << "invalid job\n";
    return 65;
  }
  uint8_t mode = 0;
  if (argc == 3) mode = static_cast<uint8_t>(std::stoi(argv[2]));
  volatile uint64_t result = vellumgrid::run_job(*batch, mode);
  std::cout << "digest=" << result << "\n";
  return 0;
}
