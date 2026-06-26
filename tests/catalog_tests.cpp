#include "vellumgrid/catalog/asset_registry.h"
#include "vellumgrid/catalog/document_property_store.h"
#include "vellumgrid/catalog/export_preset_store.h"
#include "vellumgrid/catalog/font_family_catalog.h"
#include "vellumgrid/catalog/locale_registry.h"
#include "vellumgrid/catalog/master_page_catalog.h"
#include "vellumgrid/catalog/metadata_store.h"
#include "vellumgrid/catalog/plugin_registry.h"
#include "vellumgrid/catalog/security_policy_store.h"
#include "vellumgrid/catalog/style_rule_store.h"
#include "vellumgrid/catalog/template_catalog.h"
#include "vellumgrid/catalog/workspace_catalog.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

template <typename Catalog>
void exercise_catalog(Catalog& catalog, uint32_t base) {
  auto& first = catalog.upsert(base, " Primary Record ");
  auto& second = catalog.upsert(base + 1, "Secondary Record");
  assert(first.id == base);
  assert(second.id == base + 1);
  assert(catalog.size() == 2);

  assert(catalog.set_flags(base, 0x03));
  assert(catalog.enable_flags(base, 0x04));
  assert(catalog.has_flags(base, 0x07));
  assert(catalog.disable_flags(base, 0x02));
  assert(!catalog.has_flags(base, 0x07));

  assert(catalog.append_metric(base, 10));
  assert(catalog.append_metric(base, 20));
  assert(catalog.append_metric(base, -5));
  assert(catalog.metric_sum(base) == 25);
  assert(catalog.metric_min(base).value() == -5);
  assert(catalog.metric_max(base).value() == 20);

  assert(catalog.add_link(base, base + 1));
  assert(catalog.links_to(base, base + 1));
  assert(catalog.incoming_links(base + 1).size() == 1);
  assert(catalog.reachable(base, 8).size() == 2);

  assert(catalog.set_attribute(base, " Owner ", "editor"));
  assert(catalog.attribute(base, "owner").value() == "editor");
  assert(catalog.attribute_keys(base).size() == 1);

  const uint64_t before = catalog.checksum();
  const uint64_t token = catalog.snapshot();
  assert(catalog.rename(base, "Renamed"));
  assert(catalog.checksum() != before);
  assert(catalog.restore(token));
  assert(catalog.checksum() == before);

  Catalog other;
  other.upsert(base + 2, "Imported");
  other.append_metric(base + 2, 7);
  catalog.merge_from(other, true);
  assert(catalog.contains(base + 2));
  assert(!catalog.diff(other).empty());

  std::vector<std::string> issues;
  assert(catalog.validate_all(&issues) == 0);
  catalog.normalize_all();
  catalog.compact();
}

int main() {
  vellumgrid::MetadataStore metadata;
  vellumgrid::MasterPageCatalog master_pages;
  vellumgrid::AssetRegistry assets;
  vellumgrid::FontFamilyCatalog fonts;
  vellumgrid::StyleRuleStore styles;
  vellumgrid::ExportPresetStore exports;
  vellumgrid::WorkspaceCatalog workspaces;
  vellumgrid::LocaleRegistry locales;
  vellumgrid::PluginRegistry plugins;
  vellumgrid::SecurityPolicyStore policies;
  vellumgrid::TemplateCatalog templates;
  vellumgrid::DocumentPropertyStore properties;

  exercise_catalog(metadata, 100);
  exercise_catalog(master_pages, 200);
  exercise_catalog(assets, 300);
  exercise_catalog(fonts, 400);
  exercise_catalog(styles, 500);
  exercise_catalog(exports, 600);
  exercise_catalog(workspaces, 700);
  exercise_catalog(locales, 800);
  exercise_catalog(plugins, 900);
  exercise_catalog(policies, 1000);
  exercise_catalog(templates, 1100);
  exercise_catalog(properties, 1200);
  return 0;
}
