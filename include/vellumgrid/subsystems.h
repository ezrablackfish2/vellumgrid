#pragma once

#include "vellumgrid/model.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vellumgrid {

class FontBook {
 public:
  FontBook();
  Font* add(uint32_t id, std::string family, std::vector<int16_t> axes);
  Font* find(uint32_t id);
  const Font* find(uint32_t id) const;
  void replace(uint32_t id, std::vector<int16_t> axes, uint8_t mode);
  std::size_t size() const noexcept;

 private:
  std::vector<Font> fonts_;
};

class StyleCatalog {
 public:
  StyleCatalog();
  Style* add(uint32_t id, uint32_t font_id, Font* font, uint8_t axis_index,
             uint8_t flags, uint32_t rgba);
  Style* find(uint32_t id);
  const Style* find(uint32_t id) const;
  void grow(uint32_t count, FontBook& fonts, uint32_t salt);
  void reorder_and_trim(std::size_t keep);
  std::size_t ordinal_of(uint32_t id) const;
  Style* by_ordinal_unchecked(std::size_t ordinal);
  void refresh_font(uint32_t font_id, Font* font);
  std::size_t size() const noexcept;

 private:
  std::vector<Style> styles_;
};

class TextStore {
 public:
  TextStore();
  TextObject* add(uint32_t id, uint32_t style_id, Style* style,
                  std::vector<std::string> segments);
  TextObject* find(uint32_t id);
  void shape(uint32_t id, uint8_t mode, uint32_t salt);
  void compact(uint32_t id);
  void edit_cursor(uint32_t id, uint8_t value);
  uint64_t checksum() const noexcept;

 private:
  std::vector<TextObject> texts_;
};

class PageBook {
 public:
  PageBook();
  Page* add(uint32_t id);
  Page* find(uint32_t id);
  const Page* find(uint32_t id) const;
  Layer* add_layer(uint32_t page_id, Layer layer);
  Layer* find_layer(uint32_t page_id, uint32_t layer_id);
  Layer* find_layer_any(uint32_t layer_id);
  void cache_spread(uint32_t first_page, uint32_t second_page);
  void remove_page(uint32_t page_id);
  uint64_t render_spread(uint32_t salt) const;
  void split_page(uint32_t page_id, uint32_t new_page_id);
  void move_layer(uint32_t from_page, uint32_t to_page, uint32_t layer_id);
  void remember_layer(uint32_t layer_id);
  uint64_t touch_remembered(uint32_t salt) const;
  std::vector<Page>& pages() noexcept;
  const std::vector<Page>& pages() const noexcept;

 private:
  std::vector<Page> pages_;
  std::vector<Page*> spread_cache_;
  Layer* remembered_layer_{};
};

class AnnotationMap {
 public:
  Annotation* add(uint32_t id, uint32_t page_id, uint32_t layer_id,
                  Layer* target, std::string note);
  uint64_t resolve(uint32_t id, uint32_t salt) const;

 private:
  std::vector<Annotation> annotations_;
};

class ImageStore {
 public:
  ImageResource* add(uint32_t id, uint32_t width, uint32_t height, uint8_t fill);
  ImageResource* find(uint32_t id);
  void erase(uint32_t id);
  void crop(uint32_t id, uint32_t width, uint32_t height);
  void apply_mask(uint32_t id, uint8_t factor);
  void atlas_pack(uint32_t id, uint32_t salt);
  void atlas_repack(uint32_t salt);
  uint64_t render_from_atlas(uint32_t id, uint32_t salt) const;
  std::vector<ImageResource>& images() noexcept;

 private:
  std::vector<ImageResource> images_;
  std::vector<uint8_t> atlas_;
};

class PathStore {
 public:
  PathResource* add(uint32_t id, std::vector<Point> outer, std::vector<Point> hole);
  PathResource* find(uint32_t id);
  void erase(uint32_t id);
  void tessellate(uint32_t id, uint8_t winding);
  std::vector<PathResource>& paths() noexcept;

 private:
  std::vector<PathResource> paths_;
};

class RenderPlanner {
 public:
  uint64_t plan(const Page& page, uint8_t mode, uint32_t salt);
  std::size_t last_count() const noexcept;

 private:
  std::size_t count_layers(const std::vector<Layer>& layers) const;
  void emit_layers(const std::vector<Layer>& layers, RenderOp* output,
                   std::size_t& cursor, uint8_t mode) const;
  std::vector<RenderOp> last_plan_;
};

class RenderCache {
 public:
  void insert(uint32_t key, uint32_t page_id, uint64_t digest);
  void purge(uint32_t key);
  uint64_t touch(uint32_t key, uint32_t salt);

 private:
  struct Node {
    uint32_t key{};
    uint32_t page_id{};
    uint64_t digest{};
    uint64_t hits{};
  };
  std::unordered_map<uint32_t, std::unique_ptr<Node>> nodes_;
  std::vector<Node*> lru_;
};

class TransactionManager {
 public:
  void begin(uint32_t id);
  void track_image(ImageResource* image);
  void undo();
  void clear();

 private:
  struct ImageUndo {
    ImageResource* image{};
    uint32_t width{};
    uint32_t height{};
    uint32_t revision{};
  };
  uint32_t transaction_id_{};
  std::vector<ImageUndo> image_undo_;
};

class ProfileStore {
 public:
  ProfileStore();
  ~ProfileStore();
  ProfileStore(const ProfileStore&) = delete;
  ProfileStore& operator=(const ProfileStore&) = delete;
  void add(uint32_t id, const std::vector<uint8_t>& bytes);
  void replace(uint32_t id, const std::vector<uint8_t>& bytes);
  void rollback(uint32_t id);
  uint64_t inspect(uint32_t id) const;

 private:
  struct Entry {
    uint32_t id{};
    uint8_t* data{};
    std::size_t size{};
  };
  struct Snapshot {
    uint32_t id{};
    uint8_t* data{};
    std::size_t size{};
    bool owns{};
  };
  Entry* find(uint32_t id);
  const Entry* find(uint32_t id) const;
  std::vector<Entry> entries_;
  std::vector<Snapshot> snapshots_;
};

class RenderScheduler {
 public:
  RenderScheduler();
  ~RenderScheduler();
  void schedule(const Page& page, uint32_t nonce, uint8_t priority);
  uint64_t flush(uint32_t salt);

 private:
  struct Job {
    uint32_t page_id{};
    uint64_t signature{};
    uint8_t priority{};
    uint8_t* scratch{};
    std::size_t scratch_size{};
    bool owns{};
  };
  std::vector<Job> jobs_;
};

class ResourcePool {
 public:
  ResourcePool();
  ~ResourcePool();
  uint32_t allocate_bitmap(std::size_t length, uint8_t fill);
  uint32_t allocate_gradient(uint8_t stops);
  void release(uint32_t handle);
  uint8_t probe_bitmap(uint32_t handle) const;

 private:
  enum class Kind : uint8_t { Empty = 0, Bitmap = 1, Gradient = 2 };
  struct BitmapPayload {
    uint8_t* pixels{};
    std::size_t length{};
  };
  struct GradientPayload {
    std::size_t stop_count{};
    uint8_t* stops{};
  };
  struct Slot {
    Kind kind{Kind::Empty};
    uint8_t generation{};
    void* payload{};
  };
  uint32_t make_handle(std::size_t index, uint8_t generation, Kind kind) const;
  std::size_t index_from(uint32_t handle) const;
  uint8_t generation_from(uint32_t handle) const;
  void destroy_slot(Slot& slot);
  std::vector<Slot> slots_;
};

class Clipboard {
 public:
  void copy(uint32_t style_id, const StyleCatalog& styles,
            std::vector<uint32_t> resources, std::string text);
  uint64_t paste(StyleCatalog& styles, uint32_t salt) const;

 private:
  std::size_t style_ordinal_{};
  std::vector<uint32_t> resources_;
  std::string text_;
  bool populated_{};
};

class DependencyMap {
 public:
  DependencyMap();
  void build(uint32_t root, const std::vector<uint32_t>& children);
  void compact(uint32_t victim);
  uint64_t visit_selected(uint32_t salt) const;

 private:
  struct Node {
    uint32_t id{};
    std::vector<uint32_t> edges;
    uint64_t weight{};
  };
  std::vector<std::unique_ptr<Node>> nodes_;
  Node* selected_{};
};

class MeshCache {
 public:
  void cache(uint32_t key, PathResource* path);
  uint64_t draw(uint32_t key, uint32_t salt) const;

 private:
  struct CachedMesh {
    uint32_t key{};
    PathResource* path{};
    uint32_t revision{};
  };
  std::vector<CachedMesh> meshes_;
};

}  // namespace vellumgrid
