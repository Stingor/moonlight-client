#include "game/spell_list_stage.h"

#include "ro/core/log.h"
#include "ro/net/spell_list.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

// Autospell, dont le niveau appris fixe le plafond des sorts qu'il arme.
constexpr std::uint16_t kAutospellSkill = 279;

}  // namespace

void SpellListStage::Stage(const ro::sim::SkillTree* tree, ro::render::SkillIcons* icons,
                           const ro::sim::World& world, ro::ui::SpellListWindowState& state,
                           ro::ui::SpellListView& view) {
  state.open = offer_.open();
  rows_.clear();
  if (!offer_.open()) {
    view = ro::ui::SpellListView{};
    return;
  }
  const ro::sim::KnownSkill* autospell = world.skill_book().Find(kAutospellSkill);
  const ro::sim::Actor* self = world.Find(world.player_id());
  const bool sage_linked = self != nullptr && self->status_effects.contains(ro::sim::kEfstSoulLink);
  for (const std::uint16_t skill_id : offer_.skill_ids()) {
    const ro::sim::SkillNode* node = tree != nullptr ? tree->Find(skill_id) : nullptr;
    const ro::sim::KnownSkill* learned = world.skill_book().Find(skill_id);
    ro::ui::SpellListRow row{
        .skill_id = skill_id,
        .name = node != nullptr ? std::string_view(node->display_name) : std::string_view(),
        .armed_level =
            ro::sim::AutospellArmedLevel(skill_id, autospell != nullptr ? autospell->level : 0,
                                         learned != nullptr ? learned->level : 0, sage_linked)};
    if (icons != nullptr && node != nullptr) {
      const ro::render::IconTexture& icon = icons->Get(skill_id, node->idname);
      if (icon.usable()) {
        row.icon = SkinImageOf(icon);
      }
    }
    rows_.push_back(row);
  }
  view = ro::ui::SpellListView{.rows = rows_};
}

void SpellListStage::Apply(const ro::ui::SpellListWindowAction& asked, ro::net::Connection& link) {
  if (!asked.answer.has_value()) {
    return;
  }
  if (auto sent = ro::net::RequestSpellChoice(link, *asked.answer); !sent) {
    ro::core::LogWarning("liste des sorts : réponse non émise — {}",
                         ro::core::Describe(sent.error()));
  }
  offer_.Close();
}

}  // namespace moonclient
