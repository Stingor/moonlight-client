#include "character/skill_description_view.h"

#include <string_view>

#include <imgui.h>

#include "ro/sim/world.h"

namespace moonclient {

SkillDescriptionView::SkillDescriptionView(const ro::sim::SkillTree* tree,
                                           const ro::vfs::SkillWords* words,
                                           ro::render::SkillIcons* icons)
    : tree_(tree), words_(words), icons_(icons) {}

void SkillDescriptionView::Refresh(std::uint16_t skill_id, const ro::sim::World& world) {
  view_ = ro::ui::SkillDescriptionView{};
  if (skill_id == 0) {
    return;
  }
  view_.skill_id = skill_id;

  const ro::sim::SkillNode* node = tree_ != nullptr ? tree_->Find(skill_id) : nullptr;
  if (node != nullptr) {
    view_.name = node->display_name;
  }
  if (words_ != nullptr) {
    view_.lines = words_->Of(skill_id);
  }

  // 🔴 LE COÛT VIENT DU SERVEUR QUAND IL A PARLÉ. Il l'annonce avec ce que le
  // personnage a appris, au rang appris ; la table du client, elle, ne connaît
  // que le barème. Les deux disent la même chose sur un serveur ordinaire — et
  // celui-ci n'est pas ordinaire.
  //
  // ⚠ REPLI SUR LE PREMIER RANG pour une compétence qu'on n'a pas : c'est ce
  // qu'elle coûtera si on l'apprend, et c'est la seule chose qu'on puisse en
  // dire. Zéro n'en est pas une : une compétence passive n'a pas de coût, et
  // l'écrire « 0 » se lirait « gratuite ».
  const ro::sim::KnownSkill* known = world.skill_book().Find(skill_id);
  if (known != nullptr) {
    view_.sp_cost = known->sp_cost;
  } else if (node != nullptr && !node->sp_by_level.empty()) {
    view_.sp_cost = node->sp_by_level.front();
  }

  // ── CE QUE LE SERVEUR A DIT, S'IL A DIT QUELQUE CHOSE ─────────────────────
  //
  // ⚠ VIDE EST LE CAS NORMAL : ces deux-là ne viennent pas avec la description,
  // ils se demandent quand on ouvre leur onglet. La fenêtre dit alors qu'elle
  // attend, et c'est ce qu'il faut qu'elle dise.
  cast_.clear();
  for (const ro::sim::SkillCastFacts& level : world.skill_facts().CastOf(skill_id)) {
    cast_.push_back(ro::ui::SkillCastLevelView{.cast_variable_ms = level.cast_variable_ms,
                                               .cast_fixed_ms = level.cast_fixed_ms,
                                               .cooldown_ms = level.cooldown_ms,
                                               .after_delay_ms = level.after_delay_ms});
  }
  view_.cast = cast_;

  if (const ro::sim::SkillDamageFacts* told = world.skill_facts().DamageOf(skill_id);
      told != nullptr) {
    view_.damage = ro::ui::SkillDamageView{.told = true,
                                           .status = told->status,
                                           .attack_kind = told->attack_kind,
                                           .level = told->level,
                                           .hits = told->hits,
                                           .least = told->least,
                                           .most = told->most,
                                           .average = told->average,
                                           .target_mob_id = told->target_mob_id,
                                           .target_name = told->target_name};
  }

  if (icons_ != nullptr) {
    const ro::render::IconTexture& icon = icons_->Get(
        skill_id, node != nullptr ? std::string_view(node->idname) : std::string_view());
    view_.icon = ro::ui::SkinImage{
        .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
        .width = static_cast<float>(icon.width_px),
        .height = static_cast<float>(icon.height_px)};
  }
}

}  // namespace moonclient
