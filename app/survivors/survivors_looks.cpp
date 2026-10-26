#include "survivors/survivors_looks.h"

#include <optional>
#include <span>
#include <string>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/skill_hit_effect.h"

namespace moonclient {
namespace {

// L'effet que les tables du client donnent à ce skill, ou rien.
[[nodiscard]] std::optional<std::uint32_t> TabledEffectOf(std::uint16_t skill_id) {
  if (const auto* damage = ro::render::SkillDamageEffectOf(skill_id)) return damage->effect_id;
  for (const ro::render::SkillEffectEntry& entry : ro::render::SkillEffectTable()) {
    if (entry.skill_id == skill_id) return entry.effect_id;
  }
  if (const auto* announced = ro::render::SkillAnnouncedEffectOf(skill_id)) {
    return announced->effect_id;
  }
  return std::nullopt;
}

// Le repli écrit de ce skill, ou rien.
[[nodiscard]] std::optional<std::uint32_t> FallbackEffectOf(std::uint16_t skill_id) {
  switch (skill_id) {
    case kSightSkill:
      return kSightFallbackEffect;
    case kNapalmBeatSkill:
      return kNapalmBeatFallbackEffect;
    case kFireWallSkill:
      return kFireWallFallbackEffect;
    default:
      return std::nullopt;
  }
}

[[nodiscard]] SkillBearer BearerOf(ro::sim::SkillArchetype archetype) {
  switch (archetype) {
    case ro::sim::SkillArchetype::kHoming:
    case ro::sim::SkillArchetype::kOrbit:
    case ro::sim::SkillArchetype::kLinear:
    case ro::sim::SkillArchetype::kNova:
      return SkillBearer::kHero;
    case ro::sim::SkillArchetype::kTargetStrike:
    case ro::sim::SkillArchetype::kClusterBlast:
    case ro::sim::SkillArchetype::kWall:
    case ro::sim::SkillArchetype::kRing:
      return SkillBearer::kGround;
  }
  return SkillBearer::kGround;
}

// Les bruits du lancer : ceux de la compétence, sinon ceux de son effet, sauf
// quand la troupe de l'effet sonne elle-même.
[[nodiscard]] std::vector<ro::render::EffectSoundEntry> CastSoundsOf(const SkillLook& look) {
  std::vector<ro::render::EffectSoundEntry> sounds = ro::render::SkillSounds(look.skill_id);
  if (!sounds.empty() || ro::render::EffectSoundPlaysAtSowing(look.effect_id)) return sounds;
  const std::span<const ro::render::EffectSoundEntry> of_effect =
      ro::render::EffectSounds(look.effect_id);
  sounds.assign(of_effect.begin(), of_effect.end());
  return sounds;
}

}  // namespace

ro::render::ActorLook FoeLook(std::uint16_t foe_id) noexcept {
  return ro::render::ActorLook{.job = foe_id};
}

ro::render::ActorLook ItemLook(std::uint16_t item_id) noexcept {
  return ro::render::ActorLook{.item = true, .job = item_id};
}

std::uint16_t PickupLookOf(const ro::sim::SurvivorsPickup& pickup) noexcept {
  return pickup.kind == ro::sim::PickupKind::kCard ? kGroundCardItem : pickup.item_id;
}

ro::render::ActorLook HeroLook(const ro::render::ActorLook& player,
                               const ro::sim::ClassRule& hero_class, bool evolved) noexcept {
  ro::render::ActorLook look;
  look.player = true;
  look.job =
      evolved && hero_class.evolved_job_id != 0 ? hero_class.evolved_job_id : hero_class.job_id;
  look.sex = player.sex;
  look.hair = player.hair;
  look.headgears = player.headgears;
  look.weapon = hero_class.weapon_item_id;
  look.robe = player.robe;
  look.hair_palette = player.hair_palette;
  // La couleur de vêtement du serveur se traduit par métier : elle vaut sur le
  // corps du mode. Une recette de style, elle, est faite pour le corps du
  // personnage, et sur un autre elle peindrait au hasard.
  look.body_palette = player.body_palette_names_file ? 0 : player.body_palette;
  return look;
}

SkillLook SkillLookOf(const ro::sim::SurvivorsSkillRule& skill) {
  SkillLook look{.skill_id = skill.skill_id,
                 .bearer = BearerOf(skill.archetype),
                 .icon_skill_id = skill.skill_id};
  if (const std::optional<std::uint32_t> tabled = TabledEffectOf(skill.skill_id)) {
    look.effect_id = *tabled;
  } else if (const std::optional<std::uint32_t> fallback = FallbackEffectOf(skill.skill_id)) {
    look.effect_id = *fallback;
    look.fallback = true;
  }
  const std::int32_t hit = ro::render::SkillHitEffect(skill.skill_id, true, true);
  look.hit_effect_id = hit >= 0 ? static_cast<std::uint32_t>(hit) : kPlainHitEffect;
  return look;
}

std::vector<std::string> SkillSoundFilesOf(const SkillLook& look, std::uint32_t draw) {
  std::vector<std::string> files;
  // Deux bruits d'un même lancer tirent chacun leur variante.
  for (const ro::render::EffectSoundEntry& entry : CastSoundsOf(look)) {
    files.push_back(ro::render::EffectSoundFile(entry, draw++));
  }
  return files;
}

bool SkillThudsOnHit(const SkillLook& look) {
  return !ro::render::EffectSoundPlaysAtSowing(look.effect_id) && CastSoundsOf(look).empty();
}

}  // namespace moonclient
