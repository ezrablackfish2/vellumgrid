#include "vellumgrid/composition_rules.h"

#include "vellumgrid/byte_cursor.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <utility>

namespace vellumgrid {
namespace {

const LayoutBox* box_by_id(const std::vector<LayoutBox>& boxes, uint32_t id) {
  auto it = std::find_if(boxes.begin(), boxes.end(),
                         [id](const LayoutBox& box) { return box.id == id; });
  return it == boxes.end() ? nullptr : &*it;
}

int64_t square_saturated(int64_t value) noexcept {
  const uint64_t magnitude = value < 0 ? static_cast<uint64_t>(-(value + 1)) + 1U
                                       : static_cast<uint64_t>(value);
  constexpr uint64_t limit = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  if (magnitude != 0 && magnitude > limit / magnitude) {
    return std::numeric_limits<int64_t>::max();
  }
  return static_cast<int64_t>(magnitude * magnitude);
}

int64_t add_saturated(int64_t left, int64_t right) noexcept {
  if (right > 0 && left > std::numeric_limits<int64_t>::max() - right) {
    return std::numeric_limits<int64_t>::max();
  }
  if (right < 0 && left < std::numeric_limits<int64_t>::min() - right) {
    return std::numeric_limits<int64_t>::min();
  }
  return left + right;
}

int64_t box_height(const LayoutBox& box) noexcept {
  return std::max<int64_t>(0, box.bounds.height());
}

ConstraintViolation violation(const LayoutConstraint& constraint,
                              uint32_t object_id, int64_t magnitude) {
  ConstraintViolation result;
  result.constraint_id = constraint.id;
  result.object_id = object_id;
  result.magnitude = std::max<int64_t>(0, magnitude);
  result.priority = constraint.priority;
  return result;
}

}  // namespace

bool ConstraintSet::add(LayoutConstraint constraint) {
  if (constraint.id == 0) return false;
  if (find(constraint.id) != nullptr) return false;
  if (constraint.priority == 0) constraint.priority = 1;
  constraints_.push_back(std::move(constraint));
  return true;
}

bool ConstraintSet::remove(uint32_t id) {
  const auto old_size = constraints_.size();
  constraints_.erase(
      std::remove_if(constraints_.begin(), constraints_.end(),
                     [id](const LayoutConstraint& value) { return value.id == id; }),
      constraints_.end());
  return constraints_.size() != old_size;
}

void ConstraintSet::enable(uint32_t id, bool enabled) {
  auto it = std::find_if(constraints_.begin(), constraints_.end(),
                         [id](const LayoutConstraint& value) { return value.id == id; });
  if (it != constraints_.end()) it->enabled = enabled;
}

const LayoutConstraint* ConstraintSet::find(uint32_t id) const {
  auto it = std::find_if(constraints_.begin(), constraints_.end(),
                         [id](const LayoutConstraint& value) { return value.id == id; });
  return it == constraints_.end() ? nullptr : &*it;
}

std::vector<ConstraintViolation> ConstraintSet::evaluate(
    const std::vector<LayoutBox>& boxes, Rect available) const {
  std::vector<ConstraintViolation> result;
  result.reserve(constraints_.size());
  for (const LayoutConstraint& constraint : constraints_) {
    if (!constraint.enabled) continue;
    const LayoutBox* first = box_by_id(boxes, constraint.first);
    const LayoutBox* second = box_by_id(boxes, constraint.second);
    if (first == nullptr) {
      result.push_back(violation(constraint, constraint.first, 1));
      continue;
    }
    switch (constraint.kind) {
      case ConstraintKind::KeepTogether:
        if (!available.contains(Point{first->bounds.left, first->bounds.top}) ||
            !available.contains(Point{first->bounds.right, first->bounds.bottom})) {
          result.push_back(violation(constraint, first->id,
                                     box_height(*first) - available.height()));
        }
        break;
      case ConstraintKind::KeepWithNext:
        if (second == nullptr) {
          result.push_back(violation(constraint, constraint.second, 1));
        } else if (second->bounds.top < first->bounds.bottom) {
          result.push_back(violation(constraint, second->id,
                                     static_cast<int64_t>(first->bounds.bottom) -
                                         second->bounds.top));
        }
        break;
      case ConstraintKind::MinimumGap:
        if (second != nullptr) {
          const int64_t gap = static_cast<int64_t>(second->bounds.top) - first->bounds.bottom;
          if (gap < constraint.amount) {
            result.push_back(violation(constraint, second->id, constraint.amount - gap));
          }
        }
        break;
      case ConstraintKind::MaximumGap:
        if (second != nullptr) {
          const int64_t gap = static_cast<int64_t>(second->bounds.top) - first->bounds.bottom;
          if (gap > constraint.amount) {
            result.push_back(violation(constraint, second->id, gap - constraint.amount));
          }
        }
        break;
      case ConstraintKind::AlignTop:
        if (second != nullptr && first->bounds.top != second->bounds.top) {
          result.push_back(violation(constraint, second->id,
                                     std::llabs(static_cast<int64_t>(first->bounds.top) -
                                                second->bounds.top)));
        }
        break;
      case ConstraintKind::AlignBaseline:
        if (second != nullptr && first->baseline != second->baseline) {
          result.push_back(violation(constraint, second->id,
                                     std::llabs(static_cast<int64_t>(first->baseline) -
                                                second->baseline)));
        }
        break;
      case ConstraintKind::AvoidRegion: {
        const Rect overlap = intersect_rect(first->bounds,
                                            Rect{constraint.amount, available.top,
                                                 available.right, available.bottom});
        if (!overlap.empty()) {
          result.push_back(violation(constraint, first->id,
                                     overlap.width() * overlap.height()));
        }
        break;
      }
      case ConstraintKind::PinToPage:
        if (first->bounds.top < available.top || first->bounds.bottom > available.bottom) {
          result.push_back(violation(constraint, first->id,
                                     std::max<int64_t>(available.top - first->bounds.top,
                                                       first->bounds.bottom - available.bottom)));
        }
        break;
    }
  }
  std::stable_sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    if (left.priority != right.priority) return left.priority > right.priority;
    if (left.magnitude != right.magnitude) return left.magnitude > right.magnitude;
    return left.constraint_id < right.constraint_id;
  });
  return result;
}

std::vector<uint32_t> ConstraintSet::dependency_order() const {
  std::unordered_map<uint32_t, std::vector<uint32_t>> edges;
  std::unordered_map<uint32_t, std::size_t> indegree;
  for (const LayoutConstraint& constraint : constraints_) {
    if (!constraint.enabled || constraint.first == 0) continue;
    indegree.try_emplace(constraint.first, 0);
    if (constraint.second == 0 || constraint.second == constraint.first) continue;
    edges[constraint.first].push_back(constraint.second);
    ++indegree[constraint.second];
  }
  std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> ready;
  for (const auto& [id, degree] : indegree) {
    if (degree == 0) ready.push(id);
  }
  std::vector<uint32_t> order;
  order.reserve(indegree.size());
  while (!ready.empty()) {
    const uint32_t id = ready.top();
    ready.pop();
    order.push_back(id);
    auto edge_it = edges.find(id);
    if (edge_it == edges.end()) continue;
    for (uint32_t target : edge_it->second) {
      auto degree_it = indegree.find(target);
      if (degree_it == indegree.end() || degree_it->second == 0) continue;
      --degree_it->second;
      if (degree_it->second == 0) ready.push(target);
    }
  }
  if (order.size() != indegree.size()) {
    std::vector<uint32_t> cyclic;
    for (const auto& [id, degree] : indegree) {
      if (degree != 0) cyclic.push_back(id);
    }
    std::sort(cyclic.begin(), cyclic.end());
    order.insert(order.end(), cyclic.begin(), cyclic.end());
  }
  return order;
}

uint64_t ConstraintSet::digest(uint64_t seed) const noexcept {
  uint64_t result = mix64(seed ^ constraints_.size());
  for (const LayoutConstraint& constraint : constraints_) {
    uint64_t packed = constraint.id;
    packed ^= static_cast<uint64_t>(constraint.first) << 7U;
    packed ^= static_cast<uint64_t>(constraint.second) << 19U;
    packed ^= static_cast<uint64_t>(constraint.priority) << 51U;
    packed ^= static_cast<uint64_t>(constraint.enabled) << 63U;
    result = mix64(result ^ packed ^ static_cast<uint8_t>(constraint.kind));
  }
  return result;
}

std::size_t ConstraintSet::size() const noexcept { return constraints_.size(); }

BaselineGrid::BaselineGrid(int32_t origin, int32_t step, int32_t tolerance) noexcept
    : origin_(origin), step_(std::max<int32_t>(1, step)),
      tolerance_(std::max<int32_t>(0, tolerance)) {}

int32_t BaselineGrid::snap(int32_t coordinate) const noexcept {
  const int64_t relative = static_cast<int64_t>(coordinate) - origin_;
  const int64_t quotient = relative >= 0 ? (relative + step_ / 2) / step_
                                         : (relative - step_ / 2) / step_;
  const int64_t value = static_cast<int64_t>(origin_) + quotient * step_;
  return static_cast<int32_t>(std::clamp<int64_t>(
      value, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
}

int32_t BaselineGrid::next(int32_t coordinate) const noexcept {
  int32_t value = snap(coordinate);
  if (value <= coordinate && value <= std::numeric_limits<int32_t>::max() - step_) value += step_;
  return value;
}

int32_t BaselineGrid::previous(int32_t coordinate) const noexcept {
  int32_t value = snap(coordinate);
  if (value >= coordinate && value >= std::numeric_limits<int32_t>::min() + step_) value -= step_;
  return value;
}

bool BaselineGrid::aligned(int32_t coordinate) const noexcept {
  return std::llabs(static_cast<int64_t>(snap(coordinate)) - coordinate) <= tolerance_;
}

std::vector<int32_t> BaselineGrid::lines(Rect page) const {
  std::vector<int32_t> result;
  if (page.empty()) return result;
  int32_t value = next(page.top - 1);
  while (value <= page.bottom) {
    result.push_back(value);
    if (value > std::numeric_limits<int32_t>::max() - step_) break;
    value += step_;
  }
  return result;
}

void PaginationPolicy::set_page_height(int64_t height) noexcept {
  page_height_ = std::max<int64_t>(1, height);
}
void PaginationPolicy::set_top_bottom_margin(int64_t top, int64_t bottom) noexcept {
  top_margin_ = std::max<int64_t>(0, top);
  bottom_margin_ = std::max<int64_t>(0, bottom);
}
void PaginationPolicy::set_minimum_lines(uint16_t orphan, uint16_t widow) noexcept {
  orphan_lines_ = std::max<uint16_t>(1, orphan);
  widow_lines_ = std::max<uint16_t>(1, widow);
}
void PaginationPolicy::set_raggedness_weight(uint32_t weight) noexcept {
  raggedness_weight_ = std::max<uint32_t>(1, weight);
}
void PaginationPolicy::set_keep_weight(uint32_t weight) noexcept {
  keep_weight_ = std::max<uint32_t>(1, weight);
}

int64_t PaginationPolicy::content_height() const noexcept {
  return std::max<int64_t>(1, page_height_ - top_margin_ - bottom_margin_);
}

int64_t PaginationPolicy::gap_between(const LayoutBox& first,
                                      const LayoutBox& second) const noexcept {
  return std::max<int64_t>(0, static_cast<int64_t>(second.bounds.top) - first.bounds.bottom);
}

bool PaginationPolicy::violates_orphans(const LayoutBox& box,
                                        int64_t visible_height) const noexcept {
  if (box.line_count == 0 || box_height(box) == 0) return false;
  const int64_t line_height = std::max<int64_t>(1, box_height(box) / box.line_count);
  return visible_height / line_height < orphan_lines_;
}

bool PaginationPolicy::violates_widows(const LayoutBox& box,
                                       int64_t remaining_height) const noexcept {
  if (box.line_count == 0 || box_height(box) == 0) return false;
  const int64_t line_height = std::max<int64_t>(1, box_height(box) / box.line_count);
  return remaining_height / line_height < widow_lines_;
}

int64_t PaginationPolicy::score_slice(const std::vector<LayoutBox>& boxes,
                                      std::size_t first, std::size_t last,
                                      BreakReason* reason) const {
  if (first >= boxes.size() || last > boxes.size() || first >= last) {
    if (reason) *reason = BreakReason::Oversize;
    return std::numeric_limits<int64_t>::max();
  }
  int64_t used = 0;
  BreakReason local_reason = BreakReason::Natural;
  for (std::size_t index = first; index < last; ++index) {
    used = add_saturated(used, box_height(boxes[index]));
    if (index + 1 < last) used = add_saturated(used, gap_between(boxes[index], boxes[index + 1]));
    if (boxes[index].explicit_break_before && index != first) {
      local_reason = BreakReason::Explicit;
      used = add_saturated(used, static_cast<int64_t>(keep_weight_) * 50);
    }
    if (boxes[index].explicit_break_after && index + 1 != last) {
      local_reason = BreakReason::Explicit;
      used = add_saturated(used, static_cast<int64_t>(keep_weight_) * 50);
    }
  }
  const int64_t capacity = content_height();
  if (used > capacity) {
    if (reason) *reason = BreakReason::Oversize;
    return add_saturated(square_saturated(used - capacity),
                         static_cast<int64_t>(keep_weight_) * 1000);
  }
  int64_t penalty = square_saturated(capacity - used);
  if (raggedness_weight_ > 1 && penalty < std::numeric_limits<int64_t>::max() /
                                              raggedness_weight_) {
    penalty *= raggedness_weight_;
  }
  if (last < boxes.size()) {
    const LayoutBox& before = boxes[last - 1];
    const LayoutBox& after = boxes[last];
    if (before.keep_priority != 0) {
      local_reason = BreakReason::KeepViolation;
      penalty = add_saturated(penalty,
                              static_cast<int64_t>(before.keep_priority) * keep_weight_);
    }
    if (violates_orphans(before, box_height(before) / 2) ||
        violates_widows(after, box_height(after) / 2)) {
      local_reason = BreakReason::WidowOrphan;
      penalty = add_saturated(penalty, static_cast<int64_t>(keep_weight_) * 25);
    }
  }
  if (reason) *reason = local_reason;
  return penalty;
}

std::vector<BreakCandidate> PaginationPolicy::candidates(
    const std::vector<LayoutBox>& boxes) const {
  std::vector<BreakCandidate> result;
  int64_t used = 0;
  for (std::size_t index = 0; index < boxes.size(); ++index) {
    used = add_saturated(used, box_height(boxes[index]));
    if (index != 0) used = add_saturated(used, gap_between(boxes[index - 1], boxes[index]));
    BreakReason reason = BreakReason::Natural;
    int64_t score = score_slice(boxes, 0, index + 1, &reason);
    if (used >= content_height() / 2 || boxes[index].explicit_break_after ||
        index + 1 == boxes.size()) {
      result.push_back(BreakCandidate{index + 1, used, score, reason});
    }
  }
  return result;
}

std::vector<PageSlice> PaginationPolicy::paginate(
    const std::vector<LayoutBox>& boxes) const {
  if (boxes.empty()) return {};
  const std::size_t count = boxes.size();
  const int64_t infinity = std::numeric_limits<int64_t>::max();
  std::vector<int64_t> best(count + 1, infinity);
  std::vector<std::size_t> previous(count + 1, 0);
  std::vector<BreakReason> reasons(count + 1, BreakReason::Natural);
  best[0] = 0;
  for (std::size_t end = 1; end <= count; ++end) {
    for (std::size_t begin = end; begin-- > 0;) {
      BreakReason reason = BreakReason::Natural;
      const int64_t local = score_slice(boxes, begin, end, &reason);
      if (local == infinity || best[begin] == infinity) continue;
      const int64_t candidate = add_saturated(best[begin], local);
      if (candidate < best[end]) {
        best[end] = candidate;
        previous[end] = begin;
        reasons[end] = reason;
      }
      if (end - begin > 64) break;
    }
  }
  std::vector<PageSlice> reversed;
  std::size_t cursor = count;
  while (cursor != 0) {
    const std::size_t begin = previous[cursor];
    int64_t used = 0;
    for (std::size_t index = begin; index < cursor; ++index) {
      used = add_saturated(used, box_height(boxes[index]));
      if (index + 1 < cursor) used = add_saturated(used, gap_between(boxes[index], boxes[index + 1]));
    }
    reversed.push_back(PageSlice{begin, cursor, used,
                                 score_slice(boxes, begin, cursor), reasons[cursor]});
    if (begin == cursor) break;
    cursor = begin;
  }
  std::reverse(reversed.begin(), reversed.end());
  return reversed;
}

std::vector<LayoutBox> balance_columns(const std::vector<LayoutBox>& boxes,
                                       std::size_t column_count,
                                       int64_t gutter,
                                       Rect available) {
  if (boxes.empty() || column_count == 0 || available.empty()) return {};
  column_count = std::min<std::size_t>(column_count, 32);
  const int64_t total_gutter = static_cast<int64_t>(column_count - 1) * std::max<int64_t>(0, gutter);
  const int64_t column_width = std::max<int64_t>(1, (available.width() - total_gutter) /
                                                       static_cast<int64_t>(column_count));
  int64_t total_height = 0;
  for (const LayoutBox& box : boxes) total_height = add_saturated(total_height, box_height(box));
  const int64_t target = std::max<int64_t>(1, (total_height + column_count - 1) /
                                                 static_cast<int64_t>(column_count));
  std::vector<LayoutBox> result = boxes;
  std::size_t column = 0;
  int64_t y = available.top;
  int64_t used = 0;
  for (LayoutBox& box : result) {
    const int64_t height = box_height(box);
    if (column + 1 < column_count && used != 0 && used + height > target) {
      ++column;
      y = available.top;
      used = 0;
    }
    const int64_t x = static_cast<int64_t>(available.left) +
                      static_cast<int64_t>(column) * (column_width + gutter);
    box.bounds.left = static_cast<int32_t>(std::clamp<int64_t>(
        x, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
    box.bounds.right = static_cast<int32_t>(std::clamp<int64_t>(
        x + column_width, std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int32_t>::max()));
    box.bounds.top = static_cast<int32_t>(std::clamp<int64_t>(
        y, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
    box.bounds.bottom = static_cast<int32_t>(std::clamp<int64_t>(
        y + height, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
    y += height;
    used += height;
  }
  return result;
}

std::vector<Rect> exclusion_free_regions(Rect available,
                                         const std::vector<Rect>& exclusions) {
  if (available.empty()) return {};
  std::vector<Rect> regions{available};
  for (Rect exclusion : exclusions) {
    if (exclusion.empty()) continue;
    std::vector<Rect> next;
    for (Rect region : regions) {
      Rect overlap = intersect_rect(region, exclusion);
      if (overlap.empty()) {
        next.push_back(region);
        continue;
      }
      Rect top{region.left, region.top, region.right, overlap.top};
      Rect bottom{region.left, overlap.bottom, region.right, region.bottom};
      Rect left{region.left, overlap.top, overlap.left, overlap.bottom};
      Rect right{overlap.right, overlap.top, region.right, overlap.bottom};
      if (!top.empty()) next.push_back(top);
      if (!bottom.empty()) next.push_back(bottom);
      if (!left.empty()) next.push_back(left);
      if (!right.empty()) next.push_back(right);
    }
    regions.swap(next);
  }
  std::stable_sort(regions.begin(), regions.end(), [](Rect left, Rect right) {
    const int64_t left_area = left.width() * left.height();
    const int64_t right_area = right.width() * right.height();
    if (left_area != right_area) return left_area > right_area;
    if (left.top != right.top) return left.top < right.top;
    return left.left < right.left;
  });
  return regions;
}

uint64_t composition_digest(const std::vector<LayoutBox>& boxes,
                            const std::vector<PageSlice>& pages,
                            uint64_t seed) noexcept {
  uint64_t result = mix64(seed ^ boxes.size() ^ (pages.size() << 16U));
  for (const LayoutBox& box : boxes) {
    result = mix64(result ^ box.id ^ static_cast<uint64_t>(box.bounds.left));
    result = mix64(result ^ static_cast<uint64_t>(box.bounds.top) << 7U);
    result = mix64(result ^ static_cast<uint64_t>(box.bounds.right) << 17U);
    result = mix64(result ^ static_cast<uint64_t>(box.bounds.bottom) << 29U);
    result = mix64(result ^ box.line_count ^ (static_cast<uint64_t>(box.keep_priority) << 32U));
  }
  for (const PageSlice& page : pages) {
    result = mix64(result ^ page.first ^ (page.last << 20U));
    result = mix64(result ^ static_cast<uint64_t>(page.used_height));
    result = mix64(result ^ static_cast<uint64_t>(page.penalty));
    result ^= static_cast<uint8_t>(page.reason);
  }
  return result;
}

}  // namespace vellumgrid
