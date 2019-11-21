#include "vellumgrid/subsystems.h"

#include "vellumgrid/byte_cursor.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace vellumgrid {
namespace {

template <typename Container>
typename Container::value_type* find_by_id(Container& values, uint32_t id) {
  using Value = typename Container::value_type;
  auto it = std::find_if(values.begin(), values.end(),
                         [id](const Value& value) { return value.id == id; });
  return it == values.end() ? nullptr : &*it;
}

template <typename Container>
const typename Container::value_type* find_by_id(const Container& values, uint32_t id) {
  using Value = typename Container::value_type;
  auto it = std::find_if(values.begin(), values.end(),
                         [id](const Value& value) { return value.id == id; });
  return it == values.end() ? nullptr : &*it;
}

uint64_t fold_bytes(const uint8_t* data, std::size_t size, uint64_t seed) {
  uint64_t result = seed ^ (size * 0x9e3779b97f4a7c15ULL);
  for (std::size_t index = 0; index < size; ++index) {
    result ^= static_cast<uint64_t>(data[index]) + 0x9e3779b97f4a7c15ULL +
              (result << 6U) + (result >> 2U);
    result = mix64(result);
  }
  return result;
}

Layer* find_layer_recursive(std::vector<Layer>& layers, uint32_t id) {
  for (Layer& layer : layers) {
    if (layer.id == id) return &layer;
    if (Layer* child = find_layer_recursive(layer.children, id)) return child;
  }
  return nullptr;
}

const Layer* find_layer_recursive(const std::vector<Layer>& layers, uint32_t id) {
  for (const Layer& layer : layers) {
    if (layer.id == id) return &layer;
    if (const Layer* child = find_layer_recursive(layer.children, id)) return child;
  }
  return nullptr;
}

std::size_t bounded_area(uint32_t width, uint32_t height) {
  const uint64_t area = static_cast<uint64_t>(width) * height;
  return static_cast<std::size_t>(std::min<uint64_t>(area, 1U << 20U));
}

}  // namespace

FontBook::FontBook() { fonts_.reserve(2); }

Font* FontBook::add(uint32_t id, std::string family, std::vector<int16_t> axes) {
  if (Font* existing = find(id)) return existing;
  Font font;
  font.id = id;
  font.family = std::move(family);
  font.axes = std::move(axes);
  font.hints.reserve(font.axes.size());
  for (std::size_t index = 0; index < font.axes.size(); ++index) {
    const uint32_t mixed = mix32(id ^ static_cast<uint32_t>(index * 131U));
    font.hints.push_back(static_cast<uint16_t>((mixed & 0x7fffU) + 1U));
  }
  font.revision = 1;
  fonts_.push_back(std::move(font));
  return &fonts_.back();
}

Font* FontBook::find(uint32_t id) { return find_by_id(fonts_, id); }
const Font* FontBook::find(uint32_t id) const { return find_by_id(fonts_, id); }

void FontBook::replace(uint32_t id, std::vector<int16_t> axes, uint8_t mode) {
  Font* current = find(id);
  if (current == nullptr) return;
  if (mode == 0) {
    current->axes = std::move(axes);
    current->hints.clear();
    current->hints.shrink_to_fit();
    for (std::size_t index = 0; index < current->axes.size(); ++index) {
      current->hints.push_back(static_cast<uint16_t>(mix32(id + index) & 0xffffU));
    }
    ++current->revision;
    return;
  }

  Font replacement;
  replacement.id = id;
  replacement.family = current->family;
  replacement.axes = std::move(axes);
  replacement.revision = current->revision + 1U;
  replacement.hints.reserve(replacement.axes.size());
  for (std::size_t index = 0; index < replacement.axes.size(); ++index) {
    replacement.hints.push_back(
        static_cast<uint16_t>((mix32(id ^ static_cast<uint32_t>(index)) >> 3U) & 0xffffU));
  }
  fonts_.erase(std::remove_if(fonts_.begin(), fonts_.end(),
                              [id](const Font& font) { return font.id == id; }),
               fonts_.end());
  fonts_.shrink_to_fit();
  fonts_.push_back(std::move(replacement));
}

std::size_t FontBook::size() const noexcept { return fonts_.size(); }

StyleCatalog::StyleCatalog() { styles_.reserve(1); }

Style* StyleCatalog::add(uint32_t id, uint32_t font_id, Font* font,
                         uint8_t axis_index, uint8_t flags, uint32_t rgba) {
  if (Style* existing = find(id)) return existing;
  Style style;
  style.id = id;
  style.font_id = font_id;
  style.font = font;
  style.axis_index = axis_index;
  style.flags = flags;
  style.rgba = rgba;
  styles_.push_back(style);
  return &styles_.back();
}

Style* StyleCatalog::find(uint32_t id) { return find_by_id(styles_, id); }
const Style* StyleCatalog::find(uint32_t id) const { return find_by_id(styles_, id); }

void StyleCatalog::grow(uint32_t count, FontBook& fonts, uint32_t salt) {
  count = std::min<uint32_t>(count, 2048U);
  Font* fallback = fonts.size() == 0 ? nullptr : fonts.find(1);
  for (uint32_t index = 0; index < count; ++index) {
    Style style;
    style.id = 0x80000000U ^ mix32(salt + index * 17U);
    style.font_id = fallback ? fallback->id : 0;
    style.font = fallback;
    style.axis_index = static_cast<uint8_t>(index & 3U);
    style.flags = static_cast<uint8_t>(mix32(salt ^ index) & 0x3fU);
    style.rgba = mix32(style.id ^ salt);
    styles_.push_back(style);
  }
}

void StyleCatalog::reorder_and_trim(std::size_t keep) {
  std::stable_sort(styles_.begin(), styles_.end(), [](const Style& left, const Style& right) {
    const uint32_t lk = mix32(left.id ^ left.rgba);
    const uint32_t rk = mix32(right.id ^ right.rgba);
    if (lk != rk) return lk < rk;
    return left.id < right.id;
  });
  if (keep < styles_.size()) styles_.resize(keep);
  styles_.shrink_to_fit();
}

std::size_t StyleCatalog::ordinal_of(uint32_t id) const {
  for (std::size_t index = 0; index < styles_.size(); ++index) {
    if (styles_[index].id == id) return index;
  }
  return styles_.size();
}

Style* StyleCatalog::by_ordinal_unchecked(std::size_t ordinal) {
  return &styles_[ordinal];
}

void StyleCatalog::refresh_font(uint32_t font_id, Font* font) {
  for (Style& style : styles_) {
    if (style.font_id == font_id) style.font = font;
  }
}

std::size_t StyleCatalog::size() const noexcept { return styles_.size(); }

TextStore::TextStore() { texts_.reserve(8); }

TextObject* TextStore::add(uint32_t id, uint32_t style_id, Style* style,
                           std::vector<std::string> segments) {
  if (TextObject* existing = find(id)) return existing;
  TextObject text;
  text.id = id;
  text.style_id = style_id;
  text.style = style;
  text.segments.reserve(segments.size());
  for (std::string& bytes : segments) {
    auto segment = std::make_unique<TextSegment>();
    segment->bytes = std::move(bytes);
    segment->revision = 1;
    text.segments.push_back(std::move(segment));
  }
  if (!text.segments.empty()) text.cursor = text.segments.front().get();
  texts_.push_back(std::move(text));
  return &texts_.back();
}

TextObject* TextStore::find(uint32_t id) { return find_by_id(texts_, id); }

void TextStore::shape(uint32_t id, uint8_t mode, uint32_t salt) {
  TextObject* text = find(id);
  if (text == nullptr || text->style == nullptr) return;
  std::string joined;
  for (const auto& segment : text->segments) joined += segment->bytes;
  if (joined.empty()) return;

  const uint8_t flags = text->style->flags;
  text->glyphs.clear();
  for (std::size_t index = 0; index < joined.size(); ++index) {
    uint32_t glyph = static_cast<uint8_t>(joined[index]);
    glyph ^= mix32(salt + static_cast<uint32_t>(index) + flags) & 0x3ffU;
    text->glyphs.push_back(glyph);
    if (mode == 7 && index + 2 < joined.size() && joined[index] == 'f' &&
        joined[index + 1] == 'f' && joined[index + 2] == 'i') {
      text->glyphs.push_back(glyph ^ 0x155U);
      text->glyphs.push_back(glyph ^ 0x2aaU);
    }
  }

  text->clusters.assign(joined.size(), 0);
  text->clusters.shrink_to_fit();
  for (std::size_t index = 0; index < text->glyphs.size(); ++index) {
    text->clusters[index] = static_cast<uint16_t>((index * 3U + flags) & 0xffffU);
  }

  if (mode == 9 && text->style->font != nullptr) {
    const uint16_t hint = text->style->font->hints[text->style->axis_index];
    text->glyphs.front() ^= hint;
  }
  ++text->shape_revision;
}

void TextStore::compact(uint32_t id) {
  TextObject* text = find(id);
  if (text == nullptr || text->segments.size() < 2) return;
  std::string combined;
  for (const auto& segment : text->segments) combined += segment->bytes;
  text->segments.clear();
  text->segments.shrink_to_fit();
  auto merged = std::make_unique<TextSegment>();
  merged->bytes = std::move(combined);
  merged->revision = text->shape_revision + 1U;
  text->segments.push_back(std::move(merged));
}

void TextStore::edit_cursor(uint32_t id, uint8_t value) {
  TextObject* text = find(id);
  if (text == nullptr || text->cursor == nullptr) return;
  text->cursor->bytes.push_back(static_cast<char>(value));
  ++text->cursor->revision;
}

uint64_t TextStore::checksum() const noexcept {
  uint64_t result = 0x4d595df4d0f33173ULL;
  for (const TextObject& text : texts_) {
    result ^= mix64(text.id + text.shape_revision);
    for (const auto& segment : text.segments) {
      result = fold_bytes(reinterpret_cast<const uint8_t*>(segment->bytes.data()),
                          segment->bytes.size(), result);
    }
  }
  return result;
}

PageBook::PageBook() { pages_.reserve(2); }

Page* PageBook::add(uint32_t id) {
  if (Page* existing = find(id)) return existing;
  Page page;
  page.id = id;
  page.revision = 1;
  page.layers.reserve(2);
  pages_.push_back(std::move(page));
  return &pages_.back();
}

Page* PageBook::find(uint32_t id) { return find_by_id(pages_, id); }
const Page* PageBook::find(uint32_t id) const { return find_by_id(pages_, id); }

Layer* PageBook::add_layer(uint32_t page_id, Layer layer) {
  Page* page = find(page_id);
  if (page == nullptr) return nullptr;
  page->layers.push_back(std::move(layer));
  ++page->revision;
  return &page->layers.back();
}

Layer* PageBook::find_layer(uint32_t page_id, uint32_t layer_id) {
  Page* page = find(page_id);
  return page ? find_layer_recursive(page->layers, layer_id) : nullptr;
}

Layer* PageBook::find_layer_any(uint32_t layer_id) {
  for (Page& page : pages_) {
    if (Layer* layer = find_layer_recursive(page.layers, layer_id)) return layer;
  }
  return nullptr;
}

void PageBook::cache_spread(uint32_t first_page, uint32_t second_page) {
  spread_cache_.clear();
  if (Page* first = find(first_page)) spread_cache_.push_back(first);
  if (Page* second = find(second_page)) spread_cache_.push_back(second);
}

void PageBook::remove_page(uint32_t page_id) {
  pages_.erase(std::remove_if(pages_.begin(), pages_.end(),
                              [page_id](const Page& page) { return page.id == page_id; }),
               pages_.end());
  pages_.shrink_to_fit();
}

uint64_t PageBook::render_spread(uint32_t salt) const {
  uint64_t result = mix64(salt ^ 0x737072656164ULL);
  for (const Page* page : spread_cache_) {
    result ^= mix64(page->id + page->revision + page->layers.size());
    for (const Layer& layer : page->layers) {
      result = mix64(result ^ layer.id ^ layer.resource_id);
    }
  }
  return result;
}

void PageBook::split_page(uint32_t page_id, uint32_t new_page_id) {
  Page* page = find(page_id);
  if (page == nullptr || page->layers.empty()) return;
  const std::size_t pivot = std::max<std::size_t>(1, page->layers.size() / 2U);
  std::vector<Layer> moved;
  for (std::size_t index = pivot; index < page->layers.size(); ++index) {
    moved.push_back(std::move(page->layers[index]));
  }
  page->layers.resize(pivot);
  page->layers.shrink_to_fit();
  ++page->revision;
  Page* created = add(new_page_id);
  if (created == nullptr) return;
  created->layers = std::move(moved);
  created->layers.shrink_to_fit();
  ++created->revision;
}

void PageBook::move_layer(uint32_t from_page, uint32_t to_page, uint32_t layer_id) {
  Page* source = find(from_page);
  Page* destination = find(to_page);
  if (source == nullptr || destination == nullptr) return;
  auto it = std::find_if(source->layers.begin(), source->layers.end(),
                         [layer_id](const Layer& layer) { return layer.id == layer_id; });
  if (it == source->layers.end()) return;
  Layer moved = std::move(*it);
  source->layers.erase(it);
  source->layers.shrink_to_fit();
  destination->layers.push_back(std::move(moved));
  destination->layers.shrink_to_fit();
  ++source->revision;
  ++destination->revision;
}

void PageBook::remember_layer(uint32_t layer_id) { remembered_layer_ = find_layer_any(layer_id); }

uint64_t PageBook::touch_remembered(uint32_t salt) const {
  if (remembered_layer_ == nullptr) return salt;
  uint64_t result = mix64(salt ^ remembered_layer_->id ^ remembered_layer_->resource_id);
  result ^= static_cast<uint64_t>(remembered_layer_->children.size()) << 32U;
  return result;
}

std::vector<Page>& PageBook::pages() noexcept { return pages_; }
const std::vector<Page>& PageBook::pages() const noexcept { return pages_; }

Annotation* AnnotationMap::add(uint32_t id, uint32_t page_id, uint32_t layer_id,
                               Layer* target, std::string note) {
  Annotation annotation;
  annotation.id = id;
  annotation.page_id = page_id;
  annotation.layer_id = layer_id;
  annotation.target = target;
  annotation.note = std::move(note);
  annotations_.push_back(std::move(annotation));
  return &annotations_.back();
}

uint64_t AnnotationMap::resolve(uint32_t id, uint32_t salt) const {
  const Annotation* annotation = find_by_id(annotations_, id);
  if (annotation == nullptr || annotation->target == nullptr) return salt;
  uint64_t result = mix64(salt ^ annotation->target->id ^ annotation->target->resource_id);
  result = fold_bytes(reinterpret_cast<const uint8_t*>(annotation->note.data()),
                      annotation->note.size(), result);
  result ^= annotation->target->children.size();
  return result;
}

ImageResource* ImageStore::add(uint32_t id, uint32_t width, uint32_t height, uint8_t fill) {
  if (ImageResource* existing = find(id)) return existing;
  width = std::clamp<uint32_t>(width, 1, 512);
  height = std::clamp<uint32_t>(height, 1, 512);
  ImageResource image;
  image.id = id;
  image.width = width;
  image.height = height;
  const std::size_t area = bounded_area(width, height);
  image.pixels.resize(area * 4U, fill);
  image.coverage.resize(area, 0xffU);
  image.revision = 1;
  images_.push_back(std::move(image));
  return &images_.back();
}

ImageResource* ImageStore::find(uint32_t id) { return find_by_id(images_, id); }

void ImageStore::erase(uint32_t id) {
  images_.erase(std::remove_if(images_.begin(), images_.end(),
                               [id](const ImageResource& image) { return image.id == id; }),
                images_.end());
  images_.shrink_to_fit();
}

void ImageStore::crop(uint32_t id, uint32_t width, uint32_t height) {
  ImageResource* image = find(id);
  if (image == nullptr) return;
  image->width = std::clamp<uint32_t>(width, 1, 1024);
  image->height = std::clamp<uint32_t>(height, 1, 1024);
  ++image->revision;
}

void ImageStore::apply_mask(uint32_t id, uint8_t factor) {
  ImageResource* image = find(id);
  if (image == nullptr || image->coverage.empty()) return;
  const std::size_t area = bounded_area(image->width, image->height);
  for (std::size_t index = 0; index < area; ++index) {
    image->coverage[index] = static_cast<uint8_t>((image->coverage[index] * factor) >> 8U);
  }
}

void ImageStore::atlas_pack(uint32_t id, uint32_t salt) {
  ImageResource* image = find(id);
  if (image == nullptr || image->pixels.empty()) return;
  const std::size_t offset = atlas_.size();
  const std::size_t copy = std::min<std::size_t>(image->pixels.size(), 16384);
  atlas_.reserve(atlas_.size() + copy + 64U);
  for (std::size_t index = 0; index < copy; ++index) {
    atlas_.push_back(static_cast<uint8_t>(image->pixels[index] ^ (mix32(salt + index) & 0xffU)));
  }
  image->atlas_view = atlas_.data() + offset;
  image->atlas_length = copy;
}

void ImageStore::atlas_repack(uint32_t salt) {
  std::vector<uint8_t> repacked;
  repacked.reserve(atlas_.size() + 256U);
  for (std::size_t index = 0; index < atlas_.size(); ++index) {
    repacked.push_back(static_cast<uint8_t>(atlas_[index] ^ (mix32(salt ^ index) & 0x1fU)));
  }
  repacked.shrink_to_fit();
  atlas_.swap(repacked);
}

uint64_t ImageStore::render_from_atlas(uint32_t id, uint32_t salt) const {
  const ImageResource* image = find_by_id(images_, id);
  if (image == nullptr || image->atlas_view == nullptr || image->atlas_length == 0) return salt;
  return fold_bytes(image->atlas_view, image->atlas_length, mix64(salt ^ id));
}

std::vector<ImageResource>& ImageStore::images() noexcept { return images_; }

PathResource* PathStore::add(uint32_t id, std::vector<Point> outer,
                             std::vector<Point> hole) {
  if (PathResource* existing = find(id)) return existing;
  PathResource path;
  path.id = id;
  path.outer = std::move(outer);
  path.hole = std::move(hole);
  path.revision = 1;
  paths_.push_back(std::move(path));
  return &paths_.back();
}

PathResource* PathStore::find(uint32_t id) { return find_by_id(paths_, id); }

void PathStore::erase(uint32_t id) {
  paths_.erase(std::remove_if(paths_.begin(), paths_.end(),
                              [id](const PathResource& path) { return path.id == id; }),
               paths_.end());
  paths_.shrink_to_fit();
}

void PathStore::tessellate(uint32_t id, uint8_t winding) {
  PathResource* path = find(id);
  if (path == nullptr || path->outer.size() < 3) return;
  const std::size_t outer_triangles = path->outer.size() - 2U;
  const std::size_t estimate = outer_triangles * 3U;
  path->triangles.assign(estimate, 0);
  path->triangles.shrink_to_fit();
  std::size_t cursor = 0;
  for (std::size_t index = 1; index + 1 < path->outer.size(); ++index) {
    path->triangles[cursor++] = 0;
    path->triangles[cursor++] = static_cast<uint32_t>(index);
    path->triangles[cursor++] = static_cast<uint32_t>(index + 1U);
  }
  if ((winding & 1U) != 0 && path->hole.size() >= 3) {
    for (std::size_t index = 1; index + 1 < path->hole.size(); ++index) {
      path->triangles[cursor++] = static_cast<uint32_t>(path->outer.size());
      path->triangles[cursor++] = static_cast<uint32_t>(path->outer.size() + index);
      path->triangles[cursor++] = static_cast<uint32_t>(path->outer.size() + index + 1U);
    }
  }
  ++path->revision;
}

std::vector<PathResource>& PathStore::paths() noexcept { return paths_; }

std::size_t RenderPlanner::count_layers(const std::vector<Layer>& layers) const {
  std::size_t count = 0;
  for (const Layer& layer : layers) {
    ++count;
    count += count_layers(layer.children);
  }
  return count;
}

void RenderPlanner::emit_layers(const std::vector<Layer>& layers, RenderOp* output,
                                std::size_t& cursor, uint8_t mode) const {
  for (const Layer& layer : layers) {
    output[cursor++] = RenderOp{layer.id, layer.resource_id, layer.kind, layer.blend};
    if ((mode & 1U) != 0 && layer.kind == 4U) {
      output[cursor++] = RenderOp{layer.id ^ 0x80000000U, layer.resource_id, 0xfeU, 1U};
    }
    emit_layers(layer.children, output, cursor, mode);
  }
}

uint64_t RenderPlanner::plan(const Page& page, uint8_t mode, uint32_t salt) {
  const std::size_t estimated = count_layers(page.layers);
  last_plan_.assign(estimated, RenderOp{});
  last_plan_.shrink_to_fit();
  std::size_t cursor = 0;
  emit_layers(page.layers, last_plan_.data(), cursor, mode);
  uint64_t result = mix64(salt ^ page.id ^ cursor);
  for (std::size_t index = 0; index < cursor; ++index) {
    const RenderOp& op = last_plan_[index];
    result = mix64(result ^ op.layer_id ^ (static_cast<uint64_t>(op.resource_id) << 1U));
  }
  return result;
}

std::size_t RenderPlanner::last_count() const noexcept { return last_plan_.size(); }

void RenderCache::insert(uint32_t key, uint32_t page_id, uint64_t digest) {
  auto node = std::make_unique<Node>();
  node->key = key;
  node->page_id = page_id;
  node->digest = digest;
  Node* raw = node.get();
  nodes_[key] = std::move(node);
  lru_.push_back(raw);
}

void RenderCache::purge(uint32_t key) { nodes_.erase(key); }

uint64_t RenderCache::touch(uint32_t key, uint32_t salt) {
  auto it = std::find_if(lru_.begin(), lru_.end(),
                         [key](const Node* node) { return node->key == key; });
  if (it == lru_.end()) return salt;
  Node* node = *it;
  ++node->hits;
  node->digest = mix64(node->digest ^ salt ^ node->hits);
  return node->digest;
}

void TransactionManager::begin(uint32_t id) {
  transaction_id_ = id;
  image_undo_.clear();
}

void TransactionManager::track_image(ImageResource* image) {
  if (image == nullptr || transaction_id_ == 0) return;
  image_undo_.push_back(ImageUndo{image, image->width, image->height, image->revision});
}

void TransactionManager::undo() {
  for (auto it = image_undo_.rbegin(); it != image_undo_.rend(); ++it) {
    it->image->width = it->width;
    it->image->height = it->height;
    it->image->revision = it->revision + 1U;
  }
  transaction_id_ = 0;
  image_undo_.clear();
}

void TransactionManager::clear() {
  transaction_id_ = 0;
  image_undo_.clear();
}

ProfileStore::ProfileStore() { entries_.reserve(4); }

ProfileStore::~ProfileStore() {
  for (Entry& entry : entries_) delete[] entry.data;
  for (Snapshot& snapshot : snapshots_) {
    if (snapshot.owns) delete[] snapshot.data;
  }
}

ProfileStore::Entry* ProfileStore::find(uint32_t id) { return find_by_id(entries_, id); }
const ProfileStore::Entry* ProfileStore::find(uint32_t id) const {
  return find_by_id(entries_, id);
}

void ProfileStore::add(uint32_t id, const std::vector<uint8_t>& bytes) {
  if (find(id) != nullptr || bytes.empty()) return;
  Entry entry;
  entry.id = id;
  entry.size = bytes.size();
  entry.data = new uint8_t[entry.size];
  std::memcpy(entry.data, bytes.data(), entry.size);
  entries_.push_back(entry);
}

void ProfileStore::replace(uint32_t id, const std::vector<uint8_t>& bytes) {
  Entry* entry = find(id);
  if (entry == nullptr || bytes.empty()) return;
  snapshots_.push_back(Snapshot{id, entry->data, entry->size, true});
  uint8_t* replacement = new uint8_t[bytes.size()];
  std::memcpy(replacement, bytes.data(), bytes.size());
  entry->data = replacement;
  entry->size = bytes.size();
}

void ProfileStore::rollback(uint32_t id) {
  Entry* entry = find(id);
  auto it = std::find_if(snapshots_.rbegin(), snapshots_.rend(),
                         [id](const Snapshot& snapshot) { return snapshot.id == id; });
  if (entry == nullptr || it == snapshots_.rend()) return;
  delete[] entry->data;
  entry->data = it->data;
  entry->size = it->size;
}

uint64_t ProfileStore::inspect(uint32_t id) const {
  const Entry* entry = find(id);
  if (entry == nullptr || entry->data == nullptr) return 0;
  return fold_bytes(entry->data, entry->size, id);
}

RenderScheduler::RenderScheduler() { jobs_.reserve(8); }

RenderScheduler::~RenderScheduler() {
  for (Job& job : jobs_) {
    if (job.owns) delete[] job.scratch;
  }
}

void RenderScheduler::schedule(const Page& page, uint32_t nonce, uint8_t priority) {
  uint64_t signature = mix64(page.layers.size() ^ nonce ^ priority);
  for (const Layer& layer : page.layers) {
    signature = mix64(signature ^ layer.kind ^ layer.resource_id ^ layer.children.size());
  }
  Job job;
  job.page_id = page.id;
  job.signature = signature;
  job.priority = priority;
  job.scratch_size = 128U + (signature & 0x7fU);
  auto match = std::find_if(jobs_.begin(), jobs_.end(), [signature](const Job& existing) {
    return existing.signature == signature;
  });
  if (match != jobs_.end()) {
    job.scratch = match->scratch;
    job.scratch_size = match->scratch_size;
    job.owns = true;
  } else {
    job.scratch = new uint8_t[job.scratch_size];
    for (std::size_t index = 0; index < job.scratch_size; ++index) {
      job.scratch[index] = static_cast<uint8_t>(mix32(static_cast<uint32_t>(signature + index)));
    }
    job.owns = true;
  }
  jobs_.push_back(job);
}

uint64_t RenderScheduler::flush(uint32_t salt) {
  std::stable_sort(jobs_.begin(), jobs_.end(), [](const Job& left, const Job& right) {
    if (left.priority != right.priority) return left.priority > right.priority;
    return left.page_id < right.page_id;
  });
  uint64_t result = salt;
  for (Job& job : jobs_) {
    result = fold_bytes(job.scratch, job.scratch_size, result ^ job.signature);
    if (job.owns) {
      delete[] job.scratch;
      job.scratch = nullptr;
      job.owns = false;
    }
  }
  jobs_.clear();
  return result;
}

ResourcePool::ResourcePool() { slots_.reserve(4); }

ResourcePool::~ResourcePool() {
  for (Slot& slot : slots_) destroy_slot(slot);
}

uint32_t ResourcePool::make_handle(std::size_t index, uint8_t generation, Kind kind) const {
  return static_cast<uint32_t>(index & 0xffffU) |
         (static_cast<uint32_t>(generation) << 16U) |
         (static_cast<uint32_t>(kind) << 24U);
}

std::size_t ResourcePool::index_from(uint32_t handle) const { return handle & 0xffffU; }
uint8_t ResourcePool::generation_from(uint32_t handle) const {
  return static_cast<uint8_t>((handle >> 16U) & 0xffU);
}

void ResourcePool::destroy_slot(Slot& slot) {
  if (slot.payload == nullptr) {
    slot.kind = Kind::Empty;
    return;
  }
  if (slot.kind == Kind::Bitmap) {
    auto* bitmap = static_cast<BitmapPayload*>(slot.payload);
    delete[] bitmap->pixels;
    delete bitmap;
  } else if (slot.kind == Kind::Gradient) {
    auto* gradient = static_cast<GradientPayload*>(slot.payload);
    delete[] gradient->stops;
    delete gradient;
  }
  slot.payload = nullptr;
  slot.kind = Kind::Empty;
}

uint32_t ResourcePool::allocate_bitmap(std::size_t length, uint8_t fill) {
  length = std::clamp<std::size_t>(length, 1, 4096);
  std::size_t index = 0;
  while (index < slots_.size() && slots_[index].kind != Kind::Empty) ++index;
  if (index == slots_.size()) slots_.push_back(Slot{});
  Slot& slot = slots_[index];
  auto* bitmap = new BitmapPayload();
  bitmap->length = length;
  bitmap->pixels = new uint8_t[length];
  std::memset(bitmap->pixels, fill, length);
  slot.kind = Kind::Bitmap;
  slot.payload = bitmap;
  return make_handle(index, slot.generation, Kind::Bitmap);
}

uint32_t ResourcePool::allocate_gradient(uint8_t stops) {
  const std::size_t count = std::clamp<std::size_t>(stops, 1, 32);
  std::size_t index = 0;
  while (index < slots_.size() && slots_[index].kind != Kind::Empty) ++index;
  if (index == slots_.size()) slots_.push_back(Slot{});
  Slot& slot = slots_[index];
  auto* gradient = new GradientPayload();
  gradient->stop_count = count;
  gradient->stops = new uint8_t[count * 4U];
  for (std::size_t offset = 0; offset < count * 4U; ++offset) {
    gradient->stops[offset] = static_cast<uint8_t>(mix32(static_cast<uint32_t>(offset + count)));
  }
  slot.kind = Kind::Gradient;
  slot.payload = gradient;
  return make_handle(index, slot.generation, Kind::Gradient);
}

void ResourcePool::release(uint32_t handle) {
  const std::size_t index = index_from(handle);
  if (index >= slots_.size()) return;
  Slot& slot = slots_[index];
  if (slot.generation != generation_from(handle)) return;
  destroy_slot(slot);
  ++slot.generation;
}

uint8_t ResourcePool::probe_bitmap(uint32_t handle) const {
  const std::size_t index = index_from(handle);
  if (index >= slots_.size()) return 0;
  const Slot& slot = slots_[index];
  if (slot.generation != generation_from(handle) || slot.payload == nullptr) return 0;
  const auto* bitmap = static_cast<const BitmapPayload*>(slot.payload);
  return bitmap->pixels[bitmap->length > 3 ? 3 : 0];
}

void Clipboard::copy(uint32_t style_id, const StyleCatalog& styles,
                     std::vector<uint32_t> resources, std::string text) {
  style_ordinal_ = styles.ordinal_of(style_id);
  resources_ = std::move(resources);
  text_ = std::move(text);
  populated_ = true;
}

uint64_t Clipboard::paste(StyleCatalog& styles, uint32_t salt) const {
  if (!populated_) return salt;
  Style* style = styles.by_ordinal_unchecked(style_ordinal_);
  uint64_t result = mix64(salt ^ style->id ^ style->rgba);
  for (uint32_t resource : resources_) result = mix64(result ^ resource);
  return fold_bytes(reinterpret_cast<const uint8_t*>(text_.data()), text_.size(), result);
}

DependencyMap::DependencyMap() { nodes_.reserve(4); }

void DependencyMap::build(uint32_t root, const std::vector<uint32_t>& children) {
  auto node = std::make_unique<Node>();
  node->id = root;
  node->edges = children;
  node->weight = mix64(root ^ children.size());
  selected_ = node.get();
  nodes_.push_back(std::move(node));
  for (uint32_t child : children) {
    auto leaf = std::make_unique<Node>();
    leaf->id = child;
    leaf->weight = mix64(root ^ child);
    nodes_.push_back(std::move(leaf));
  }
}

void DependencyMap::compact(uint32_t victim) {
  nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(), [victim](const auto& node) {
                 return node && node->id == victim;
               }),
               nodes_.end());
  nodes_.shrink_to_fit();
}

uint64_t DependencyMap::visit_selected(uint32_t salt) const {
  if (selected_ == nullptr) return salt;
  uint64_t result = mix64(salt ^ selected_->id ^ selected_->weight);
  for (uint32_t edge : selected_->edges) result = mix64(result ^ edge);
  return result;
}

void MeshCache::cache(uint32_t key, PathResource* path) {
  if (path == nullptr) return;
  meshes_.push_back(CachedMesh{key, path, path->revision});
}

uint64_t MeshCache::draw(uint32_t key, uint32_t salt) const {
  auto it = std::find_if(meshes_.begin(), meshes_.end(),
                         [key](const CachedMesh& mesh) { return mesh.key == key; });
  if (it == meshes_.end() || it->path == nullptr) return salt;
  uint64_t result = mix64(salt ^ it->path->id ^ it->revision);
  for (uint32_t index : it->path->triangles) result = mix64(result ^ index);
  for (const Point& point : it->path->outer) {
    result = mix64(result ^ static_cast<uint32_t>(point.x) ^
                   (static_cast<uint64_t>(static_cast<uint32_t>(point.y)) << 32U));
  }
  return result;
}

}  // namespace vellumgrid
