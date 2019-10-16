#include "vellumgrid/job_format.h"

#include "vellumgrid/byte_cursor.h"

#include <array>
#include <limits>

namespace vellumgrid {
namespace {
constexpr std::array<uint8_t, 4> kMagic{{'V', 'G', 'D', '2'}};
constexpr std::size_t kMaxCommands = 4096;
constexpr std::size_t kMaxPayload = 16384;
}

std::optional<JobBatch> read_job_batch(const uint8_t* data, std::size_t size) {
  if (data == nullptr || size < 16) return std::nullopt;
  ByteCursor cursor(data, size);
  for (uint8_t expected : kMagic) {
    const auto actual = cursor.u8();
    if (!actual || *actual != expected) return std::nullopt;
  }
  const auto version = cursor.u16();
  const auto flags = cursor.u16();
  const auto count = cursor.u32();
  const auto nonce = cursor.u32();
  if (!version || !flags || !count || !nonce) return std::nullopt;
  if (*version != 2 || *count > kMaxCommands) return std::nullopt;

  JobBatch batch;
  batch.version = *version;
  batch.flags = *flags;
  batch.nonce = *nonce;
  batch.commands.reserve(*count);

  for (uint32_t index = 0; index < *count; ++index) {
    const auto opcode = cursor.u8();
    const auto command_flags = cursor.u8();
    const auto length = cursor.u16();
    const auto epoch = cursor.u32();
    if (!opcode || !command_flags || !length || !epoch) return std::nullopt;
    if (*length > kMaxPayload || *length > cursor.remaining()) return std::nullopt;
    auto payload = cursor.bytes(*length);
    if (!payload) return std::nullopt;
    Command command;
    command.opcode = static_cast<Opcode>(*opcode);
    command.flags = *command_flags;
    command.epoch = *epoch;
    command.payload = std::move(*payload);
    batch.commands.push_back(std::move(command));
  }
  return batch;
}

}  // namespace vellumgrid
