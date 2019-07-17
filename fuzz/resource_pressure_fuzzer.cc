#include "vellumgrid/engine.h"
#include "vellumgrid/job_format.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  auto batch = vellumgrid::read_job_batch(data, size);
  if (batch) (void)vellumgrid::run_job(*batch, 3);
  return 0;
}
