#include "vellumgrid/composition_rules.h"
#include "vellumgrid/engine.h"
#include "vellumgrid/job_format.h"
#include "vellumgrid/layout_math.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {
void append16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value));
  out.push_back(static_cast<uint8_t>(value >> 8U));
}
void append32(std::vector<uint8_t>& out, uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<uint8_t>(value >> shift));
}
void append_record(std::vector<uint8_t>& out, uint8_t opcode, uint32_t epoch,
                   const std::vector<uint8_t>& payload) {
  out.push_back(opcode);
  out.push_back(0);
  append16(out, static_cast<uint16_t>(payload.size()));
  append32(out, epoch);
  out.insert(out.end(), payload.begin(), payload.end());
}
}

int main() {
  using namespace vellumgrid;
  const std::vector<Point> triangle{{0, 0}, {100, 0}, {0, 100}};
  assert(signed_area_twice(triangle) == 10000);
  assert(bounds_of(triangle).width() == 100);
  BaselineGrid grid(0, 12, 1);
  assert(grid.snap(25) == 24);
  assert(grid.aligned(24));

  ConstraintSet constraints;
  assert(constraints.add(LayoutConstraint{1, ConstraintKind::MinimumGap, 10, 11, 8, 4, true}));
  std::vector<LayoutBox> boxes{
      LayoutBox{10, Rect{0, 0, 100, 40}, 30, 2, 0, true, false, false},
      LayoutBox{11, Rect{0, 44, 100, 84}, 74, 2, 0, true, false, false},
  };
  assert(constraints.evaluate(boxes, Rect{0, 0, 200, 200}).size() == 1);
  PaginationPolicy policy;
  policy.set_page_height(100);
  policy.set_top_bottom_margin(5, 5);
  assert(!policy.paginate(boxes).empty());

  std::vector<uint8_t> input{'V', 'G', 'D', '2'};
  append16(input, 2);
  append16(input, 0);
  append32(input, 2);
  append32(input, 0x12345678U);
  std::vector<uint8_t> begin;
  append32(begin, 77);
  append_record(input, 1, 0, begin);
  std::vector<uint8_t> page;
  append32(page, 900);
  append_record(input, 4, 1, page);
  auto batch = read_job_batch(input.data(), input.size());
  assert(batch.has_value());
  assert(batch->commands.size() == 2);
  const uint64_t digest = run_job(*batch, 0);
  assert(digest != 0);
  return 0;
}
