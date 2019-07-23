#pragma once

#include "vellumgrid/layout_math.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vellumgrid {

enum class ConstraintKind : uint8_t {
  KeepTogether = 0,
  KeepWithNext = 1,
  MinimumGap = 2,
  MaximumGap = 3,
  AlignTop = 4,
  AlignBaseline = 5,
  AvoidRegion = 6,
  PinToPage = 7,
};

enum class BreakReason : uint8_t {
  Natural = 0,
  ColumnLimit = 1,
  Explicit = 2,
  KeepViolation = 3,
  WidowOrphan = 4,
  Oversize = 5,
};

struct LayoutBox {
  uint32_t id{};
  Rect bounds;
  int32_t baseline{};
  uint16_t line_count{};
  uint16_t keep_priority{};
  bool splittable{true};
  bool explicit_break_before{};
  bool explicit_break_after{};
};

struct LayoutConstraint {
  uint32_t id{};
  ConstraintKind kind{ConstraintKind::KeepTogether};
  uint32_t first{};
  uint32_t second{};
  int32_t amount{};
  uint16_t priority{};
  bool enabled{true};
};

struct ConstraintViolation {
  uint32_t constraint_id{};
  uint32_t object_id{};
  int64_t magnitude{};
  uint16_t priority{};
};

struct BreakCandidate {
  std::size_t index{};
  int64_t used_height{};
  int64_t penalty{};
  BreakReason reason{BreakReason::Natural};
};

struct PageSlice {
  std::size_t first{};
  std::size_t last{};
  int64_t used_height{};
  int64_t penalty{};
  BreakReason reason{BreakReason::Natural};
};

class ConstraintSet {
 public:
  bool add(LayoutConstraint constraint);
  bool remove(uint32_t id);
  void enable(uint32_t id, bool enabled);
  const LayoutConstraint* find(uint32_t id) const;
  std::vector<ConstraintViolation> evaluate(
      const std::vector<LayoutBox>& boxes, Rect available) const;
  std::vector<uint32_t> dependency_order() const;
  uint64_t digest(uint64_t seed) const noexcept;
  std::size_t size() const noexcept;

 private:
  std::vector<LayoutConstraint> constraints_;
};

class BaselineGrid {
 public:
  BaselineGrid(int32_t origin, int32_t step, int32_t tolerance) noexcept;
  int32_t snap(int32_t coordinate) const noexcept;
  int32_t next(int32_t coordinate) const noexcept;
  int32_t previous(int32_t coordinate) const noexcept;
  bool aligned(int32_t coordinate) const noexcept;
  std::vector<int32_t> lines(Rect page) const;

 private:
  int32_t origin_{};
  int32_t step_{1};
  int32_t tolerance_{};
};

class PaginationPolicy {
 public:
  void set_page_height(int64_t height) noexcept;
  void set_top_bottom_margin(int64_t top, int64_t bottom) noexcept;
  void set_minimum_lines(uint16_t orphan, uint16_t widow) noexcept;
  void set_raggedness_weight(uint32_t weight) noexcept;
  void set_keep_weight(uint32_t weight) noexcept;

  std::vector<BreakCandidate> candidates(
      const std::vector<LayoutBox>& boxes) const;
  std::vector<PageSlice> paginate(const std::vector<LayoutBox>& boxes) const;
  int64_t score_slice(const std::vector<LayoutBox>& boxes,
                      std::size_t first, std::size_t last,
                      BreakReason* reason = nullptr) const;

 private:
  int64_t content_height() const noexcept;
  int64_t gap_between(const LayoutBox& first, const LayoutBox& second) const noexcept;
  bool violates_orphans(const LayoutBox& box, int64_t visible_height) const noexcept;
  bool violates_widows(const LayoutBox& box, int64_t remaining_height) const noexcept;

  int64_t page_height_{1000};
  int64_t top_margin_{40};
  int64_t bottom_margin_{40};
  uint16_t orphan_lines_{2};
  uint16_t widow_lines_{2};
  uint32_t raggedness_weight_{1};
  uint32_t keep_weight_{100};
};

std::vector<LayoutBox> balance_columns(const std::vector<LayoutBox>& boxes,
                                       std::size_t column_count,
                                       int64_t gutter,
                                       Rect available);
std::vector<Rect> exclusion_free_regions(Rect available,
                                         const std::vector<Rect>& exclusions);
uint64_t composition_digest(const std::vector<LayoutBox>& boxes,
                            const std::vector<PageSlice>& pages,
                            uint64_t seed) noexcept;

}  // namespace vellumgrid
