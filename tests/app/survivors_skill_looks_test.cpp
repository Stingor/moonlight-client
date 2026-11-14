// LA TABLE D'APPARENCE DES SKILLS DU MODE : chaque skill des règles livrées a
// un effet que l'on sait dessiner — par les tables du client ou par un des
// trois replis écrits — et une icône ; aucun effet ne naît sur un monstre ;
// chaque lancer fait entendre un nom de fichier formé, ou laisse sonner sa
// troupe ou ses coups.

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_table.h"
#include "ro/render/primitive_str_loop.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/skill_unit_table.h"
#include "ro/render/str_effect_setup.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_rules.h"
#include "ro/vfs/skill_info.h"

#include "effects/effect_arrow_shower.h"
#include "effects/effect_node_view.h"
#include "survivors/arena_effect_plan.h"
#include "survivors/survivors_looks.h"

namespace {

// Assez de tirages pour que chaque variante d'un nom à trois paraisse.
constexpr std::uint32_t kDrawsTried = 24;

// Le dépôt se situe depuis ce fichier : le crible Linux ne pose pas les
// définitions de cible.
[[nodiscard]] std::string ShippedTable(const char* name) {
  const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / ".." /
                                     "dist" / "data" / "yaml_files" / name;
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

[[nodiscard]] const ro::sim::SurvivorsSkillRule& SkillRule(std::uint16_t skill_id) {
  const ro::sim::SurvivorsSkillRule* rule =
      ro::sim::FindSkill(ro::sim::ShippedSurvivorsRules(), skill_id);
  REQUIRE(rule != nullptr);
  return *rule;
}

// Un effet se dessine s'il a un fichier ou si la troupe le connaît.
[[nodiscard]] bool Drawable(std::uint32_t effect_id) {
  return !ro::render::EffectFile(effect_id).empty() ||
         moonclient::EffectNodesInPlay::KnowsProcedural(effect_id);
}

// LE PORTEUR ATTENDU DE CHAQUE ARCHÉTYPE, écrit en toutes lettres : orbite,
// projectile autoguidé ou droit et nova sur le héros, le reste au sol.
[[nodiscard]] std::optional<moonclient::SkillBearer> ExpectedBearer(
    ro::sim::SkillArchetype archetype) {
  using ro::sim::SkillArchetype;
  struct Row {
    SkillArchetype archetype;
    moonclient::SkillBearer bearer;
  };
  constexpr std::array<Row, 15> kRows = {{
      {SkillArchetype::kOrbit, moonclient::SkillBearer::kHero},
      {SkillArchetype::kHoming, moonclient::SkillBearer::kHero},
      {SkillArchetype::kLinear, moonclient::SkillBearer::kHero},
      {SkillArchetype::kNova, moonclient::SkillBearer::kHero},
      {SkillArchetype::kTargetStrike, moonclient::SkillBearer::kGround},
      {SkillArchetype::kClusterBlast, moonclient::SkillBearer::kGround},
      {SkillArchetype::kWall, moonclient::SkillBearer::kGround},
      {SkillArchetype::kStorm, moonclient::SkillBearer::kGround},
      {SkillArchetype::kRing, moonclient::SkillBearer::kGround},
      {SkillArchetype::kCone, moonclient::SkillBearer::kHero},
      {SkillArchetype::kBarrier, moonclient::SkillBearer::kGround},
      {SkillArchetype::kStance, moonclient::SkillBearer::kHero},
      {SkillArchetype::kReflex, moonclient::SkillBearer::kHero},
      // Le coup d'un allié tombe sur son monstre, un soin sur le héros soigné.
      {SkillArchetype::kSummon, moonclient::SkillBearer::kGround},
      {SkillArchetype::kMend, moonclient::SkillBearer::kGround},
  }};
  for (const Row& row : kRows) {
    if (row.archetype == archetype) return row.bearer;
  }
  // Un archétype sans ligne ne vaut aucun porteur : l'essai échoue.
  return std::nullopt;
}

// Si l'effet retenu pour ce skill est un effet que le client fait naître sur
// la cible, d'après la table d'où il vient, dans l'ordre où `SkillLookOf` les
// lit. Un repli écrit ne vient d'aucune table.
//
// ⚠ UNE NOVA SE LANCE SUR SOI : sa cible, pour le client, est le lanceur, et
// l'anneau de Frost Nova que la table pose « sur la cible » naît sur le héros.
[[nodiscard]] bool BornOnTarget(const moonclient::SkillLook& look,
                                ro::sim::SkillArchetype archetype) {
  // Une posture se lance sur soi : sa cible est le héros qui la porte.
  if (look.fallback || archetype == ro::sim::SkillArchetype::kNova ||
      archetype == ro::sim::SkillArchetype::kStance) {
    return false;
  }
  const std::uint16_t shown = look.effect_skill_id;
  if (const auto* damage = ro::render::SkillDamageEffectOf(shown)) {
    return damage->on == ro::render::SkillEffectOn::kTarget;
  }
  for (const ro::render::SkillEffectEntry& entry : ro::render::SkillEffectTable()) {
    if (entry.skill_id == shown) return entry.on == ro::render::SkillEffectOn::kTarget;
  }
  if (const auto* announced = ro::render::SkillAnnouncedEffectOf(shown)) {
    return announced->on == ro::render::SkillEffectOn::kTarget;
  }
  return false;
}

}  // namespace

TEST_CASE("Apparence des skills : chaque skill livré a un effet dessinable et une icône",
          "[app][survivors]") {
  auto info = ro::vfs::ParseSkillInfo(ShippedTable("skill_info.yml"));
  REQUIRE(info.has_value());
  for (const ro::sim::SurvivorsSkillRule& skill : ro::sim::ShippedSurvivorsRules().skills) {
    INFO("skill " << skill.skill_id << " (" << skill.key << ")");
    const moonclient::SkillLook look = moonclient::SkillLookOf(skill);
    CHECK(look.effect_id != 0);
    CHECK(Drawable(look.effect_id));
    CHECK(Drawable(look.hit_effect_id));
    // 🔴 PORTEUR = CIBLE N'EXISTE PAS : la horde n'est pas dans le monde des
    // effets, un effet qui y naîtrait se perdrait en silence. Le porteur se
    // lit sur l'archétype, et un effet porté par le héros n'est pas un effet
    // que le client fait naître sur la cible.
    CHECK((ExpectedBearer(skill.archetype) == look.bearer));
    if (look.bearer == moonclient::SkillBearer::kHero)
      CHECK_FALSE(BornOnTarget(look, skill.archetype));
    // Le coup de l'arme n'est pas une compétence : son icône est celle de son arme.
    if (ro::sim::IsWeaponSkill(skill)) {
      CHECK(std::ranges::none_of(skill.weapon_item_ids,
                                 [](std::uint16_t item_id) { return item_id == 0; }));
      continue;
    }
    bool has_icon = false;
    for (const ro::vfs::SkillInfoRecord& record : *info) {
      if (record.skill_id == look.icon_skill_id && !record.name.empty()) has_icon = true;
    }
    CHECK(has_icon);
  }
}

TEST_CASE("Apparence des skills : les trois replis rendent exactement 22, 32 et 25",
          "[app][survivors]") {
  const moonclient::SkillLook sight = moonclient::SkillLookOf(SkillRule(moonclient::kSightSkill));
  const moonclient::SkillLook napalm =
      moonclient::SkillLookOf(SkillRule(moonclient::kNapalmBeatSkill));
  const moonclient::SkillLook fire_wall =
      moonclient::SkillLookOf(SkillRule(moonclient::kFireWallSkill));
  CHECK(sight.effect_id == 22);
  CHECK(sight.fallback);
  CHECK(sight.bearer == moonclient::SkillBearer::kHero);
  CHECK(napalm.effect_id == 32);
  CHECK(napalm.fallback);
  CHECK(napalm.bearer == moonclient::SkillBearer::kGround);
  CHECK(fire_wall.effect_id == 25);
  CHECK(fire_wall.fallback);
  CHECK(fire_wall.bearer == moonclient::SkillBearer::kGround);
}

TEST_CASE("Apparence des skills : chaque évolution a sa ligne, sans repli", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const moonclient::ArenaSkillLooks looks(rules);
  REQUIRE(rules.evolutions.size() == 42);
  for (const ro::sim::EvolutionRule& evolution : rules.evolutions) {
    INFO("évolution " << evolution.evolved_skill_id);
    const moonclient::SkillLook* look = looks.Find(evolution.evolved_skill_id);
    REQUIRE(look != nullptr);
    CHECK_FALSE(look->fallback);
    CHECK(Drawable(look->effect_id));
  }
}

TEST_CASE("Apparence des skills : Magnum Break montre ses anneaux au lancer", "[app][survivors]") {
  constexpr std::uint32_t kMagnumRingsEffect = 17;
  constexpr std::uint32_t kFireHitEffect = 49;
  const moonclient::SkillLook look =
      moonclient::SkillLookOf(SkillRule(moonclient::kMagnumBreakSkill));
  CHECK(look.effect_id == kMagnumRingsEffect);
  CHECK(look.hit_effect_id == kFireHitEffect);
  CHECK_FALSE(look.fallback);
}

TEST_CASE("Apparence des skills : l'Archer tire à l'arc et pose des pièges", "[app][survivors]") {
  const auto look_of = [](std::uint16_t skill_id) {
    return moonclient::SkillLookOf(SkillRule(skill_id));
  };
  // Ses tirs droits volent en flèches ; ceux de base n'ont que leur flèche.
  for (const std::uint16_t arrow :
       {moonclient::kDoubleStrafeSkill, moonclient::kChargeArrowSkill,
        moonclient::kBeastStrafingSkill, moonclient::kSharpShootingSkill,
        moonclient::kPhantasmicArrowSkill}) {
    INFO(arrow);
    CHECK(look_of(arrow).shoots_arrow);
    CHECK_FALSE(look_of(arrow).trap);
  }
  CHECK(look_of(moonclient::kDoubleStrafeSkill).fallback);
  constexpr std::uint32_t kSharpShootingEffect = 388;
  CHECK(look_of(moonclient::kBeastStrafingSkill).effect_id == kSharpShootingEffect);
  CHECK(look_of(moonclient::kArrowShowerSkill).effect_id == moonclient::kArrowShowerEffect);
  CHECK_FALSE(look_of(moonclient::kArrowShowerSkill).shoots_arrow);
  // Le faucon vole jusqu'où il frappe.
  CHECK(look_of(moonclient::kBlitzBeatSkill).effect_id == moonclient::kFalconDiveEffect);
  CHECK(look_of(moonclient::kFalconAssaultSkill).effect_id == moonclient::kFalconDiveEffect);
  // Les pièges : une marque sans flamme, et l'amorce des mines à chaque coup.
  for (const std::uint16_t trap : {moonclient::kAnkleSnareSkill, moonclient::kLandMineSkill,
                                   moonclient::kSkidTrapSkill, moonclient::kFreezingTrapSkill,
                                   moonclient::kClaymoreTrapSkill, moonclient::kBlastMineSkill}) {
    INFO(trap);
    CHECK(look_of(trap).trap);
    CHECK_FALSE(look_of(trap).shoots_arrow);
  }
  CHECK(look_of(moonclient::kLandMineSkill).hit_effect_id == moonclient::kLandMinePrimerEffect);
  CHECK(look_of(moonclient::kClaymoreTrapSkill).hit_effect_id == moonclient::kClaymorePrimerEffect);
  CHECK(look_of(moonclient::kAnkleSnareSkill).hit_effect_id != moonclient::kLandMinePrimerEffect);
}

TEST_CASE("Apparence des skills : le Voleur et ses emprunts", "[app][survivors]") {
  const auto look_of = [](std::uint16_t skill_id) {
    return moonclient::SkillLookOf(SkillRule(skill_id));
  };
  // L'effet du jeu, porté par le héros au lieu de naître sur la horde.
  constexpr std::uint32_t kSandEffect = 310;
  constexpr std::uint32_t kPoisonEffect = 125;
  CHECK(look_of(moonclient::kSprinkleSandSkill).effect_id == kSandEffect);
  CHECK(look_of(moonclient::kSprinkleSandSkill).bearer == moonclient::SkillBearer::kHero);
  CHECK(look_of(moonclient::kEnchantPoisonSkill).effect_id == kPoisonEffect);
  CHECK(look_of(moonclient::kEnchantPoisonSkill).bearer == moonclient::SkillBearer::kHero);
  // Ce qu'il jette vole vers sa cible, et aucune évolution ne vit d'un repli.
  constexpr std::uint32_t kStoneEffect = 308;
  constexpr std::uint32_t kKnifeEffect = 600;
  CHECK(look_of(moonclient::kThrowStoneSkill).effect_id == kStoneEffect);
  CHECK(look_of(moonclient::kThrowVenomKnifeSkill).effect_id == kKnifeEffect);
  CHECK_FALSE(look_of(moonclient::kThrowVenomKnifeSkill).fallback);
  // Ce que le jeu ne montre pas s'emprunte, l'icône restant la sienne.
  const moonclient::SkillLook slide = look_of(moonclient::kBackSlideSkill);
  CHECK(slide.effect_skill_id == moonclient::kBowlingBashSkill);
  CHECK(slide.icon_skill_id == moonclient::kBackSlideSkill);
  CHECK(look_of(moonclient::kImproveDodgeSkill).effect_skill_id == moonclient::kWindWalkSkill);
  CHECK(look_of(moonclient::kCloakingSkill).effect_skill_id == moonclient::kHidingSkill);
  CHECK(look_of(moonclient::kDoubleAttackSkill).fallback);
  // Venom Dust entretient le nuage de son unité tant que sa zone dure.
  const moonclient::SkillLook dust = look_of(moonclient::kVenomDustSkill);
  CHECK(dust.effect_id == moonclient::kVenomDustFallbackEffect);
  CHECK(dust.ground_effect_id == moonclient::kVenomDustFallbackEffect);
  // Demonstration brûle du feu de son unité, et non du mur de feu.
  const moonclient::SkillLook bomb = look_of(moonclient::kDemonstrationSkill);
  CHECK(bomb.effect_id == moonclient::kDemonstrationFallbackEffect);
  CHECK(bomb.ground_effect_id == moonclient::kDemonstrationFallbackEffect);
  // Acid Demonstration pose ses anneaux d'acide, et n'emprunte rien au poison.
  const moonclient::SkillLook acid = look_of(moonclient::kAcidDemonstrationSkill);
  CHECK(acid.effect_skill_id == moonclient::kAcidDemonstrationSkill);
  CHECK(acid.effect_id == moonclient::kAcidDemonstrationEffect);
  CHECK_FALSE(acid.fallback);
  // Les protections chimiques montrent leur propre bouclier, celui des tables.
  constexpr std::uint32_t kChemicalShieldEffect = 300;
  for (const std::uint16_t armor :
       {moonclient::kChemicalProtectionArmorSkill, moonclient::kFullChemicalProtectionSkill}) {
    INFO("skill " << armor);
    const moonclient::SkillLook shield = look_of(armor);
    CHECK(shield.effect_skill_id == armor);
    CHECK(shield.effect_id == kChemicalShieldEffect);
  }
}

TEST_CASE("Bruits des skills : Sonic Blow sonne à son lancer", "[app][survivors]") {
  const std::vector<std::string> files = moonclient::SkillSoundFilesOf(
      moonclient::SkillLookOf(SkillRule(moonclient::kSonicBlowSkill)), 0);
  REQUIRE(files.size() == 1);
  CHECK(files.front() == R"(effect\assasin_sonicblow.wav)");
}

TEST_CASE("Apparence des skills : Frenzy montre la furie et sonne comme sa base",
          "[app][survivors]") {
  constexpr std::uint32_t kFuryEffect = 756;
  const moonclient::SkillLook look = moonclient::SkillLookOf(SkillRule(moonclient::kFrenzySkill));
  CHECK(look.effect_id == kFuryEffect);
  CHECK(look.effect_skill_id == moonclient::kGiantGrowthSkill);
  CHECK(look.icon_skill_id == moonclient::kFrenzySkill);
  CHECK(look.bearer == moonclient::SkillBearer::kHero);
  const moonclient::SkillLook quicken =
      moonclient::SkillLookOf(SkillRule(moonclient::kTwoHandQuickenSkill));
  CHECK(moonclient::SkillSoundFilesOf(look, 0) == moonclient::SkillSoundFilesOf(quicken, 0));
  CHECK_FALSE(moonclient::SkillSoundFilesOf(look, 0).empty());
}

TEST_CASE("Apparence des skills : l'effet d'une posture s'éteint avant sa reprise",
          "[app][survivors]") {
  // Une posture relance son effet à chaque recharge, et rien ne l'éteint : il
  // doit finir de lui-même, sans quoi ils s'empileraient sur le héros.
  std::size_t stances = 0;
  for (const ro::sim::SurvivorsSkillRule& skill : ro::sim::ShippedSurvivorsRules().skills) {
    if (skill.archetype != ro::sim::SkillArchetype::kStance) continue;
    INFO("posture " << skill.key);
    ++stances;
    const moonclient::SkillLook look = moonclient::SkillLookOf(skill);
    // Un fichier se joue une fois, sauf ceux que le lecteur fait reboucler.
    if (!ro::render::EffectFile(look.effect_id).empty()) {
      CHECK_FALSE(ro::render::PrimitiveStrLoopsForLife(look.effect_id));
      const ro::render::StrEffectSetup* setup = ro::render::StrEffectSetupFor(look.effect_id);
      CHECK((setup == nullptr || !setup->loops));
    } else {
      // Ses nœuds vivent moins longtemps que la plus courte de ses recharges.
      const std::int64_t life_ms =
          std::int64_t{moonclient::EffectNodesInPlay::ProceduralLifeTicks(look.effect_id)} *
          ro::render::kEffectTickMs;
      CHECK(life_ms > 0);
      for (const ro::sim::SkillLevel& level : skill.levels) {
        CHECK(life_ms < std::int64_t{level.cooldown_ms});
      }
    }
  }
  CHECK(stances == 12U);
}

TEST_CASE("Apparence des skills : le feu montre son éclat de fichier à l'impact",
          "[app][survivors]") {
  // Fire Bolt pose l'éclat de feu sur ce qu'il touche, un fichier à trois
  // variantes ; Soul Strike, la gerbe ordinaire.
  constexpr std::uint16_t kFireBoltSkill = 19;
  constexpr std::uint16_t kSoulStrikeSkill = 13;
  CHECK(moonclient::SkillLookOf(SkillRule(kFireBoltSkill)).hit_effect_id == 49);
  CHECK(moonclient::SkillLookOf(SkillRule(kSoulStrikeSkill)).hit_effect_id == 0);
}

TEST_CASE("Bruits des skills : chaque lancer sonne, sous un nom de fichier formé",
          "[app][survivors]") {
  for (const ro::sim::SurvivorsSkillRule& rule : ro::sim::ShippedSurvivorsRules().skills) {
    const moonclient::SkillLook look = moonclient::SkillLookOf(rule);
    INFO("skill " << rule.key);
    for (std::uint32_t draw = 0; draw < kDrawsTried; ++draw) {
      const std::vector<std::string> files = moonclient::SkillSoundFilesOf(look, draw);
      // Muet au lancer, un sort sonne par sa troupe ou à l'impact, jamais des
      // deux façons ni d'aucune.
      if (files.empty()) {
        CHECK(ro::render::EffectSoundPlaysAtSowing(look.effect_id) !=
              moonclient::SkillThudsOnHit(look));
      } else {
        CHECK_FALSE(moonclient::SkillThudsOnHit(look));
      }
      for (const std::string& file : files) {
        INFO("fichier " << file);
        CHECK_FALSE(file.empty());
        CHECK(file.find('%') == std::string::npos);
      }
    }
  }
}

TEST_CASE("Bruits des skills : les trois bolts et Soul Strike", "[app][survivors]") {
  constexpr std::uint16_t kFireBoltSkill = 19;
  constexpr std::uint16_t kColdBoltSkill = 14;
  constexpr std::uint16_t kLightningBoltSkill = 20;
  constexpr std::uint16_t kSoulStrikeSkill = 13;
  const auto files_of = [](std::uint16_t skill_id, std::uint32_t draw) {
    return moonclient::SkillSoundFilesOf(moonclient::SkillLookOf(SkillRule(skill_id)), draw);
  };
  // Fire Bolt et Cold Bolt tirent une de leurs trois variantes.
  const std::set<std::string> fire = {R"(effect\EF_FireArrow1.wav)", R"(effect\EF_FireArrow2.wav)",
                                      R"(effect\EF_FireArrow3.wav)"};
  const std::set<std::string> cold = {R"(effect\EF_IceArrow1.wav)", R"(effect\EF_IceArrow2.wav)",
                                      R"(effect\EF_IceArrow3.wav)"};
  std::set<std::string> fire_heard;
  for (std::uint32_t draw = 0; draw < kDrawsTried; ++draw) {
    const std::vector<std::string> fire_files = files_of(kFireBoltSkill, draw);
    const std::vector<std::string> cold_files = files_of(kColdBoltSkill, draw);
    REQUIRE(fire_files.size() == 1);
    REQUIRE(cold_files.size() == 1);
    CHECK(fire.contains(fire_files.front()));
    CHECK(cold.contains(cold_files.front()));
    fire_heard.insert(fire_files.front());
  }
  CHECK(fire_heard.size() > 1);
  CHECK_FALSE(moonclient::SkillThudsOnHit(moonclient::SkillLookOf(SkillRule(kFireBoltSkill))));
  // Lightning Bolt n'a de bruit ni à lui ni à son effet : muet au lancer, il
  // fait entendre à chaque coup le coup sourd d'une compétence.
  CHECK(files_of(kLightningBoltSkill, 0).empty());
  CHECK(moonclient::SkillThudsOnHit(moonclient::SkillLookOf(SkillRule(kLightningBoltSkill))));
  // Soul Strike sonne par ses boules, une à une, et non à l'impact.
  CHECK(files_of(kSoulStrikeSkill, 0).empty());
  CHECK_FALSE(moonclient::SkillThudsOnHit(moonclient::SkillLookOf(SkillRule(kSoulStrikeSkill))));
}

TEST_CASE("Apparence des skills : chaque piège armé pose le modèle d'une unité du jeu",
          "[app][survivors]") {
  for (const ro::sim::SurvivorsSkillRule& skill : ro::sim::ShippedSurvivorsRules().skills) {
    if (!moonclient::SkillLookOf(skill).trap || skill.skill_id == moonclient::kSphereMineSkill) {
      continue;
    }
    INFO("skill " << skill.skill_id);
    const std::optional<std::uint32_t> unit_id = moonclient::TrapUnitOf(skill.skill_id);
    REQUIRE(unit_id.has_value());
    CHECK_FALSE(ro::render::SkillUnitModelPath(*unit_id).empty());
  }
}
