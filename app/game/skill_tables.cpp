#include "game/skill_tables.h"

#include <expected>
#include <utility>

namespace moonclient {

std::vector<ro::sim::SkillNode> ToSkillNodes(std::span<const ro::vfs::SkillInfoRecord> records) {
  std::vector<ro::sim::SkillNode> nodes;
  nodes.reserve(records.size());
  for (const ro::vfs::SkillInfoRecord& record : records) {
    ro::sim::SkillNode node{.id = record.skill_id,
                            .idname = record.name,
                            .display_name = record.label,
                            .sp_by_level = record.sp_by_level,
                            .range_cells_by_level = record.range_by_level,
                            .level_matters = record.separate_levels};
    node.scale_by_level.reserve(record.scale_by_level.size());
    for (const ro::vfs::SkillInfoScale& scale : record.scale_by_level) {
      node.scale_by_level.push_back(ro::sim::SkillScaleCells{.x = scale.x, .y = scale.y});
    }
    nodes.push_back(std::move(node));
  }
  return nodes;
}

ro::sim::SkillTreeLayout ToSkillTreeLayout(const ro::vfs::SkillLayout& layout) {
  ro::sim::SkillTreeLayout converted;
  for (const ro::vfs::SkillLayoutGrid& grid : layout.grids) {
    std::vector<ro::sim::SkillSlot>& slots = converted.slots_by_job[grid.job];
    slots.reserve(grid.slots.size());
    for (const ro::vfs::SkillLayoutSlot& slot : grid.slots) {
      slots.push_back(ro::sim::SkillSlot{.position = slot.position, .skill_id = slot.skill_id});
    }
  }
  for (const ro::vfs::SkillLayoutLink& link : layout.inherit) {
    converted.parent_by_job.insert_or_assign(link.job, link.parent);
  }
  for (const ro::vfs::SkillLayoutLink& link : layout.origins) {
    converted.origin_by_job.insert_or_assign(link.job, link.parent);
  }
  for (const ro::vfs::SkillLayoutTabNames& named : layout.tab_names) {
    converted.tab_names_by_job.insert_or_assign(named.job, named.names);
  }
  return converted;
}

ro::core::Result<SkillTables> LoadSkillTables(ro::vfs::Vfs& vfs) {
  auto records = ro::vfs::LoadSkillInfo(vfs);
  if (!records) {
    return std::unexpected(records.error());
  }
  std::optional<ro::core::Error> layout_error;
  ro::sim::SkillTreeLayout layout;
  if (auto read = ro::vfs::LoadSkillLayout(vfs)) {
    layout = ToSkillTreeLayout(*read);
  } else {
    layout_error = read.error();
  }
  // L'arbre d'abord : les descriptions quittent ensuite les fiches.
  ro::sim::SkillTree tree =
      ro::sim::SkillTree::FromTables(ToSkillNodes(*records), std::move(layout));
  return SkillTables{.tree = std::move(tree),
                     .words = ro::vfs::SkillWords::FromRecords(*records),
                     .layout_error = std::move(layout_error)};
}

}  // namespace moonclient
