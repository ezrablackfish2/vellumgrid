#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace vellumgrid {

enum class Opcode : uint8_t {
  BeginDocument = 1,
  AddFont = 2,
  AddStyle = 3,
  AddPage = 4,
  AddLayer = 5,
  AddText = 6,
  AddImage = 7,
  AddPath = 8,
  AddProfile = 9,
  AddAnnotation = 10,
  CacheSpread = 11,
  GrowStyles = 12,
  ReplaceFont = 13,
  ShapeText = 14,
  CompactText = 15,
  EditCursor = 16,
  RemovePage = 17,
  RenderSpread = 18,
  SplitPage = 19,
  ResolveAnnotation = 20,
  AtlasPack = 21,
  AtlasRepack = 22,
  RenderImage = 23,
  CropImage = 24,
  ApplyMask = 25,
  TessellatePath = 26,
  PlanRender = 27,
  CacheInsert = 28,
  CachePurge = 29,
  CacheTouch = 30,
  BeginTransaction = 31,
  TrackImage = 32,
  DeleteImage = 33,
  UndoTransaction = 34,
  ReplaceProfile = 35,
  RollbackProfile = 36,
  SchedulePage = 37,
  FlushScheduler = 38,
  AllocateBitmapSlot = 39,
  ReleaseSlot = 40,
  AllocateGradientSlot = 41,
  ProbeStaleHandle = 42,
  CopyFragment = 43,
  ReorderStyles = 44,
  PasteFragment = 45,
  BuildDependency = 46,
  CompactDependency = 47,
  VisitDependency = 48,
  MoveLayer = 49,
  TouchMovedLayer = 50,
  CacheMesh = 51,
  PurgePath = 52,
  DrawCachedMesh = 53,
  FinalizeDocument = 54,
};

struct Command {
  Opcode opcode{};
  uint8_t flags{};
  uint32_t epoch{};
  std::vector<uint8_t> payload;
};

struct JobBatch {
  uint16_t version{};
  uint16_t flags{};
  uint32_t nonce{};
  std::vector<Command> commands;
};

std::optional<JobBatch> read_job_batch(const uint8_t* data, std::size_t size);

}  // namespace vellumgrid
