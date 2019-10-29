#include "vellumgrid/layout_math.h"

#include "vellumgrid/byte_cursor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vellumgrid {
namespace {
constexpr int64_t kFixedOne = 1LL << 16;

int32_t clamp_i32(int64_t value) noexcept {
  if (value < std::numeric_limits<int32_t>::min()) return std::numeric_limits<int32_t>::min();
  if (value > std::numeric_limits<int32_t>::max()) return std::numeric_limits<int32_t>::max();
  return static_cast<int32_t>(value);
}

int64_t cross(Point a, Point b, Point c) noexcept {
  const int64_t abx = static_cast<int64_t>(b.x) - a.x;
  const int64_t aby = static_cast<int64_t>(b.y) - a.y;
  const int64_t acx = static_cast<int64_t>(c.x) - a.x;
  const int64_t acy = static_cast<int64_t>(c.y) - a.y;
  return abx * acy - aby * acx;
}

enum class Edge { Left, Right, Top, Bottom };

bool inside(Point point, Rect clip, Edge edge) noexcept {
  switch (edge) {
    case Edge::Left: return point.x >= clip.left;
    case Edge::Right: return point.x <= clip.right;
    case Edge::Top: return point.y >= clip.top;
    case Edge::Bottom: return point.y <= clip.bottom;
  }
  return false;
}

Point intersection(Point a, Point b, Rect clip, Edge edge) noexcept {
  const int64_t dx = static_cast<int64_t>(b.x) - a.x;
  const int64_t dy = static_cast<int64_t>(b.y) - a.y;
  if (edge == Edge::Left || edge == Edge::Right) {
    const int64_t x = edge == Edge::Left ? clip.left : clip.right;
    if (dx == 0) return Point{static_cast<int32_t>(x), a.y};
    const int64_t y = a.y + dy * (x - a.x) / dx;
    return Point{static_cast<int32_t>(x), clamp_i32(y)};
  }
  const int64_t y = edge == Edge::Top ? clip.top : clip.bottom;
  if (dy == 0) return Point{a.x, static_cast<int32_t>(y)};
  const int64_t x = a.x + dx * (y - a.y) / dy;
  return Point{clamp_i32(x), static_cast<int32_t>(y)};
}

std::vector<Point> clip_edge(const std::vector<Point>& input, Rect clip, Edge edge) {
  std::vector<Point> output;
  if (input.empty()) return output;
  output.reserve(input.size() + 4);
  Point previous = input.back();
  bool previous_inside = inside(previous, clip, edge);
  for (Point current : input) {
    const bool current_inside = inside(current, clip, edge);
    if (current_inside) {
      if (!previous_inside) output.push_back(intersection(previous, current, clip, edge));
      output.push_back(current);
    } else if (previous_inside) {
      output.push_back(intersection(previous, current, clip, edge));
    }
    previous = current;
    previous_inside = current_inside;
  }
  return output;
}

}  // namespace

bool Rect::empty() const noexcept { return right <= left || bottom <= top; }
int64_t Rect::width() const noexcept { return std::max<int64_t>(0, static_cast<int64_t>(right) - left); }
int64_t Rect::height() const noexcept { return std::max<int64_t>(0, static_cast<int64_t>(bottom) - top); }
bool Rect::contains(Point point) const noexcept {
  return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
}

AffineTransform AffineTransform::identity() noexcept { return {}; }
AffineTransform AffineTransform::translation(int32_t x, int32_t y) noexcept {
  AffineTransform result;
  result.m02 = static_cast<int64_t>(x) << 16;
  result.m12 = static_cast<int64_t>(y) << 16;
  return result;
}
AffineTransform AffineTransform::scale(int32_t x_16_16, int32_t y_16_16) noexcept {
  AffineTransform result;
  result.m00 = x_16_16;
  result.m11 = y_16_16;
  return result;
}
Point AffineTransform::apply(Point point) const noexcept {
  const int64_t x = (m00 * point.x + m01 * point.y + m02) / kFixedOne;
  const int64_t y = (m10 * point.x + m11 * point.y + m12) / kFixedOne;
  return Point{clamp_i32(x), clamp_i32(y)};
}

AffineTransform compose(const AffineTransform& first,
                        const AffineTransform& second) noexcept {
  AffineTransform out;
  out.m00 = (first.m00 * second.m00 + first.m01 * second.m10) / kFixedOne;
  out.m01 = (first.m00 * second.m01 + first.m01 * second.m11) / kFixedOne;
  out.m02 = (first.m00 * second.m02 + first.m01 * second.m12) / kFixedOne + first.m02;
  out.m10 = (first.m10 * second.m00 + first.m11 * second.m10) / kFixedOne;
  out.m11 = (first.m10 * second.m01 + first.m11 * second.m11) / kFixedOne;
  out.m12 = (first.m10 * second.m02 + first.m11 * second.m12) / kFixedOne + first.m12;
  return out;
}

Rect union_rect(Rect left, Rect right) noexcept {
  if (left.empty()) return right;
  if (right.empty()) return left;
  return Rect{std::min(left.left, right.left), std::min(left.top, right.top),
              std::max(left.right, right.right), std::max(left.bottom, right.bottom)};
}

Rect intersect_rect(Rect left, Rect right) noexcept {
  Rect result{std::max(left.left, right.left), std::max(left.top, right.top),
              std::min(left.right, right.right), std::min(left.bottom, right.bottom)};
  if (result.empty()) return {};
  return result;
}

Rect bounds_of(const std::vector<Point>& points) noexcept {
  if (points.empty()) return {};
  Rect result{points.front().x, points.front().y, points.front().x, points.front().y};
  for (Point point : points) {
    result.left = std::min(result.left, point.x);
    result.top = std::min(result.top, point.y);
    result.right = std::max(result.right, point.x);
    result.bottom = std::max(result.bottom, point.y);
  }
  return result;
}

int64_t signed_area_twice(const std::vector<Point>& polygon) noexcept {
  if (polygon.size() < 3) return 0;
  int64_t area = 0;
  for (std::size_t index = 0; index < polygon.size(); ++index) {
    const Point current = polygon[index];
    const Point next = polygon[(index + 1U) % polygon.size()];
    area += static_cast<int64_t>(current.x) * next.y -
            static_cast<int64_t>(current.y) * next.x;
  }
  return area;
}

bool is_clockwise(const std::vector<Point>& polygon) noexcept {
  return signed_area_twice(polygon) < 0;
}

std::vector<Point> simplify_collinear(const std::vector<Point>& points,
                                      int32_t tolerance) {
  if (points.size() < 3) return points;
  const int64_t threshold = static_cast<int64_t>(std::max<int32_t>(0, tolerance));
  std::vector<Point> output;
  output.reserve(points.size());
  output.push_back(points.front());
  for (std::size_t index = 1; index + 1 < points.size(); ++index) {
    const int64_t bend = std::llabs(cross(output.back(), points[index], points[index + 1]));
    if (bend > threshold) output.push_back(points[index]);
  }
  output.push_back(points.back());
  return output;
}

std::vector<Point> clip_polygon_to_rect(const std::vector<Point>& polygon,
                                        Rect clip) {
  if (polygon.empty() || clip.empty()) return {};
  std::vector<Point> result = polygon;
  result = clip_edge(result, clip, Edge::Left);
  result = clip_edge(result, clip, Edge::Right);
  result = clip_edge(result, clip, Edge::Top);
  result = clip_edge(result, clip, Edge::Bottom);
  return result;
}

uint64_t geometry_digest(const std::vector<Point>& points, uint64_t seed) noexcept {
  uint64_t result = mix64(seed ^ points.size());
  for (Point point : points) {
    const uint64_t packed = static_cast<uint32_t>(point.x) |
                            (static_cast<uint64_t>(static_cast<uint32_t>(point.y)) << 32U);
    result = mix64(result ^ packed);
  }
  return result;
}

}  // namespace vellumgrid
