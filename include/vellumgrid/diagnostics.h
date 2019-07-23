#pragma once

#include "vellumgrid/model.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vellumgrid {

enum class Severity : uint8_t { Note = 0, Warning = 1, Error = 2 };

struct Diagnostic {
  Severity severity{Severity::Note};
  uint32_t code{};
  uint32_t object_id{};
  std::string message;
};

class DiagnosticSink {
 public:
  void note(uint32_t code, uint32_t object_id, std::string message);
  void warning(uint32_t code, uint32_t object_id, std::string message);
  void error(uint32_t code, uint32_t object_id, std::string message);
  std::size_t count(Severity severity) const noexcept;
  bool has_errors() const noexcept;
  uint64_t digest(uint64_t seed) const noexcept;
  const std::vector<Diagnostic>& entries() const noexcept;
  void clear() noexcept;

 private:
  void append(Severity severity, uint32_t code, uint32_t object_id,
              std::string message);
  std::vector<Diagnostic> entries_;
};

void inspect_page(const Page& page, DiagnosticSink& sink);
void inspect_text(const TextObject& text, DiagnosticSink& sink);
void inspect_image(const ImageResource& image, DiagnosticSink& sink);
void inspect_path(const PathResource& path, DiagnosticSink& sink);

}  // namespace vellumgrid
