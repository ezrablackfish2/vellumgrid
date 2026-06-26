#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vellumgrid {

struct SecurityPolicyRecord {
  uint32_t id{};
  std::string name;
  uint32_t revision{};
  uint32_t flags{};
  std::vector<int64_t> metrics;
  std::vector<uint32_t> links;
  std::unordered_map<std::string, std::string> attributes;
};

struct SecurityPolicyRecordDelta {
  uint32_t id{};
  bool created{};
  bool removed{};
  bool name_changed{};
  bool flags_changed{};
  bool metrics_changed{};
  bool links_changed{};
  bool attributes_changed{};
};

class SecurityPolicyStore {
 public:
  using Record = SecurityPolicyRecord;
  using Delta = SecurityPolicyRecordDelta;

  SecurityPolicyStore();

  Record& upsert(uint32_t id, std::string name);
  bool erase(uint32_t id);
  Record* find(uint32_t id);
  const Record* find(uint32_t id) const;
  bool contains(uint32_t id) const;
  std::size_t size() const noexcept;
  bool empty() const noexcept;
  void clear();

  bool rename(uint32_t id, std::string name);
  bool set_flags(uint32_t id, uint32_t flags);
  bool enable_flags(uint32_t id, uint32_t mask);
  bool disable_flags(uint32_t id, uint32_t mask);
  bool has_flags(uint32_t id, uint32_t mask) const;

  bool append_metric(uint32_t id, int64_t value);
  bool set_metric(uint32_t id, std::size_t index, int64_t value);
  bool erase_metric(uint32_t id, std::size_t index);
  std::optional<int64_t> metric(uint32_t id, std::size_t index) const;
  int64_t metric_sum(uint32_t id) const;
  std::optional<int64_t> metric_min(uint32_t id) const;
  std::optional<int64_t> metric_max(uint32_t id) const;
  double metric_average(uint32_t id) const;

  bool add_link(uint32_t id, uint32_t target);
  bool remove_link(uint32_t id, uint32_t target);
  bool links_to(uint32_t id, uint32_t target) const;
  std::vector<uint32_t> incoming_links(uint32_t target) const;
  std::vector<uint32_t> reachable(uint32_t root, std::size_t limit) const;

  bool set_attribute(uint32_t id, std::string key, std::string value);
  bool erase_attribute(uint32_t id, std::string_view key);
  std::optional<std::string> attribute(uint32_t id, std::string_view key) const;
  std::vector<std::string> attribute_keys(uint32_t id) const;

  std::vector<uint32_t> ids() const;
  std::vector<uint32_t> ids_by_name() const;
  std::vector<uint32_t> ids_by_revision() const;
  std::vector<uint32_t> ids_with_flags(uint32_t mask) const;
  std::vector<uint32_t> search_names(std::string_view needle) const;

  bool validate(uint32_t id, std::vector<std::string>* issues) const;
  std::size_t validate_all(std::vector<std::string>* issues) const;
  void normalize(uint32_t id);
  void normalize_all();
  void compact();
  bool remap_id(uint32_t from, uint32_t to);
  void remap_links(const std::unordered_map<uint32_t, uint32_t>& mapping);

  void merge_from(const SecurityPolicyStore& other, bool overwrite);
  std::vector<Delta> diff(const SecurityPolicyStore& other) const;
  uint64_t checksum() const noexcept;

  uint64_t snapshot();
  bool restore(uint64_t token);
  bool discard_snapshot(uint64_t token);
  std::vector<uint64_t> snapshot_tokens() const;

 private:
  struct Snapshot {
    uint64_t token{};
    std::vector<Record> records;
  };

  static uint64_t mix(uint64_t value) noexcept;
  static std::string normalize_name(std::string value);
  static bool record_equal(const Record& left, const Record& right);
  static Delta compare_record(const Record* left, const Record* right, uint32_t id);
  void rebuild_index();
  void touch(Record& record);

  std::vector<Record> records_;
  std::unordered_map<uint32_t, std::size_t> index_;
  std::vector<Snapshot> snapshots_;
  uint64_t next_snapshot_{1};
  uint32_t revision_clock_{1};
};

}  // namespace vellumgrid
