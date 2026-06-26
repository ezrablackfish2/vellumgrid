#include "vellumgrid/catalog/locale_registry.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <unordered_set>

namespace vellumgrid {
namespace {

constexpr uint64_t kDomainSalt = UINT64_C(0xb55a4f09);
constexpr std::size_t kMaxReachable = 4096;

uint64_t hash_text(std::string_view text) noexcept {
  uint64_t value = UINT64_C(1469598103934665603) ^ kDomainSalt;
  for (unsigned char byte : text) {
    value ^= byte;
    value *= UINT64_C(1099511628211);
  }
  return value;
}

uint64_t hash_number(uint64_t value) noexcept {
  value ^= value >> 30U;
  value *= UINT64_C(0xbf58476d1ce4e5b9);
  value ^= value >> 27U;
  value *= UINT64_C(0x94d049bb133111eb);
  value ^= value >> 31U;
  return value;
}

bool same_metrics(const std::vector<int64_t>& left,
                  const std::vector<int64_t>& right) {
  return left == right;
}

bool same_links(const std::vector<uint32_t>& left,
                const std::vector<uint32_t>& right) {
  return left == right;
}

}  // namespace

LocaleRegistry::LocaleRegistry() = default;

LocaleRegistry::Record& LocaleRegistry::upsert(uint32_t id, std::string name) {
  if (Record* current = find(id)) {
    current->name = normalize_name(std::move(name));
    touch(*current);
    return *current;
  }

  Record record;
  record.id = id;
  record.name = normalize_name(std::move(name));
  record.revision = revision_clock_++;
  records_.push_back(std::move(record));
  index_[id] = records_.size() - 1;
  return records_.back();
}

bool LocaleRegistry::erase(uint32_t id) {
  const auto found = index_.find(id);
  if (found == index_.end()) {
    return false;
  }

  const std::size_t ordinal = found->second;
  records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(ordinal));
  rebuild_index();
  for (Record& record : records_) {
    auto& links = record.links;
    links.erase(std::remove(links.begin(), links.end(), id), links.end());
  }
  return true;
}

LocaleRegistry::Record* LocaleRegistry::find(uint32_t id) {
  const auto found = index_.find(id);
  if (found == index_.end() || found->second >= records_.size()) {
    return nullptr;
  }
  return &records_[found->second];
}

const LocaleRegistry::Record* LocaleRegistry::find(uint32_t id) const {
  const auto found = index_.find(id);
  if (found == index_.end() || found->second >= records_.size()) {
    return nullptr;
  }
  return &records_[found->second];
}

bool LocaleRegistry::contains(uint32_t id) const {
  return find(id) != nullptr;
}

std::size_t LocaleRegistry::size() const noexcept {
  return records_.size();
}

bool LocaleRegistry::empty() const noexcept {
  return records_.empty();
}

void LocaleRegistry::clear() {
  records_.clear();
  index_.clear();
  snapshots_.clear();
  next_snapshot_ = 1;
  revision_clock_ = 1;
}

bool LocaleRegistry::rename(uint32_t id, std::string name) {
  Record* record = find(id);
  if (record == nullptr) {
    return false;
  }
  const std::string normalized = normalize_name(std::move(name));
  if (record->name == normalized) {
    return true;
  }
  record->name = normalized;
  touch(*record);
  return true;
}

bool LocaleRegistry::set_flags(uint32_t id, uint32_t flags) {
  Record* record = find(id);
  if (record == nullptr) {
    return false;
  }
  if (record->flags != flags) {
    record->flags = flags;
    touch(*record);
  }
  return true;
}

bool LocaleRegistry::enable_flags(uint32_t id, uint32_t mask) {
  Record* record = find(id);
  if (record == nullptr) {
    return false;
  }
  const uint32_t updated = record->flags | mask;
  if (updated != record->flags) {
    record->flags = updated;
    touch(*record);
  }
  return true;
}

bool LocaleRegistry::disable_flags(uint32_t id, uint32_t mask) {
  Record* record = find(id);
  if (record == nullptr) {
    return false;
  }
  const uint32_t updated = record->flags & ~mask;
  if (updated != record->flags) {
    record->flags = updated;
    touch(*record);
  }
  return true;
}

bool LocaleRegistry::has_flags(uint32_t id, uint32_t mask) const {
  const Record* record = find(id);
  return record != nullptr && (record->flags & mask) == mask;
}

bool LocaleRegistry::append_metric(uint32_t id, int64_t value) {
  Record* record = find(id);
  if (record == nullptr || record->metrics.size() >= 4096) {
    return false;
  }
  record->metrics.push_back(value);
  touch(*record);
  return true;
}

bool LocaleRegistry::set_metric(uint32_t id, std::size_t index, int64_t value) {
  Record* record = find(id);
  if (record == nullptr || index >= record->metrics.size()) {
    return false;
  }
  if (record->metrics[index] != value) {
    record->metrics[index] = value;
    touch(*record);
  }
  return true;
}

bool LocaleRegistry::erase_metric(uint32_t id, std::size_t index) {
  Record* record = find(id);
  if (record == nullptr || index >= record->metrics.size()) {
    return false;
  }
  record->metrics.erase(record->metrics.begin() + static_cast<std::ptrdiff_t>(index));
  touch(*record);
  return true;
}

std::optional<int64_t> LocaleRegistry::metric(uint32_t id, std::size_t index) const {
  const Record* record = find(id);
  if (record == nullptr || index >= record->metrics.size()) {
    return std::nullopt;
  }
  return record->metrics[index];
}

int64_t LocaleRegistry::metric_sum(uint32_t id) const {
  const Record* record = find(id);
  if (record == nullptr) {
    return 0;
  }
  int64_t total = 0;
  for (int64_t value : record->metrics) {
    if (value > 0 && total > std::numeric_limits<int64_t>::max() - value) {
      return std::numeric_limits<int64_t>::max();
    }
    if (value < 0 && total < std::numeric_limits<int64_t>::min() - value) {
      return std::numeric_limits<int64_t>::min();
    }
    total += value;
  }
  return total;
}

std::optional<int64_t> LocaleRegistry::metric_min(uint32_t id) const {
  const Record* record = find(id);
  if (record == nullptr || record->metrics.empty()) {
    return std::nullopt;
  }
  return *std::min_element(record->metrics.begin(), record->metrics.end());
}

std::optional<int64_t> LocaleRegistry::metric_max(uint32_t id) const {
  const Record* record = find(id);
  if (record == nullptr || record->metrics.empty()) {
    return std::nullopt;
  }
  return *std::max_element(record->metrics.begin(), record->metrics.end());
}

double LocaleRegistry::metric_average(uint32_t id) const {
  const Record* record = find(id);
  if (record == nullptr || record->metrics.empty()) {
    return 0.0;
  }
  long double total = 0.0;
  for (int64_t value : record->metrics) {
    total += static_cast<long double>(value);
  }
  return static_cast<double>(total / static_cast<long double>(record->metrics.size()));
}

bool LocaleRegistry::add_link(uint32_t id, uint32_t target) {
  Record* record = find(id);
  if (record == nullptr || target == id) {
    return false;
  }
  if (std::find(record->links.begin(), record->links.end(), target) != record->links.end()) {
    return true;
  }
  record->links.push_back(target);
  touch(*record);
  return true;
}

bool LocaleRegistry::remove_link(uint32_t id, uint32_t target) {
  Record* record = find(id);
  if (record == nullptr) {
    return false;
  }
  auto found = std::find(record->links.begin(), record->links.end(), target);
  if (found == record->links.end()) {
    return false;
  }
  record->links.erase(found);
  touch(*record);
  return true;
}

bool LocaleRegistry::links_to(uint32_t id, uint32_t target) const {
  const Record* record = find(id);
  return record != nullptr &&
         std::find(record->links.begin(), record->links.end(), target) != record->links.end();
}

std::vector<uint32_t> LocaleRegistry::incoming_links(uint32_t target) const {
  std::vector<uint32_t> incoming;
  for (const Record& record : records_) {
    if (std::find(record.links.begin(), record.links.end(), target) != record.links.end()) {
      incoming.push_back(record.id);
    }
  }
  std::sort(incoming.begin(), incoming.end());
  return incoming;
}

std::vector<uint32_t> LocaleRegistry::reachable(uint32_t root, std::size_t limit) const {
  std::vector<uint32_t> result;
  if (find(root) == nullptr || limit == 0) {
    return result;
  }
  limit = std::min(limit, kMaxReachable);
  std::queue<uint32_t> pending;
  std::unordered_set<uint32_t> visited;
  pending.push(root);
  visited.insert(root);
  while (!pending.empty() && result.size() < limit) {
    const uint32_t current = pending.front();
    pending.pop();
    result.push_back(current);
    const Record* record = find(current);
    if (record == nullptr) {
      continue;
    }
    for (uint32_t target : record->links) {
      if (find(target) != nullptr && visited.insert(target).second) {
        pending.push(target);
      }
    }
  }
  return result;
}

bool LocaleRegistry::set_attribute(uint32_t id, std::string key, std::string value) {
  Record* record = find(id);
  if (record == nullptr || key.empty() || key.size() > 256 || value.size() > 4096) {
    return false;
  }
  key = normalize_name(std::move(key));
  const auto found = record->attributes.find(key);
  if (found != record->attributes.end() && found->second == value) {
    return true;
  }
  record->attributes[std::move(key)] = std::move(value);
  touch(*record);
  return true;
}

bool LocaleRegistry::erase_attribute(uint32_t id, std::string_view key) {
  Record* record = find(id);
  if (record == nullptr) {
    return false;
  }
  const std::size_t erased = record->attributes.erase(std::string(key));
  if (erased != 0) {
    touch(*record);
  }
  return erased != 0;
}

std::optional<std::string> LocaleRegistry::attribute(uint32_t id, std::string_view key) const {
  const Record* record = find(id);
  if (record == nullptr) {
    return std::nullopt;
  }
  const auto found = record->attributes.find(std::string(key));
  if (found == record->attributes.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::vector<std::string> LocaleRegistry::attribute_keys(uint32_t id) const {
  std::vector<std::string> keys;
  const Record* record = find(id);
  if (record == nullptr) {
    return keys;
  }
  keys.reserve(record->attributes.size());
  for (const auto& entry : record->attributes) {
    keys.push_back(entry.first);
  }
  std::sort(keys.begin(), keys.end());
  return keys;
}

std::vector<uint32_t> LocaleRegistry::ids() const {
  std::vector<uint32_t> result;
  result.reserve(records_.size());
  for (const Record& record : records_) {
    result.push_back(record.id);
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<uint32_t> LocaleRegistry::ids_by_name() const {
  std::vector<const Record*> ordered;
  ordered.reserve(records_.size());
  for (const Record& record : records_) {
    ordered.push_back(&record);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const Record* left, const Record* right) {
    if (left->name != right->name) {
      return left->name < right->name;
    }
    return left->id < right->id;
  });
  std::vector<uint32_t> result;
  result.reserve(ordered.size());
  for (const Record* record : ordered) {
    result.push_back(record->id);
  }
  return result;
}

std::vector<uint32_t> LocaleRegistry::ids_by_revision() const {
  std::vector<const Record*> ordered;
  ordered.reserve(records_.size());
  for (const Record& record : records_) {
    ordered.push_back(&record);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const Record* left, const Record* right) {
    if (left->revision != right->revision) {
      return left->revision < right->revision;
    }
    return left->id < right->id;
  });
  std::vector<uint32_t> result;
  result.reserve(ordered.size());
  for (const Record* record : ordered) {
    result.push_back(record->id);
  }
  return result;
}

std::vector<uint32_t> LocaleRegistry::ids_with_flags(uint32_t mask) const {
  std::vector<uint32_t> result;
  for (const Record& record : records_) {
    if ((record.flags & mask) == mask) {
      result.push_back(record.id);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<uint32_t> LocaleRegistry::search_names(std::string_view needle) const {
  std::string folded(needle);
  std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char byte) {
    return static_cast<char>(std::tolower(byte));
  });
  std::vector<uint32_t> result;
  for (const Record& record : records_) {
    if (record.name.find(folded) != std::string::npos) {
      result.push_back(record.id);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

bool LocaleRegistry::validate(uint32_t id, std::vector<std::string>* issues) const {
  const Record* record = find(id);
  if (record == nullptr) {
    if (issues != nullptr) {
      issues->push_back("missing record " + std::to_string(id));
    }
    return false;
  }
  bool valid = true;
  if (record->name.empty()) {
    valid = false;
    if (issues != nullptr) {
      issues->push_back("empty name on " + std::to_string(id));
    }
  }
  if (record->metrics.size() > 4096) {
    valid = false;
    if (issues != nullptr) {
      issues->push_back("too many metrics on " + std::to_string(id));
    }
  }
  std::unordered_set<uint32_t> seen_links;
  for (uint32_t target : record->links) {
    if (target == id || !seen_links.insert(target).second) {
      valid = false;
      if (issues != nullptr) {
        issues->push_back("invalid link on " + std::to_string(id));
      }
    }
  }
  for (const auto& entry : record->attributes) {
    if (entry.first.empty() || entry.first.size() > 256 || entry.second.size() > 4096) {
      valid = false;
      if (issues != nullptr) {
        issues->push_back("invalid attribute on " + std::to_string(id));
      }
    }
  }
  return valid;
}

std::size_t LocaleRegistry::validate_all(std::vector<std::string>* issues) const {
  std::size_t invalid = 0;
  for (const Record& record : records_) {
    if (!validate(record.id, issues)) {
      ++invalid;
    }
  }
  return invalid;
}

void LocaleRegistry::normalize(uint32_t id) {
  Record* record = find(id);
  if (record == nullptr) {
    return;
  }
  bool changed = false;
  const std::string name = normalize_name(record->name);
  if (name != record->name) {
    record->name = name;
    changed = true;
  }
  auto& links = record->links;
  std::sort(links.begin(), links.end());
  const auto new_end = std::unique(links.begin(), links.end());
  if (new_end != links.end()) {
    links.erase(new_end, links.end());
    changed = true;
  }
  const auto self = std::remove(links.begin(), links.end(), id);
  if (self != links.end()) {
    links.erase(self, links.end());
    changed = true;
  }
  if (changed) {
    touch(*record);
  }
}

void LocaleRegistry::normalize_all() {
  const std::vector<uint32_t> ordered = ids();
  for (uint32_t id : ordered) {
    normalize(id);
  }
}

void LocaleRegistry::compact() {
  records_.shrink_to_fit();
  for (Record& record : records_) {
    record.metrics.shrink_to_fit();
    record.links.shrink_to_fit();
  }
  snapshots_.shrink_to_fit();
  rebuild_index();
}

bool LocaleRegistry::remap_id(uint32_t from, uint32_t to) {
  if (from == to) {
    return contains(from);
  }
  Record* source = find(from);
  if (source == nullptr || contains(to)) {
    return false;
  }
  source->id = to;
  touch(*source);
  for (Record& record : records_) {
    for (uint32_t& link : record.links) {
      if (link == from) {
        link = to;
        touch(record);
      }
    }
  }
  rebuild_index();
  return true;
}

void LocaleRegistry::remap_links(const std::unordered_map<uint32_t, uint32_t>& mapping) {
  for (Record& record : records_) {
    bool changed = false;
    for (uint32_t& link : record.links) {
      const auto found = mapping.find(link);
      if (found != mapping.end() && found->second != link) {
        link = found->second;
        changed = true;
      }
    }
    if (changed) {
      touch(record);
    }
  }
}

void LocaleRegistry::merge_from(const LocaleRegistry& other, bool overwrite) {
  for (const Record& source : other.records_) {
    Record* target = find(source.id);
    if (target == nullptr) {
      records_.push_back(source);
      records_.back().revision = revision_clock_++;
      continue;
    }
    if (!overwrite) {
      continue;
    }
    const uint32_t id = target->id;
    *target = source;
    target->id = id;
    target->revision = revision_clock_++;
  }
  rebuild_index();
}

std::vector<LocaleRegistry::Delta> LocaleRegistry::diff(const LocaleRegistry& other) const {
  std::set<uint32_t> all_ids;
  for (const Record& record : records_) {
    all_ids.insert(record.id);
  }
  for (const Record& record : other.records_) {
    all_ids.insert(record.id);
  }
  std::vector<Delta> result;
  for (uint32_t id : all_ids) {
    const Record* left = find(id);
    const Record* right = other.find(id);
    Delta delta = compare_record(left, right, id);
    if (delta.created || delta.removed || delta.name_changed || delta.flags_changed ||
        delta.metrics_changed || delta.links_changed || delta.attributes_changed) {
      result.push_back(delta);
    }
  }
  return result;
}

uint64_t LocaleRegistry::checksum() const noexcept {
  uint64_t digest = kDomainSalt ^ static_cast<uint64_t>(records_.size());
  std::vector<const Record*> ordered;
  ordered.reserve(records_.size());
  for (const Record& record : records_) {
    ordered.push_back(&record);
  }
  std::sort(ordered.begin(), ordered.end(), [](const Record* left, const Record* right) {
    return left->id < right->id;
  });
  for (const Record* record : ordered) {
    digest ^= mix(record->id + digest);
    digest ^= mix(hash_text(record->name) + digest);
    digest ^= mix(record->flags + digest);
    for (int64_t value : record->metrics) {
      digest ^= hash_number(static_cast<uint64_t>(value) + digest);
    }
    for (uint32_t link : record->links) {
      digest ^= hash_number(link + digest);
    }
    std::vector<std::string> keys;
    keys.reserve(record->attributes.size());
    for (const auto& entry : record->attributes) {
      keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    for (const std::string& key : keys) {
      digest ^= hash_text(key);
      digest ^= hash_text(record->attributes.at(key));
    }
  }
  return mix(digest);
}

uint64_t LocaleRegistry::snapshot() {
  Snapshot state;
  state.token = next_snapshot_++;
  state.records = records_;
  snapshots_.push_back(std::move(state));
  return snapshots_.back().token;
}

bool LocaleRegistry::restore(uint64_t token) {
  const auto found = std::find_if(snapshots_.begin(), snapshots_.end(),
                                  [token](const Snapshot& state) {
                                    return state.token == token;
                                  });
  if (found == snapshots_.end()) {
    return false;
  }
  records_ = found->records;
  uint32_t highest_revision = 0;
  for (const Record& record : records_) {
    highest_revision = std::max(highest_revision, record.revision);
  }
  revision_clock_ = highest_revision + 1;
  rebuild_index();
  return true;
}

bool LocaleRegistry::discard_snapshot(uint64_t token) {
  const auto found = std::find_if(snapshots_.begin(), snapshots_.end(),
                                  [token](const Snapshot& state) {
                                    return state.token == token;
                                  });
  if (found == snapshots_.end()) {
    return false;
  }
  snapshots_.erase(found);
  return true;
}

std::vector<uint64_t> LocaleRegistry::snapshot_tokens() const {
  std::vector<uint64_t> tokens;
  tokens.reserve(snapshots_.size());
  for (const Snapshot& state : snapshots_) {
    tokens.push_back(state.token);
  }
  std::sort(tokens.begin(), tokens.end());
  return tokens;
}

uint64_t LocaleRegistry::mix(uint64_t value) noexcept {
  return hash_number(value ^ kDomainSalt);
}

std::string LocaleRegistry::normalize_name(std::string value) {
  auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char byte) {
    return std::isspace(byte) != 0;
  });
  auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char byte) {
    return std::isspace(byte) != 0;
  }).base();
  if (first >= last) {
    return {};
  }
  std::string result(first, last);
  bool previous_space = false;
  for (char& character : result) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if (std::isspace(byte) != 0) {
      if (previous_space) {
        character = '\0';
      } else {
        character = ' ';
      }
      previous_space = true;
    } else {
      character = static_cast<char>(std::tolower(byte));
      previous_space = false;
    }
  }
  result.erase(std::remove(result.begin(), result.end(), '\0'), result.end());
  return result;
}

bool LocaleRegistry::record_equal(const Record& left, const Record& right) {
  return left.id == right.id && left.name == right.name && left.flags == right.flags &&
         same_metrics(left.metrics, right.metrics) && same_links(left.links, right.links) &&
         left.attributes == right.attributes;
}

LocaleRegistry::Delta LocaleRegistry::compare_record(const Record* left,
                                   const Record* right,
                                   uint32_t id) {
  Delta delta;
  delta.id = id;
  if (left == nullptr && right != nullptr) {
    delta.created = true;
    return delta;
  }
  if (left != nullptr && right == nullptr) {
    delta.removed = true;
    return delta;
  }
  if (left == nullptr || right == nullptr || record_equal(*left, *right)) {
    return delta;
  }
  delta.name_changed = left->name != right->name;
  delta.flags_changed = left->flags != right->flags;
  delta.metrics_changed = !same_metrics(left->metrics, right->metrics);
  delta.links_changed = !same_links(left->links, right->links);
  delta.attributes_changed = left->attributes != right->attributes;
  return delta;
}

void LocaleRegistry::rebuild_index() {
  index_.clear();
  index_.reserve(records_.size());
  for (std::size_t index = 0; index < records_.size(); ++index) {
    index_[records_[index].id] = index;
  }
}

void LocaleRegistry::touch(Record& record) {
  record.revision = revision_clock_++;
}

}  // namespace vellumgrid
