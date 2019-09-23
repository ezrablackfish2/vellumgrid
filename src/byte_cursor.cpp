#include "vellumgrid/byte_cursor.h"

#include <algorithm>

namespace vellumgrid {

ByteCursor::ByteCursor(const uint8_t* data, std::size_t size) noexcept
    : data_(data), size_(size), pos_(0) {}

std::size_t ByteCursor::remaining() const noexcept { return size_ - pos_; }
std::size_t ByteCursor::offset() const noexcept { return pos_; }
const uint8_t* ByteCursor::current() const noexcept { return data_ + pos_; }

bool ByteCursor::skip(std::size_t count) noexcept {
  if (count > remaining()) return false;
  pos_ += count;
  return true;
}

std::optional<uint8_t> ByteCursor::u8() noexcept {
  if (remaining() < 1) return std::nullopt;
  return data_[pos_++];
}

std::optional<uint16_t> ByteCursor::u16() noexcept {
  if (remaining() < 2) return std::nullopt;
  const uint16_t value = static_cast<uint16_t>(data_[pos_]) |
                         (static_cast<uint16_t>(data_[pos_ + 1]) << 8U);
  pos_ += 2;
  return value;
}

std::optional<uint32_t> ByteCursor::u32() noexcept {
  if (remaining() < 4) return std::nullopt;
  const uint32_t value = static_cast<uint32_t>(data_[pos_]) |
                         (static_cast<uint32_t>(data_[pos_ + 1]) << 8U) |
                         (static_cast<uint32_t>(data_[pos_ + 2]) << 16U) |
                         (static_cast<uint32_t>(data_[pos_ + 3]) << 24U);
  pos_ += 4;
  return value;
}

std::optional<int16_t> ByteCursor::i16() noexcept {
  const auto value = u16();
  if (!value) return std::nullopt;
  return static_cast<int16_t>(*value);
}

std::optional<std::vector<uint8_t>> ByteCursor::bytes(std::size_t count) {
  if (count > remaining()) return std::nullopt;
  std::vector<uint8_t> result(data_ + pos_, data_ + pos_ + count);
  pos_ += count;
  return result;
}

std::optional<std::string> ByteCursor::text(std::size_t count) {
  if (count > remaining()) return std::nullopt;
  std::string result(reinterpret_cast<const char*>(data_ + pos_), count);
  pos_ += count;
  return result;
}

uint32_t mix32(uint32_t value) noexcept {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return value;
}

uint64_t mix64(uint64_t value) noexcept {
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return value;
}

}  // namespace vellumgrid
