#pragma once

#include "vellumgrid/model.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace vellumgrid {

struct Rect {
  int32_t left{};
  int32_t top{};
  int32_t right{};
  int32_t bottom{};

  bool empty() const noexcept;
  int64_t width() const noexcept;
  int64_t height() const noexcept;
  bool contains(Point point) const noexcept;
};

struct AffineTransform {
  int64_t m00{1 << 16};
  int64_t m01{};
  int64_t m02{};
  int64_t m10{};
  int64_t m11{1 << 16};
  int64_t m12{};

  static AffineTransform identity() noexcept;
  static AffineTransform translation(int32_t x, int32_t y) noexcept;
  static AffineTransform scale(int32_t x_16_16, int32_t y_16_16) noexcept;
  Point apply(Point point) const noexcept;
};

AffineTransform compose(const AffineTransform& first,
                        const AffineTransform& second) noexcept;
Rect union_rect(Rect left, Rect right) noexcept;
Rect intersect_rect(Rect left, Rect right) noexcept;
Rect bounds_of(const std::vector<Point>& points) noexcept;
int64_t signed_area_twice(const std::vector<Point>& polygon) noexcept;
bool is_clockwise(const std::vector<Point>& polygon) noexcept;
std::vector<Point> simplify_collinear(const std::vector<Point>& points,
                                      int32_t tolerance);
std::vector<Point> clip_polygon_to_rect(const std::vector<Point>& polygon,
                                        Rect clip);
uint64_t geometry_digest(const std::vector<Point>& points, uint64_t seed) noexcept;

}  // namespace vellumgrid
