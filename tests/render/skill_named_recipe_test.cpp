#include "ro/render/skill_named_recipe.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/skill_named_table.h"

using Catch::Approx;
using ro::render::ExpandNamedEntry;
using ro::render::NamedEffectFlip;
using ro::render::NamedEffectPace;
using ro::render::NamedEffectPaceOf;
using ro::render::NamedEffectRecipe;
using ro::render::NamedEffectTurn;
using ro::render::NamedPaceDelayMs;
using ro::render::NamedScreenDegrees;
using ro::render::NamedSowing;
using ro::render::NamedSowingContext;
using ro::render::NamedSowingDeck;
using ro::render::SkillNamedEffectEntry;

namespace {

[[nodiscard]] std::vector<NamedSowing> Expand(const SkillNamedEffectEntry& entry,
                                              const NamedSowingContext& context) {
  NamedSowingDeck deck;
  std::uint32_t generator = 7;
  return ExpandNamedEntry(entry, context, deck, generator);
}

[[nodiscard]] SkillNamedEffectEntry WithRecipe(std::string_view name, NamedEffectRecipe recipe) {
  SkillNamedEffectEntry entry;
  entry.name = name;
  entry.recipe = recipe;
  return entry;
}

[[nodiscard]] NamedSowingContext AtAngle(float degrees) {
  NamedSowingContext context;
  context.screen_degrees = degrees;
  return context;
}

}  // namespace

TEST_CASE("Semis nommés : l'angle à l'écran part de la droite et descend", "[render][skillnamed]") {
  // L'ordonnée de l'écran DESCEND : vers le bas, c'est un quart de tour.
  CHECK(NamedScreenDegrees({0.0F, 0.0F}, {1.0F, 0.0F}) == Approx(0.0F));
  CHECK(NamedScreenDegrees({0.0F, 0.0F}, {0.0F, 1.0F}) == Approx(90.0F).margin(0.01F));
  CHECK(NamedScreenDegrees({0.0F, 0.0F}, {-1.0F, 0.0F}) == Approx(180.0F).margin(0.01F));
  CHECK(NamedScreenDegrees({0.0F, 0.0F}, {0.0F, -1.0F}) == Approx(270.0F).margin(0.01F));
  // Deux points confondus rendent zéro, comme chez le client.
  CHECK(NamedScreenDegrees({3.0F, 4.0F}, {3.0F, 4.0F}) == 0.0F);
}

TEST_CASE("Semis nommés : la lance choisit son quart", "[render][skillnamed]") {
  const SkillNamedEffectEntry lance =
      WithRecipe("dragonic_pierce_{}", NamedEffectRecipe::kPierceByScreenQuarter);
  // Vers le bas : dressée, retournée de haut en bas.
  std::vector<NamedSowing> sowing = Expand(lance, AtAngle(90.0F));
  REQUIRE(sowing.size() == 1);
  CHECK(sowing[0].name == "dragonic_pierce_vertical");
  CHECK(sowing[0].flip_y);
  CHECK_FALSE(sowing[0].flip_x);
  CHECK(sowing[0].turn_degrees == Approx(0.0F));
  // Vers la gauche : couchée, sans retournement, un demi-tour retranché.
  sowing = Expand(lance, AtAngle(200.0F));
  REQUIRE(sowing.size() == 1);
  CHECK(sowing[0].name == "dragonic_pierce_horizon");
  CHECK_FALSE(sowing[0].flip_x);
  CHECK(sowing[0].turn_degrees == Approx(20.0F));
  // Vers le haut : dressée, sans retournement, trois quarts retranchés.
  sowing = Expand(lance, AtAngle(250.0F));
  CHECK(sowing[0].name == "dragonic_pierce_vertical");
  CHECK_FALSE(sowing[0].flip_y);
  CHECK(sowing[0].turn_degrees == Approx(340.0F));
  // Vers la droite : couchée et retournée.
  sowing = Expand(lance, AtAngle(30.0F));
  CHECK(sowing[0].name == "dragonic_pierce_horizon");
  CHECK(sowing[0].flip_x);
  CHECK(sowing[0].turn_degrees == Approx(30.0F));
}

TEST_CASE("Semis nommés : le côté et le douzième de tour", "[render][skillnamed]") {
  const SkillNamedEffectEntry cote =
      WithRecipe("radiant_spear_{}down", NamedEffectRecipe::kSideByScreenAngle);
  CHECK(Expand(cote, AtAngle(180.0F))[0].name == "radiant_spear_rightdown");
  CHECK(Expand(cote, AtAngle(45.0F))[0].name == "radiant_spear_leftdown");
  CHECK(Expand(cote, AtAngle(90.0F))[0].name == "radiant_spear_leftdown");

  const SkillNamedEffectEntry wave =
      WithRecipe("chul_ho_sonic_claw_wave_%d", NamedEffectRecipe::kTwelfthByScreenAngle);
  // Cent cinq degrés de décalage, trente par douzième.
  CHECK(Expand(wave, AtAngle(0.0F))[0].name == "chul_ho_sonic_claw_wave_3");
  CHECK(Expand(wave, AtAngle(300.0F))[0].name == "chul_ho_sonic_claw_wave_1");
  // Sans cible, pas d'onde.
  CHECK(Expand(wave, NamedSowingContext{}).empty());
}

TEST_CASE("Semis nommés : le carré tiré sans remise se partage", "[render][skillnamed]") {
  const SkillNamedEffectEntry socle =
      WithRecipe("hells_drive_bottom", NamedEffectRecipe::kShuffledGridByLevel);
  const SkillNamedEffectEntry shard =
      WithRecipe("hells_drive_%02d", NamedEffectRecipe::kShuffledGridByLevel);
  NamedSowingContext context;
  context.level = 4;
  NamedSowingDeck deck;
  std::uint32_t generator = 3;
  const std::vector<NamedSowing> socles = ExpandNamedEntry(socle, context, deck, generator);
  const std::vector<NamedSowing> eclats = ExpandNamedEntry(shard, context, deck, generator);
  // Niveau quatre : un côté de cinq.
  REQUIRE(socles.size() == 5);
  REQUIRE(eclats.size() == 5);
  std::set<float> abscisses;
  for (std::size_t rang = 0; rang < socles.size(); ++rang) {
    // Le socle et l'éclat d'un même rang tombent au même endroit, au même
    // instant.
    CHECK(socles[rang].client_offset == eclats[rang].client_offset);
    CHECK(socles[rang].start_delay_ms == static_cast<std::int32_t>(rang) * 32);
    abscisses.insert(socles[rang].client_offset.x);
    CHECK(eclats[rang].name.starts_with("hells_drive_0"));
  }
  // Sans remise : chaque colonne une fois.
  CHECK(abscisses == std::set<float>{-10.0F, -5.0F, 0.0F, 5.0F, 10.0F});
}

TEST_CASE("Semis nommés : l'éventail, l'anneau, la dispersion, les salves",
          "[render][skillnamed]") {
  const std::vector<NamedSowing> vagues =
      Expand(WithRecipe("hitouakumu_%02d", NamedEffectRecipe::kShuffledFanWaves), {});
  CHECK(vagues.size() >= 45);
  CHECK(vagues.size() <= 57);
  CHECK(vagues.front().name == "hitouakumu_00");
  CHECK(vagues.back().name == "hitouakumu_02");

  const std::vector<NamedSowing> ring =
      Expand(WithRecipe("wild_shot", NamedEffectRecipe::kScatteredRing), {});
  REQUIRE(ring.size() == 8);
  for (std::size_t tir = 0; tir < ring.size(); ++tir) {
    CHECK(ring[tir].start_delay_ms == static_cast<std::int32_t>(tir) * 240);
  }
  // Le premier est droit devant, en profondeur : quinze ou vingt unités.
  CHECK(ring[0].client_offset.x == Approx(0.0F).margin(0.001F));
  CHECK((ring[0].client_offset.z == Approx(15.0F) || ring[0].client_offset.z == Approx(20.0F)));

  SkillNamedEffectEntry dispersion = WithRecipe("reiketsuhou", NamedEffectRecipe::kScatterByLevel);
  NamedSowingContext niveau;
  niveau.level = 5;
  CHECK(Expand(dispersion, niveau).size() == 5);
  niveau.level = 6;
  CHECK(Expand(dispersion, niveau).size() == 6);
  niveau.level = 12;
  const std::vector<NamedSowing> sept = Expand(dispersion, niveau);
  REQUIRE(sept.size() == 7);
  for (const NamedSowing& shard : sept) {
    CHECK(std::abs(shard.client_offset.x) <= 17.5F);
  }
  niveau.level = 0;
  CHECK(Expand(dispersion, niveau).empty());

  const std::vector<NamedSowing> salves =
      Expand(WithRecipe("dust_explosion", NamedEffectRecipe::kPairedQuarterBursts), {});
  REQUIRE(salves.size() == 6);
  CHECK(salves[1].start_delay_ms == 0);
  CHECK(salves[2].start_delay_ms == 200);
  CHECK(salves[5].start_delay_ms == 400);
}

TEST_CASE("Semis nommés : l'allure, l'orientation, le retournement", "[render][skillnamed]") {
  const NamedEffectPaceOf ordinary{4.0F, 6.0F};
  CHECK(NamedPaceDelayMs(NamedEffectPace::kTwoThirdsOfHit, ordinary) == 384);
  CHECK(NamedPaceDelayMs(NamedEffectPace::kCadenceTimesFiftyPlusThirty, ordinary) == 230);
  CHECK(NamedPaceDelayMs(NamedEffectPace::kCadenceTimesHundredPlusHundred, ordinary) == 500);
  CHECK(NamedPaceDelayMs(NamedEffectPace::kCadenceTimesHundredPlusHundred, std::nullopt) == 500);
  CHECK(NamedPaceDelayMs(NamedEffectPace::kTwoThirdsOfHit, std::nullopt) ==
        ro::render::kRecordStartDelay);

  SkillNamedEffectEntry dos;
  dos.name = "rhythmshooting_shot";
  dos.turn = NamedEffectTurn::kSourceToTarget;
  dos.turn_degrees = 180.0F;
  const std::vector<NamedSowing> rotated = Expand(dos, AtAngle(40.0F));
  REQUIRE(rotated.size() == 1);
  CHECK(rotated[0].turn_degrees == Approx(220.0F));

  SkillNamedEffectEntry impair;
  impair.name = "chul_ho_battering_hit";
  impair.flip_x = NamedEffectFlip::kOnOddHit;
  NamedSowingContext frappe;
  frappe.hit_index = 1;
  CHECK(Expand(impair, frappe)[0].flip_x);
  frappe.hit_index = 2;
  CHECK_FALSE(Expand(impair, frappe)[0].flip_x);

  SkillNamedEffectEntry face;
  face.name = "powerful_swing";
  face.flip_x = NamedEffectFlip::kWhenSourceFacesAway;
  NamedSowingContext direction;
  direction.source_sprite_direction = 6;
  CHECK(Expand(face, direction)[0].flip_x);
  direction.source_sprite_direction = 1;
  CHECK_FALSE(Expand(face, direction)[0].flip_x);

  SkillNamedEffectEntry element;
  element.name = "grenade_fragment_fire";
  element.only_at_level = 4;
  NamedSowingContext choix;
  choix.level = 4;
  CHECK(Expand(element, choix).size() == 1);
  choix.level = 3;
  CHECK(Expand(element, choix).empty());

  SkillNamedEffectEntry charme;
  charme.name = "four_charm_fire";
  charme.only_with_source_charm = 3;
  NamedSowingContext spheres;
  spheres.source_charm_element = 3;
  CHECK(Expand(charme, spheres).size() == 1);
  spheres.source_charm_element = 0;
  CHECK(Expand(charme, spheres).empty());
}
