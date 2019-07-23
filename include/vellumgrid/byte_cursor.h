#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vellumgrid {

class ByteCursor {
 public:
  ByteCursor(const uint8_t* data, std::size_t size) noexcept;

  std::size_t remaining() const noexcept;
  std::size_t offset() const noexcept;
  bool skip(std::size_t count) noexcept;
  std::optional<uint8_t> u8() noexcept;
  std::optional<uint16_t> u16() noexcept;
  std::optional<uint32_t> u32() noexcept;
  std::optional<int16_t> i16() noexcept;
  std::optional<std::vector<uint8_t>> bytes(std::size_t count);
  std::optional<std::string> text(std::size_t count);
  const uint8_t* current() const noexcept;

 private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_;
};

uint32_t mix32(uint32_t value) noexcept;
uint64_t mix64(uint64_t value) noexcept;

}  // namespace vellumgrid
