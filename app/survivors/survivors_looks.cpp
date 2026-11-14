#include "survivors/survivors_looks.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include "ro/render/attack_sound.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/skill_hit_effect.h"
#include "ro/render/skill_unit_table.h"
#include "ro/render/sprite_frame.h"

#include "effects/effect_arrow_shower.h"

namespace moonclient {
namespace {

// L'effet que les tables du client donnent à ce skill, ou rien.
[[nodiscard]] std::optional<std::uint32_t> TabledEffectOf(std::uint16_t skill_id) {
  if (skill_id == kMagnumBreakSkill) {
    if (const auto* announced = ro::render::SkillAnnouncedEffectOf(skill_id)) {
      return announced->effect_id;
    }
  }
  if (const auto* damage = ro::render::SkillDamageEffectOf(skill_id)) return damage->effect_id;
  for (const ro::render::SkillEffectEntry& entry : ro::render::SkillEffectTable()) {
    if (entry.skill_id == skill_id) return entry.effect_id;
  }
  if (const auto* announced = ro::render::SkillAnnouncedEffectOf(skill_id)) {
    return announced->effect_id;
  }
  return std::nullopt;
}

// L'amorce d'un piège, celle de la mine pour les pièges qui n'en ont pas ; rien
// pour un skill qui n'est pas un piège.
[[nodiscard]] std::optional<std::uint32_t> TrapPrimerOf(std::uint16_t skill_id) {
  switch (skill_id) {
    case kAnkleSnareSkill:
    case kSkidTrapSkill:
    case kLandMineSkill:
    case kSphereMineSkill:
      return kLandMinePrimerEffect;
    case kFreezingTrapSkill:
      return kFreezingTrapPrimerEffect;
    case kClaymoreTrapSkill:
      return kClaymorePrimerEffect;
    case kBlastMineSkill:
      return kBlastMinePrimerEffect;
    default:
      return std::nullopt;
  }
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
    case kIceWallSkill:
      return kIceWallFallbackEffect;
    case kBashSkill:
      return kBashFallbackEffect;
    case kDoubleAttackSkill:
      return kDoubleAttackFallbackEffect;
    case kVenomDustSkill:
      return kVenomDustFallbackEffect;
    case kDoubleStrafeSkill:
    case kChargeArrowSkill:
      return kArrowFallbackEffect;
    case kArrowShowerSkill:
      return kArrowShowerEffect;
    case kDemonstrationSkill:
      return kDemonstrationFallbackEffect;
    case kMammoniteSkill:
    case kBioCannibalizeSkill:
    case kMoonlightSkill:
    case kCastlingSkill:
      return kStrikeSparkFallbackEffect;
    default:
      return std::nullopt;
  }
}

// Vrai pour un skill dont les projectiles sont des flèches d'arc.
[[nodiscard]] bool ShootsArrow(std::uint16_t skill_id) {
  return skill_id == kDoubleStrafeSkill || skill_id == kChargeArrowSkill ||
         skill_id == kBeastStrafingSkill || skill_id == kSharpShootingSkill ||
         skill_id == kPhantasmicArrowSkill;
}

// L'effet de ce skill que le jeu fait naître sur sa cible, écrit pour que le
// héros le porte ; il passe avant les tables, qui le poseraient sur la horde.
[[nodiscard]] std::optional<std::uint32_t> HeroBorneFallbackOf(std::uint16_t skill_id) {
  switch (skill_id) {
    case kSprinkleSandSkill:
      return kSprinkleSandFallbackEffect;
    case kEnchantPoisonSkill:
      return kEnchantPoisonFallbackEffect;
    default:
      return std::nullopt;
  }
}

// L'objet que le lancer de ce skill fait voler, ou rien.
[[nodiscard]] std::optional<std::uint32_t> ThrownEffectOf(std::uint16_t skill_id) {
  switch (skill_id) {
    case kThrowStoneSkill:
    case kAcidTerrorSkill:
      return kThrownStoneEffect;
    case kThrowVenomKnifeSkill:
      return kThrownVenomKnifeEffect;
    case kBlitzBeatSkill:
    case kFalconAssaultSkill:
      return kFalconDiveEffect;
    default:
      return std::nullopt;
  }
}

// Le skill dont ce skill emprunte l'effet, ou lui-même.
[[nodiscard]] std::uint16_t ShownSkillOf(std::uint16_t skill_id) {
  switch (skill_id) {
    case kNapalmVulcanSkill:
      return kSoulStrikeSkill;
    case kBrandishSpearSkill:
      return kSpearStabSkill;
    case kSpiralPierceSkill:
      return kPierceSkill;
    case kFrenzySkill:
      return kGiantGrowthSkill;
    case kBackSlideSkill:
      return kBowlingBashSkill;
    case kImproveDodgeSkill:
      return kWindWalkSkill;
    case kCloakingSkill:
      return kHidingSkill;
    case kBeastStrafingSkill:
    case kPhantasmicArrowSkill:
      return kSharpShootingSkill;
    case kPotionPitcherSkill:
    case kSlimPotionPitcherSkill:
    case kTouchOfHealSkill:
    case kMentalChangeSkill:
      return kHealBorrowed;
    case kCartCannonSkill:
    case kBioExplosionSkill:
      return kFireBallBorrowed;
    case kCapriceSkill:
      return kFireBoltBorrowed;
    case kPlantCultivationSkill:
      return kEarthSpikeBorrowed;
    case kSbr44Skill:
      return kSonicBlowSkill;
    default:
      return skill_id;
  }
}

// Le skill dont ce skill emprunte le bruit de lancer, ou lui-même.
[[nodiscard]] std::uint16_t SoundingSkillOf(std::uint16_t skill_id) {
  return skill_id == kFrenzySkill ? kTwoHandQuickenSkill : skill_id;
}

// L'unité que ce skill pose au sol dans le jeu, ou rien.
[[nodiscard]] std::optional<std::uint32_t> GroundUnitOf(std::uint16_t skill_id) {
  if (skill_id == kFirePillarSkill) return kFirePillarWaitingUnit;
  if (skill_id == kIceWallSkill) return kIceWallUnit;
  if (skill_id == kVenomDustSkill) return kVenomDustUnit;
  if (skill_id == kDemonstrationSkill) return kDemonstrationUnit;
  return std::nullopt;
}

// Le visuel que la table des unités entretient sous ce skill, ou zéro.
[[nodiscard]] std::uint32_t GroundEffectOf(std::uint16_t skill_id) {
  const std::optional<std::uint32_t> unit_id = GroundUnitOf(skill_id);
  if (!unit_id.has_value()) return 0;
  const ro::render::SkillUnitVisualEntry* visual = ro::render::SkillUnitVisual(*unit_id);
  return visual != nullptr ? visual->effect_id : 0;
}

[[nodiscard]] SkillBearer BearerOf(ro::sim::SkillArchetype archetype) {
  switch (archetype) {
    case ro::sim::SkillArchetype::kHoming:
    case ro::sim::SkillArchetype::kOrbit:
    case ro::sim::SkillArchetype::kLinear:
    case ro::sim::SkillArchetype::kNova:
    case ro::sim::SkillArchetype::kCone:
    case ro::sim::SkillArchetype::kStance:
    case ro::sim::SkillArchetype::kReflex:
      return SkillBearer::kHero;
    // Un allié porte ses coups : son effet tombe sur le monstre qu'il frappe. Un
    // soin tombe sur le héros soigné, qui n'est pas toujours le lanceur.
    case ro::sim::SkillArchetype::kSummon:
    case ro::sim::SkillArchetype::kMend:
    case ro::sim::SkillArchetype::kTargetStrike:
    case ro::sim::SkillArchetype::kClusterBlast:
    case ro::sim::SkillArchetype::kWall:
    case ro::sim::SkillArchetype::kRing:
    case ro::sim::SkillArchetype::kStorm:
    case ro::sim::SkillArchetype::kBarrier:
      return SkillBearer::kGround;
  }
  return SkillBearer::kGround;
}

// Les bruits du lancer : ceux de la compétence, sinon ceux de son effet, sauf
// quand la troupe de l'effet sonne elle-même.
[[nodiscard]] std::vector<ro::render::EffectSoundEntry> CastSoundsOf(const SkillLook& look) {
  if (look.skill_id == kSonicBlowSkill) {
    const std::span<const ro::render::EffectSoundEntry> blows =
        ro::render::EffectSounds(kSonicBlowSoundingEffect);
    return std::vector<ro::render::EffectSoundEntry>(blows.begin(), blows.end());
  }
  // Une flèche sans effet de table ne joue pas l'étincelle de repli, ni à
  // l'image ni au son : elle sonne à l'impact (`kArrowHitSound`).
  if (look.shoots_arrow && look.fallback) return {};
  std::vector<ro::render::EffectSoundEntry> sounds =
      ro::render::SkillSounds(SoundingSkillOf(look.skill_id));
  if (!sounds.empty() || ro::render::EffectSoundPlaysAtSowing(look.effect_id)) return sounds;
  const std::span<const ro::render::EffectSoundEntry> of_effect =
      ro::render::EffectSounds(look.effect_id);
  sounds.assign(of_effect.begin(), of_effect.end());
  return sounds;
}

// Le coup de l'arme, selon sa forme : voir SkillLookOf.
[[nodiscard]] SkillLook WeaponLookOf(ro::sim::SkillArchetype archetype, SkillLook look) {
  look.hit_effect_id = kPlainHitEffect;
  switch (archetype) {
    case ro::sim::SkillArchetype::kHoming:
      look.effect_skill_id = kSoulStrikeSkill;
      look.effect_id = TabledEffectOf(kSoulStrikeSkill).value_or(kStrikeSparkFallbackEffect);
      break;
    case ro::sim::SkillArchetype::kLinear:
      look.effect_skill_id = look.skill_id;
      look.effect_id = kArrowFallbackEffect;
      look.fallback = true;
      look.shoots_arrow = true;
      look.shoots_wand_bolt = look.skill_id == kRodShotSkill;
      break;
    case ro::sim::SkillArchetype::kCone:
      look.effect_skill_id = kSpearStabSkill;
      look.effect_id = TabledEffectOf(kSpearStabSkill).value_or(kStrikeSparkFallbackEffect);
      break;
    default:
      look.effect_skill_id = look.skill_id;
      look.effect_id = kStrikeSparkFallbackEffect;
      look.fallback = true;
      break;
  }
  return look;
}

}  // namespace

ro::render::ActorLook FoeLook(std::uint16_t foe_id) noexcept {
  return ro::render::ActorLook{.job = foe_id};
}

std::optional<std::uint32_t> FoeShotEffectOf(std::uint16_t foe_id) noexcept {
  if (foe_id == kGazetiFoe) return kFrostDiverBoltEffect;
  return std::nullopt;
}

std::optional<SurvivorsStillPose> StillPoseOf(std::uint16_t foe_id) noexcept {
  // L'image où le glaçon touche le sol, puis celle où il éclate : relevées dans
  // l'attaque de son .act.
  constexpr SurvivorsStillPose kIceicle{.standing_frame = 5, .burst_frame = 6};
  if (foe_id == kIceicleFoe) return kIceicle;
  return std::nullopt;
}

ro::render::ActorLook ItemLook(std::uint16_t item_id) noexcept {
  return ro::render::ActorLook{.item = true, .job = item_id};
}

std::uint16_t PickupLookOf(const ro::sim::SurvivorsPickup& pickup) noexcept {
  return pickup.kind == ro::sim::PickupKind::kCard ? kGroundCardItem : pickup.item_id;
}

HeroBody HeroBodyOf(const ro::sim::ClassRule& hero_class,
                    const ro::sim::SurvivorsHero& hero) noexcept {
  return HeroBody{.job = ro::sim::HeroJobOf(hero_class, hero), .evolved = hero.evolutions > 0};
}

ro::render::ActorLook HeroLook(const ro::render::ActorLook& player,
                               const ro::sim::ClassRule& hero_class, HeroBody body) noexcept {
  ro::render::ActorLook look;
  look.player = true;
  look.job = body.job != 0 ? body.job : hero_class.job_id;
  look.sex = player.sex;
  look.hair = player.hair;
  look.headgears = player.headgears;
  look.weapon = body.evolved && hero_class.evolved_weapon_item_id != 0
                    ? hero_class.evolved_weapon_item_id
                    : hero_class.weapon_item_id;
  look.robe = player.robe;
  look.hair_palette = player.hair_palette;
  // La couleur de vêtement du serveur se traduit par métier : elle vaut sur le
  // corps du mode. Une recette de style, elle, est faite pour le corps du
  // personnage, et sur un autre elle peindrait au hasard.
  look.body_palette = player.body_palette_names_file ? 0 : player.body_palette;
  return look;
}

ro::render::ActorLook MeetingBodyLook(const ro::sim::ClassRule& hero_class) noexcept {
  constexpr std::uint8_t kMeetingSexMale = 1;
  constexpr std::uint16_t kMeetingHair = 2;
  return ro::render::ActorLook{.player = true,
                               .job = hero_class.job_id,
                               .sex = kMeetingSexMale,
                               .hair = kMeetingHair,
                               .weapon = hero_class.weapon_item_id};
}

std::size_t HeroAttackAnimation(const ro::render::SpriteCatalog& catalog,
                                const ro::sim::ClassRule& hero_class,
                                const ro::render::ActorLook& look) {
  // Le rang des taekwons seuls se tire au sort, et aucune classe n'en joue.
  constexpr std::uint8_t kNoDraw = 0;
  if (hero_class.evolved_weapon_item_id == 0) {
    return ro::render::AnimationFor(ro::render::Motion::kAttack, ro::render::SpriteLayout::kPlayer);
  }
  return ro::render::AttackAnimation(catalog, look, kNoDraw);
}

std::uint16_t WeaponItemShown(const ro::sim::SurvivorsSkillRule& skill,
                              const ro::sim::SurvivorsHero& hero) noexcept {
  if (!ro::sim::IsWeaponSkill(skill)) return 0;
  std::size_t level = 1;
  for (const ro::sim::OwnedSkill& owned : hero.skills) {
    if (owned.skill_id == skill.skill_id) level = owned.level;
  }
  const std::size_t index = std::clamp<std::size_t>(level, 1, skill.weapon_item_ids.size()) - 1;
  return skill.weapon_item_ids[index];
}

SkillLook SkillLookOf(const ro::sim::SurvivorsSkillRule& skill) {
  SkillLook look{.skill_id = skill.skill_id,
                 .bearer = BearerOf(skill.archetype),
                 .icon_skill_id = skill.skill_id};
  if (ro::sim::IsWeaponSkill(skill)) return WeaponLookOf(skill.archetype, look);
  const std::uint16_t shown = ShownSkillOf(skill.skill_id);
  look.effect_skill_id = shown;
  if (const std::optional<std::uint32_t> thrown = ThrownEffectOf(shown)) {
    look.effect_id = *thrown;
  } else if (const std::optional<std::uint32_t> primer = TrapPrimerOf(shown)) {
    look.effect_id = *primer;
  } else if (const std::optional<std::uint32_t> borne = HeroBorneFallbackOf(shown)) {
    look.effect_id = *borne;
    look.fallback = true;
  } else if (shown == kAcidDemonstrationSkill) {
    look.effect_id = kAcidDemonstrationEffect;
  } else if (const std::optional<std::uint32_t> tabled = TabledEffectOf(shown)) {
    look.effect_id = *tabled;
  } else if (const std::optional<std::uint32_t> fallback = FallbackEffectOf(shown)) {
    look.effect_id = *fallback;
    look.fallback = true;
  }
  const std::int32_t hit = ro::render::SkillHitEffect(shown, true, true);
  look.hit_effect_id = hit >= 0 ? static_cast<std::uint32_t>(hit) : kPlainHitEffect;
  look.ground_effect_id = GroundEffectOf(skill.skill_id);
  look.storm_sown = skill.skill_id == kHeavensDriveSkill;
  look.shoots_arrow = ShootsArrow(skill.skill_id);
  look.cart_spin = skill.skill_id == kCartRevolutionSkill || skill.skill_id == kCartTornadoSkill;
  if (const std::optional<std::uint32_t> primer = TrapPrimerOf(skill.skill_id)) {
    look.trap = true;
    const bool primed = skill.skill_id != kAnkleSnareSkill && skill.skill_id != kSkidTrapSkill;
    if (primed) look.hit_effect_id = *primer;
  }
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

std::string SkillHitSoundOf(const SkillLook& look, std::uint32_t draw) {
  if (look.shoots_arrow && !look.shoots_wand_bolt) return std::string(kArrowHitSound);
  // UN PIÈGE SONNE QUAND IL SAUTE, du bruit de son amorce ; celui qui n'en a pas
  // (Ankle Snare, Skid Trap) se tait.
  if (look.trap) {
    if (look.skill_id == kAnkleSnareSkill || look.skill_id == kSkidTrapSkill) return {};
    const std::vector<std::string> primer = SkillSoundFilesOf(look, draw);
    return primer.empty() ? std::string() : primer.front();
  }
  if (SkillThudsOnHit(look)) return ro::render::SkillBlowSound(draw);
  return {};
}

}  // namespace moonclient
