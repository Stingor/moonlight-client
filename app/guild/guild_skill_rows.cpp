// Les compétences de guilde de l'onglet : l'arbre de guilde du serveur croisé
// avec ce que le serveur envoie.
//
// 🔴 LE PAQUET TAIT DEUX CHOSES : le niveau maximal, et les compétences dont
// les prérequis manquent — elles n'arrivent jamais. L'arbre de guilde que les
// bases du serveur exportent comble ce trou ; il est purement informatif, et le
// serveur reste seul juge d'une montée.

#include <algorithm>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/sim/world.h"

#include "game/skin_image_of.h"
#include "guild/guild_panel.h"

namespace moonclient {
namespace {

// Le libellé que les tables du client donnent à une compétence qu'elles ne
// nomment pas.
constexpr std::string_view kUnknownSkillLabel = "Unknown-Skill";
// Le bit de nature d'une compétence qui se lance sur soi, sans curseur : toutes
// les compétences de guilde. Seules celles-là se proposent au lancer.
constexpr std::int32_t kSelfTargetedInf = 0x04;

}  // namespace

void GuildPanel::EnsureGuildTree() {
  if (guild_tree_tried_ || pieces_.skills == nullptr) {
    return;
  }
  guild_tree_tried_ = true;
  for (const ro::sim::SkillRule& rule : pieces_.skills->GuildRules()) {
    const ro::sim::SkillNode* node = pieces_.skills->Find(rule.skill_id);
    guild_nodes_.push_back(GuildTreeNode{.id = rule.skill_id,
                                         .max_level = rule.max_level,
                                         .idname = node != nullptr ? node->idname : std::string(),
                                         .needs = rule.needs,
                                         .depth = 0});
  }
  if (guild_nodes_.empty()) {
    ro::core::LogWarning(
        "arbre de guilde : absent des règles du serveur — l'onglet se limitera à ce que le "
        "serveur envoie");
    return;
  }
  // LA PROFONDEUR : un de plus que le prérequis le plus profond. Par passes
  // successives — l'ordre des fiches est arbitraire — et bornées par leur
  // nombre, ce qui protège aussi d'un cycle.
  for (std::size_t pass = 0; pass < guild_nodes_.size(); ++pass) {
    bool changed = false;
    for (GuildTreeNode& node : guild_nodes_) {
      int depth = 0;
      for (const ro::sim::SkillRequirement& need : node.needs) {
        for (const GuildTreeNode& other : guild_nodes_) {
          if (other.id == need.skill_id) {
            depth = std::max(depth, other.depth + 1);
          }
        }
      }
      changed = changed || depth != node.depth;
      node.depth = depth;
    }
    if (!changed) {
      break;
    }
  }
  std::ranges::stable_sort(guild_nodes_, [](const GuildTreeNode& a, const GuildTreeNode& b) {
    return a.depth != b.depth ? a.depth < b.depth : a.id < b.id;
  });
  ro::core::LogInfo("arbre de guilde : {} compétence(s)", guild_nodes_.size());
}

void GuildPanel::RefreshSkills(const ro::sim::GuildBook& book, const ro::sim::World& world,
                               GuildPlayer player, std::uint32_t now_ms) {
  EnsureGuildTree();
  view_.skills_known = book.skills_known();
  view_.skill_points = book.skill_points();
  view_.skill_tree_known = !guild_nodes_.empty();

  // LE LIBELLÉ : d'abord la table du client — seule source pour une
  // verrouillée —, puis le nom technique du paquet.
  const auto node_of = [this](std::uint16_t id) -> const GuildTreeNode* {
    const auto found = std::ranges::find(guild_nodes_, id, &GuildTreeNode::id);
    return found == guild_nodes_.end() ? nullptr : &*found;
  };
  const auto label_of = [this, &book](std::uint16_t id) -> std::string_view {
    if (pieces_.skills != nullptr) {
      const ro::sim::SkillNode* node = pieces_.skills->Find(id);
      if (node != nullptr && !node->display_name.empty() &&
          node->display_name != kUnknownSkillLabel) {
        return node->display_name;
      }
    }
    const ro::sim::GuildSkill* live = book.FindSkill(id);
    return live != nullptr ? std::string_view(live->name) : std::string_view();
  };

  // Les rangées : l'ARBRE quand il est là, sinon la seule liste du serveur.
  std::vector<std::uint16_t> ids;
  if (!guild_nodes_.empty()) {
    for (const GuildTreeNode& node : guild_nodes_) {
      ids.push_back(node.id);
    }
  } else {
    for (const ro::sim::GuildSkill& skill : book.skills()) {
      ids.push_back(skill.id);
    }
  }

  // ⚠ LES PRÉREQUIS ET LEUR TEXTE D'ABORD, les vues ensuite : elles pointent
  // dedans, et ces vecteurs ne doivent plus bouger une fois visés.
  skill_needs_.assign(ids.size(), {});
  skill_requirements_.assign(ids.size(), {});
  for (std::size_t index = 0; index < ids.size(); ++index) {
    const GuildTreeNode* node = node_of(ids[index]);
    if (node == nullptr) {
      continue;
    }
    for (const ro::sim::SkillRequirement& need : node->needs) {
      skill_needs_[index].push_back(
          ro::ui::SkillNeedView{.skill_id = need.skill_id, .level = need.level});
      const std::string label(label_of(need.skill_id));
      if (!skill_requirements_[index].empty()) {
        skill_requirements_[index] += ", ";
      }
      skill_requirements_[index] +=
          ro::core::Printed(ro::core::Tr("%s Niv %d"), label.empty() ? "?" : label.c_str(),
                            static_cast<int>(need.level));
    }
  }

  skills_.clear();
  skills_.reserve(ids.size());
  for (std::size_t index = 0; index < ids.size(); ++index) {
    const std::uint16_t id = ids[index];
    const GuildTreeNode* node = node_of(id);
    const ro::sim::GuildSkill* live = book.FindSkill(id);
    ro::ui::GuildSkillView skill;
    skill.locked = live == nullptr;
    skill.depth = node != nullptr ? node->depth : 0;
    skill.active = live != nullptr && live->inf != 0;
    skill.wieldable = live != nullptr && book.MayWield(id, player.char_id);
    skill.technical = live != nullptr   ? std::string_view(live->name)
                      : node != nullptr ? std::string_view(node->idname)
                                        : std::string_view();
    skill.requirements = skill_requirements_[index];
    ro::ui::SkillCellView& cell = skill.cell;
    cell.skill_id = id;
    cell.name = label_of(id);
    cell.learned = live != nullptr ? live->level : 0;
    cell.max_level = node != nullptr ? node->max_level : 0;
    cell.raisable = live != nullptr && live->upgradable;
    cell.needs = skill_needs_[index];
    cell.cooldown = world.skill_book().CooldownFraction(id, now_ms);
    cell.cooldown_ms = world.skill_book().CooldownRemainingMs(id, now_ms);
    cell.use_level = cell.learned;
    cell.castable =
        skill.wieldable && skill.active && cell.learned > 0 && (live->inf & kSelfTargetedInf) != 0;
    cell.sp_cost = live != nullptr ? live->sp : 0;
    cell.range_cells = live != nullptr ? live->range : 0;
    cell.passive = live != nullptr && live->inf == 0;
    if (pieces_.skill_icons != nullptr) {
      std::string_view idname = skill.technical;
      if (pieces_.skills != nullptr) {
        if (const ro::sim::SkillNode* known = pieces_.skills->Find(id); known != nullptr) {
          idname = known->idname;
        }
      }
      const ro::render::IconTexture& icon = pieces_.skill_icons->Get(id, idname);
      if (icon.usable()) {
        cell.icon = SkinImageOf(icon);
      }
    }
    skills_.push_back(skill);
  }
  view_.skills = skills_;
}

}  // namespace moonclient
