#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vellumgrid {

struct Point {
  int32_t x{};
  int32_t y{};
};

struct Font {
  uint32_t id{};
  std::string family;
  std::vector<int16_t> axes;
  std::vector<uint16_t> hints;
  uint32_t revision{};
};

struct Style {
  uint32_t id{};
  uint32_t font_id{};
  Font* font{};
  uint8_t axis_index{};
  uint8_t flags{};
  uint32_t rgba{};
};

struct TextSegment {
  std::string bytes;
  uint32_t revision{};
};

struct TextObject {
  uint32_t id{};
  uint32_t style_id{};
  Style* style{};
  std::vector<std::unique_ptr<TextSegment>> segments;
  TextSegment* cursor{};
  std::vector<uint32_t> glyphs;
  std::vector<uint16_t> clusters;
  uint32_t shape_revision{};
};

struct ImageResource {
  uint32_t id{};
  uint32_t width{};
  uint32_t height{};
  std::vector<uint8_t> pixels;
  std::vector<uint8_t> coverage;
  const uint8_t* atlas_view{};
  std::size_t atlas_length{};
  uint32_t revision{};
};

struct PathResource {
  uint32_t id{};
  std::vector<Point> outer;
  std::vector<Point> hole;
  std::vector<uint32_t> triangles;
  uint32_t revision{};
};

struct Layer {
  uint32_t id{};
  uint8_t kind{};
  uint8_t blend{};
  uint32_t resource_id{};
  std::vector<Layer> children;
};

struct Page {
  uint32_t id{};
  uint32_t revision{};
  std::vector<Layer> layers;
};

struct Annotation {
  uint32_t id{};
  uint32_t page_id{};
  uint32_t layer_id{};
  Layer* target{};
  std::string note;
};

struct RenderOp {
  uint32_t layer_id{};
  uint32_t resource_id{};
  uint8_t kind{};
  uint8_t flags{};
};

}  // namespace vellumgrid
