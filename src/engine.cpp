#include "vellumgrid/engine.h"

#include "vellumgrid/byte_cursor.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace vellumgrid {
namespace {

std::vector<int16_t> read_axes(ByteCursor& cursor, uint8_t count) {
  std::vector<int16_t> axes;
  axes.reserve(count);
  for (uint8_t index = 0; index < count; ++index) {
    const auto value = cursor.i16();
    if (!value) break;
    axes.push_back(*value);
  }
  return axes;
}

std::vector<Point> read_points(ByteCursor& cursor, uint8_t count) {
  std::vector<Point> points;
  points.reserve(count);
  for (uint8_t index = 0; index < count; ++index) {
    const auto x = cursor.i16();
    const auto y = cursor.i16();
    if (!x || !y) break;
    points.push_back(Point{*x, *y});
  }
  return points;
}

uint64_t combine(uint64_t current, uint64_t value) {
  return mix64(current ^ (value + 0x9e3779b97f4a7c15ULL + (current << 6U) +
                          (current >> 2U)));
}

}  // namespace

DocumentEngine::DocumentEngine() : digest_(0xcbf29ce484222325ULL) {}

void DocumentEngine::begin_document(uint32_t id, uint32_t nonce) {
  document_id_ = id;
  last_epoch_ = 0;
  active_ = true;
  digest_ = mix64(static_cast<uint64_t>(id) << 32U | nonce);
}

bool DocumentEngine::accepts(const Command& command) const noexcept {
  if (command.opcode == Opcode::BeginDocument) return true;
  if (!active_) return false;
  return command.epoch >= last_epoch_ && command.payload.size() <= 16384;
}

void DocumentEngine::apply(const Command& command, uint32_t batch_nonce) {
  if (!accepts(command)) return;
  ByteCursor cursor(command.payload.data(), command.payload.size());
  if (command.opcode != Opcode::BeginDocument) last_epoch_ = command.epoch;
  digest_ = combine(digest_, static_cast<uint8_t>(command.opcode));
  digest_ = combine(digest_, command.epoch);
  digest_ = combine(digest_, command.flags);

  switch (command.opcode) {
    case Opcode::BeginDocument: {
      const auto id = cursor.u32();
      if (id) begin_document(*id, batch_nonce);
      break;
    }
    case Opcode::AddFont: {
      const auto id = cursor.u32();
      const auto count = cursor.u8();
      if (!id || !count || *count > 32) break;
      auto axes = read_axes(cursor, *count);
      const auto name_length = cursor.u8();
      if (!name_length || *name_length > cursor.remaining()) break;
      auto family = cursor.text(*name_length);
      if (family && axes.size() == *count) {
        Font* font = fonts_.add(*id, std::move(*family), std::move(axes));
        digest_ = combine(digest_, font ? font->revision : 0);
      }
      break;
    }
    case Opcode::AddStyle: {
      const auto id = cursor.u32();
      const auto font_id = cursor.u32();
      const auto axis = cursor.u8();
      const auto flags = cursor.u8();
      const auto rgba = cursor.u32();
      if (id && font_id && axis && flags && rgba) {
        Style* style = styles_.add(*id, *font_id, fonts_.find(*font_id), *axis, *flags, *rgba);
        digest_ = combine(digest_, style ? style->id : 0);
      }
      break;
    }
    case Opcode::AddPage: {
      const auto id = cursor.u32();
      if (id) {
        Page* page = pages_.add(*id);
        digest_ = combine(digest_, page ? page->id : 0);
      }
      break;
    }
    case Opcode::AddLayer: {
      const auto page_id = cursor.u32();
      const auto layer_id = cursor.u32();
      const auto kind = cursor.u8();
      const auto blend = cursor.u8();
      const auto resource = cursor.u32();
      const auto child_count = cursor.u8();
      if (!page_id || !layer_id || !kind || !blend || !resource || !child_count ||
          *child_count > 16) {
        break;
      }
      Layer layer;
      layer.id = *layer_id;
      layer.kind = *kind;
      layer.blend = *blend;
      layer.resource_id = *resource;
      for (uint8_t index = 0; index < *child_count; ++index) {
        const auto child_id = cursor.u32();
        const auto child_kind = cursor.u8();
        const auto child_blend = cursor.u8();
        const auto child_resource = cursor.u32();
        if (!child_id || !child_kind || !child_blend || !child_resource) break;
        layer.children.push_back(Layer{*child_id, *child_kind, *child_blend, *child_resource, {}});
      }
      Layer* added = pages_.add_layer(*page_id, std::move(layer));
      digest_ = combine(digest_, added ? added->id : 0);
      break;
    }
    case Opcode::AddText: {
      const auto id = cursor.u32();
      const auto style_id = cursor.u32();
      const auto count = cursor.u8();
      if (!id || !style_id || !count || *count > 32) break;
      std::vector<std::string> segments;
      for (uint8_t index = 0; index < *count; ++index) {
        const auto length = cursor.u8();
        if (!length || *length > cursor.remaining()) break;
        auto text = cursor.text(*length);
        if (!text) break;
        segments.push_back(std::move(*text));
      }
      if (segments.size() == *count) {
        TextObject* text = texts_.add(*id, *style_id, styles_.find(*style_id), std::move(segments));
        digest_ = combine(digest_, text ? text->id : 0);
      }
      break;
    }
    case Opcode::AddImage: {
      const auto id = cursor.u32();
      const auto width = cursor.u16();
      const auto height = cursor.u16();
      const auto fill = cursor.u8();
      if (id && width && height && fill) {
        ImageResource* image = images_.add(*id, *width, *height, *fill);
        digest_ = combine(digest_, image ? image->pixels.size() : 0);
      }
      break;
    }
    case Opcode::AddPath: {
      const auto id = cursor.u32();
      const auto outer_count = cursor.u8();
      if (!id || !outer_count || *outer_count > 64) break;
      auto outer = read_points(cursor, *outer_count);
      const auto hole_count = cursor.u8();
      if (!hole_count || *hole_count > 64) break;
      auto hole = read_points(cursor, *hole_count);
      if (outer.size() == *outer_count && hole.size() == *hole_count) {
        PathResource* path = paths_.add(*id, std::move(outer), std::move(hole));
        digest_ = combine(digest_, path ? path->outer.size() : 0);
      }
      break;
    }
    case Opcode::AddProfile: {
      const auto id = cursor.u32();
      const auto length = cursor.u16();
      if (!id || !length || *length > cursor.remaining()) break;
      auto bytes = cursor.bytes(*length);
      if (bytes) profiles_.add(*id, *bytes);
      break;
    }
    case Opcode::AddAnnotation: {
      const auto id = cursor.u32();
      const auto page_id = cursor.u32();
      const auto layer_id = cursor.u32();
      const auto length = cursor.u8();
      if (!id || !page_id || !layer_id || !length || *length > cursor.remaining()) break;
      auto note = cursor.text(*length);
      if (note) {
        annotations_.add(*id, *page_id, *layer_id,
                         pages_.find_layer(*page_id, *layer_id), std::move(*note));
      }
      break;
    }
    case Opcode::CacheSpread: {
      const auto first = cursor.u32();
      const auto second = cursor.u32();
      if (first && second) pages_.cache_spread(*first, *second);
      break;
    }
    case Opcode::GrowStyles: {
      const auto count = cursor.u16();
      if (count) styles_.grow(*count, fonts_, batch_nonce ^ command.epoch);
      break;
    }
    case Opcode::ReplaceFont: {
      const auto id = cursor.u32();
      const auto mode = cursor.u8();
      const auto count = cursor.u8();
      if (!id || !mode || !count || *count > 32) break;
      auto axes = read_axes(cursor, *count);
      if (axes.size() == *count) {
        fonts_.replace(*id, std::move(axes), *mode);
        if (*mode == 0) styles_.refresh_font(*id, fonts_.find(*id));
      }
      break;
    }
    case Opcode::ShapeText: {
      const auto id = cursor.u32();
      const auto mode = cursor.u8();
      if (id && mode) texts_.shape(*id, *mode, batch_nonce ^ command.epoch);
      digest_ = combine(digest_, texts_.checksum());
      break;
    }
    case Opcode::CompactText: {
      const auto id = cursor.u32();
      if (id) texts_.compact(*id);
      break;
    }
    case Opcode::EditCursor: {
      const auto id = cursor.u32();
      const auto value = cursor.u8();
      if (id && value) texts_.edit_cursor(*id, *value);
      break;
    }
    case Opcode::RemovePage: {
      const auto id = cursor.u32();
      if (id) pages_.remove_page(*id);
      break;
    }
    case Opcode::RenderSpread:
      digest_ = combine(digest_, pages_.render_spread(batch_nonce));
      break;
    case Opcode::SplitPage: {
      const auto id = cursor.u32();
      const auto created = cursor.u32();
      if (id && created) pages_.split_page(*id, *created);
      break;
    }
    case Opcode::ResolveAnnotation: {
      const auto id = cursor.u32();
      if (id) digest_ = combine(digest_, annotations_.resolve(*id, batch_nonce));
      break;
    }
    case Opcode::AtlasPack: {
      const auto id = cursor.u32();
      if (id) images_.atlas_pack(*id, batch_nonce ^ command.epoch);
      break;
    }
    case Opcode::AtlasRepack:
      images_.atlas_repack(batch_nonce ^ command.epoch);
      break;
    case Opcode::RenderImage: {
      const auto id = cursor.u32();
      if (id) digest_ = combine(digest_, images_.render_from_atlas(*id, batch_nonce));
      break;
    }
    case Opcode::CropImage: {
      const auto id = cursor.u32();
      const auto width = cursor.u16();
      const auto height = cursor.u16();
      if (id && width && height) images_.crop(*id, *width, *height);
      break;
    }
    case Opcode::ApplyMask: {
      const auto id = cursor.u32();
      const auto factor = cursor.u8();
      if (id && factor) images_.apply_mask(*id, *factor);
      break;
    }
    case Opcode::TessellatePath: {
      const auto id = cursor.u32();
      const auto winding = cursor.u8();
      if (id && winding) paths_.tessellate(*id, *winding);
      break;
    }
    case Opcode::PlanRender: {
      const auto page_id = cursor.u32();
      const auto mode = cursor.u8();
      const Page* page = page_id ? pages_.find(*page_id) : nullptr;
      if (page && mode) digest_ = combine(digest_, planner_.plan(*page, *mode, batch_nonce));
      break;
    }
    case Opcode::CacheInsert: {
      const auto key = cursor.u32();
      const auto page_id = cursor.u32();
      const auto low = cursor.u32();
      const auto high = cursor.u32();
      if (key && page_id && low && high) {
        const uint64_t value = static_cast<uint64_t>(*low) | (static_cast<uint64_t>(*high) << 32U);
        render_cache_.insert(*key, *page_id, value);
      }
      break;
    }
    case Opcode::CachePurge: {
      const auto key = cursor.u32();
      if (key) render_cache_.purge(*key);
      break;
    }
    case Opcode::CacheTouch: {
      const auto key = cursor.u32();
      if (key) digest_ = combine(digest_, render_cache_.touch(*key, batch_nonce));
      break;
    }
    case Opcode::BeginTransaction: {
      const auto id = cursor.u32();
      if (id) transactions_.begin(*id);
      break;
    }
    case Opcode::TrackImage: {
      const auto id = cursor.u32();
      if (id) transactions_.track_image(images_.find(*id));
      break;
    }
    case Opcode::DeleteImage: {
      const auto id = cursor.u32();
      if (id) images_.erase(*id);
      break;
    }
    case Opcode::UndoTransaction:
      transactions_.undo();
      break;
    case Opcode::ReplaceProfile: {
      const auto id = cursor.u32();
      const auto length = cursor.u16();
      if (!id || !length || *length > cursor.remaining()) break;
      auto bytes = cursor.bytes(*length);
      if (bytes) profiles_.replace(*id, *bytes);
      break;
    }
    case Opcode::RollbackProfile: {
      const auto id = cursor.u32();
      if (id) profiles_.rollback(*id);
      break;
    }
    case Opcode::SchedulePage: {
      const auto page_id = cursor.u32();
      const auto nonce = cursor.u32();
      const auto priority = cursor.u8();
      const Page* page = page_id ? pages_.find(*page_id) : nullptr;
      if (page && nonce && priority) scheduler_.schedule(*page, *nonce, *priority);
      break;
    }
    case Opcode::FlushScheduler:
      digest_ = combine(digest_, scheduler_.flush(batch_nonce));
      break;
    case Opcode::AllocateBitmapSlot: {
      const auto length = cursor.u16();
      const auto fill = cursor.u8();
      if (length && fill) digest_ = combine(digest_, resource_pool_.allocate_bitmap(*length, *fill));
      break;
    }
    case Opcode::ReleaseSlot: {
      const auto handle = cursor.u32();
      if (handle) resource_pool_.release(*handle);
      break;
    }
    case Opcode::AllocateGradientSlot: {
      const auto stops = cursor.u8();
      if (stops) digest_ = combine(digest_, resource_pool_.allocate_gradient(*stops));
      break;
    }
    case Opcode::ProbeStaleHandle: {
      const auto handle = cursor.u32();
      if (handle) digest_ = combine(digest_, resource_pool_.probe_bitmap(*handle));
      break;
    }
    case Opcode::CopyFragment: {
      const auto style_id = cursor.u32();
      const auto count = cursor.u8();
      if (!style_id || !count || *count > 32) break;
      std::vector<uint32_t> resources;
      for (uint8_t index = 0; index < *count; ++index) {
        const auto resource = cursor.u32();
        if (!resource) break;
        resources.push_back(*resource);
      }
      const auto length = cursor.u8();
      if (!length || *length > cursor.remaining() || resources.size() != *count) break;
      auto text = cursor.text(*length);
      if (text) clipboard_.copy(*style_id, styles_, std::move(resources), std::move(*text));
      break;
    }
    case Opcode::ReorderStyles: {
      const auto keep = cursor.u16();
      if (keep) styles_.reorder_and_trim(*keep);
      break;
    }
    case Opcode::PasteFragment:
      digest_ = combine(digest_, clipboard_.paste(styles_, batch_nonce));
      break;
    case Opcode::BuildDependency: {
      const auto root = cursor.u32();
      const auto count = cursor.u8();
      if (!root || !count || *count > 64) break;
      std::vector<uint32_t> children;
      for (uint8_t index = 0; index < *count; ++index) {
        const auto child = cursor.u32();
        if (!child) break;
        children.push_back(*child);
      }
      if (children.size() == *count) dependencies_.build(*root, children);
      break;
    }
    case Opcode::CompactDependency: {
      const auto victim = cursor.u32();
      if (victim) dependencies_.compact(*victim);
      break;
    }
    case Opcode::VisitDependency:
      digest_ = combine(digest_, dependencies_.visit_selected(batch_nonce));
      break;
    case Opcode::MoveLayer: {
      const auto from = cursor.u32();
      const auto to = cursor.u32();
      const auto layer = cursor.u32();
      if (from && to && layer) {
        pages_.remember_layer(*layer);
        pages_.move_layer(*from, *to, *layer);
      }
      break;
    }
    case Opcode::TouchMovedLayer:
      digest_ = combine(digest_, pages_.touch_remembered(batch_nonce));
      break;
    case Opcode::CacheMesh: {
      const auto key = cursor.u32();
      const auto path_id = cursor.u32();
      if (key && path_id) mesh_cache_.cache(*key, paths_.find(*path_id));
      break;
    }
    case Opcode::PurgePath: {
      const auto path_id = cursor.u32();
      if (path_id) paths_.erase(*path_id);
      break;
    }
    case Opcode::DrawCachedMesh: {
      const auto key = cursor.u32();
      if (key) digest_ = combine(digest_, mesh_cache_.draw(*key, batch_nonce));
      break;
    }
    case Opcode::FinalizeDocument:
      digest_ = combine(digest_, render_all(batch_nonce));
      break;
    default:
      break;
  }
}

uint64_t DocumentEngine::render_all(uint32_t salt) {
  uint64_t result = combine(digest_, salt);
  for (const Page& page : pages_.pages()) {
    result = combine(result, planner_.plan(page, 0, salt ^ page.id));
  }
  result = combine(result, texts_.checksum());
  digest_ = result;
  return result;
}

uint64_t DocumentEngine::replay_pending(uint32_t salt) {
  uint64_t result = combine(digest_, pages_.render_spread(salt));
  result = combine(result, dependencies_.visit_selected(salt ^ document_id_));
  digest_ = result;
  return result;
}

uint64_t DocumentEngine::pressure_cycle(uint32_t salt) {
  images_.atlas_repack(salt);
  uint64_t result = combine(digest_, salt ^ planner_.last_count());
  for (const Page& page : pages_.pages()) {
    result = combine(result, planner_.plan(page, 0, salt));
  }
  digest_ = result;
  return result;
}

uint64_t DocumentEngine::digest() const noexcept { return digest_; }

uint64_t run_job(const JobBatch& batch, uint8_t mode) {
  DocumentEngine engine;
  for (const Command& command : batch.commands) engine.apply(command, batch.nonce);
  if (mode == 1) engine.render_all(batch.nonce ^ batch.flags);
  if (mode == 2) engine.replay_pending(batch.nonce ^ batch.flags);
  if (mode == 3) engine.pressure_cycle(batch.nonce ^ batch.flags);
  return engine.digest();
}

}  // namespace vellumgrid
