#include "vellumgrid/diagnostics.h"

#include "vellumgrid/byte_cursor.h"
#include "vellumgrid/layout_math.h"

#include <algorithm>

namespace vellumgrid {

void DiagnosticSink::append(Severity severity, uint32_t code, uint32_t object_id,
                            std::string message) {
  if (message.size() > 256) message.resize(256);
  entries_.push_back(Diagnostic{severity, code, object_id, std::move(message)});
}

void DiagnosticSink::note(uint32_t code, uint32_t object_id, std::string message) {
  append(Severity::Note, code, object_id, std::move(message));
}
void DiagnosticSink::warning(uint32_t code, uint32_t object_id, std::string message) {
  append(Severity::Warning, code, object_id, std::move(message));
}
void DiagnosticSink::error(uint32_t code, uint32_t object_id, std::string message) {
  append(Severity::Error, code, object_id, std::move(message));
}
std::size_t DiagnosticSink::count(Severity severity) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [severity](const Diagnostic& item) { return item.severity == severity; }));
}
bool DiagnosticSink::has_errors() const noexcept { return count(Severity::Error) != 0; }
uint64_t DiagnosticSink::digest(uint64_t seed) const noexcept {
  uint64_t result = mix64(seed ^ entries_.size());
  for (const Diagnostic& item : entries_) {
    result = mix64(result ^ item.code ^ item.object_id ^ static_cast<uint8_t>(item.severity));
    for (unsigned char ch : item.message) result = mix64(result ^ ch);
  }
  return result;
}
const std::vector<Diagnostic>& DiagnosticSink::entries() const noexcept { return entries_; }
void DiagnosticSink::clear() noexcept { entries_.clear(); }

void inspect_page(const Page& page, DiagnosticSink& sink) {
  if (page.layers.empty()) sink.note(1001, page.id, "page has no drawable layers");
  std::vector<uint32_t> ids;
  for (const Layer& layer : page.layers) {
    if (std::find(ids.begin(), ids.end(), layer.id) != ids.end()) {
      sink.error(1002, layer.id, "duplicate top-level layer identifier");
    }
    ids.push_back(layer.id);
    if (layer.kind > 8) sink.warning(1003, layer.id, "unknown layer kind");
    if (layer.children.size() > 64) sink.warning(1004, layer.id, "large child layer fanout");
  }
}

void inspect_text(const TextObject& text, DiagnosticSink& sink) {
  if (text.segments.empty()) sink.warning(2001, text.id, "text object has no segments");
  std::size_t bytes = 0;
  for (const auto& segment : text.segments) {
    if (!segment) {
      sink.error(2002, text.id, "null text segment");
      continue;
    }
    bytes += segment->bytes.size();
  }
  if (bytes > 65536) sink.warning(2003, text.id, "text object exceeds preferred size");
  if (!text.glyphs.empty() && text.clusters.empty()) {
    sink.error(2004, text.id, "shaped text has no cluster map");
  }
}

void inspect_image(const ImageResource& image, DiagnosticSink& sink) {
  if (image.width == 0 || image.height == 0) sink.error(3001, image.id, "zero-sized image");
  const uint64_t area = static_cast<uint64_t>(image.width) * image.height;
  if (area > 1024ULL * 1024ULL) sink.warning(3002, image.id, "image exceeds tile budget");
  if (image.pixels.size() < std::min<uint64_t>(area * 4ULL, 1ULL << 20U)) {
    sink.warning(3003, image.id, "pixel storage is smaller than logical dimensions");
  }
}

void inspect_path(const PathResource& path, DiagnosticSink& sink) {
  if (path.outer.size() < 3) sink.error(4001, path.id, "path has fewer than three vertices");
  if (!path.outer.empty()) {
    Rect bounds = bounds_of(path.outer);
    if (bounds.width() > 1LL << 28 || bounds.height() > 1LL << 28) {
      sink.warning(4002, path.id, "path bounds exceed preferred coordinate range");
    }
    if (signed_area_twice(path.outer) == 0) sink.warning(4003, path.id, "path has zero area");
  }
  if (path.triangles.size() % 3 != 0) sink.error(4004, path.id, "triangle index count is invalid");
}

}  // namespace vellumgrid
