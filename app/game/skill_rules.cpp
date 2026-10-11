#include "game/skill_rules.h"

#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace moonclient {
namespace {

[[nodiscard]] std::vector<ro::sim::SkillRule> ToRules(
    std::span<const ro::vfs::ServerSkillRule> read) {
  std::vector<ro::sim::SkillRule> rules;
  rules.reserve(read.size());
  for (const ro::vfs::ServerSkillRule& one : read) {
    ro::sim::SkillRule rule{.skill_id = one.skill_id,
                            .max_level = one.max_level,
                            .base_level = one.base_level,
                            .job_level = one.job_level,
                            .by_points = one.by_points};
    rule.needs.reserve(one.needs.size());
    for (const auto& [skill_id, level] : one.needs) {
      rule.needs.push_back(ro::sim::SkillRequirement{.skill_id = skill_id, .level = level});
    }
    rules.push_back(std::move(rule));
  }
  return rules;
}

}  // namespace

ro::sim::SkillRules ToSkillRules(const ro::vfs::SkillTreeRules& read) {
  ro::sim::SkillRules rules{.guild = ToRules(read.guild()), .skill_free = read.skill_free()};
  for (const ro::vfs::ServerJobSkills& job : read.jobs()) {
    rules.by_job.insert_or_assign(job.job, ToRules(job.skills));
  }
  return rules;
}

ro::core::Result<ro::sim::SkillRules> LoadSkillRules(ro::vfs::Vfs& vfs) {
  auto read = ro::vfs::SkillTreeRules::Load(vfs);
  if (!read) {
    return std::unexpected(read.error());
  }
  return ToSkillRules(*read);
}

}  // namespace moonclient
