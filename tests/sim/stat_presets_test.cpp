// LES PRÉRÉGLAGES DE STATS : le coût d'un point, le carnet, et le plan qui
// dépense les points libres dans l'ordre de la fiche.

#include "ro/sim/stat_presets.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::sim::BaseStatPointCost;
using ro::sim::BaseStats;
using ro::sim::PlanStatPreset;
using ro::sim::StatPlan;
using ro::sim::StatPlanInput;
using ro::sim::StatPlanVerdict;
using ro::sim::StatPresetBook;
using ro::sim::StatPresetRefusal;

namespace {

// Aucun coût annoncé : la règle sert pour chaque marche.
constexpr BaseStats kUntold = {-1, -1, -1, -1, -1, -1};
constexpr BaseStats kNovice = {1, 1, 1, 1, 1, 1};

[[nodiscard]] StatPlanInput Input(const BaseStats& current, std::int64_t points,
                                  const BaseStats& next_cost = kUntold) {
  return StatPlanInput{.current = current, .next_cost = next_cost, .status_point = points};
}

}  // namespace

TEST_CASE("le coût d'un point suit les trois paliers du serveur", "[stat_presets]") {
  CHECK(BaseStatPointCost(1) == 2);
  CHECK(BaseStatPointCost(10) == 2);
  CHECK(BaseStatPointCost(11) == 3);
  CHECK(BaseStatPointCost(99) == 11);
  CHECK(BaseStatPointCost(200) == 21);
  CHECK(BaseStatPointCost(201) == 42);
  CHECK(BaseStatPointCost(300) == 61);
  CHECK(BaseStatPointCost(301) == 122);
}

TEST_CASE("le plan sert STR avant AGI et s'arrête quand les points manquent", "[stat_presets]") {
  // STR de 1 à 12 : neuf points à 2, puis 2, puis 3 — vingt-trois. Restent
  // sept points : trois montées d'AGI à 2, puis il en manque.
  const BaseStats target = {12, 5, 1, 1, 1, 1};
  const StatPlan plan = PlanStatPreset(target, Input(kNovice, 30));
  CHECK(plan.rows[0].after == 12);
  CHECK(plan.rows[0].spent == 23);
  CHECK(plan.rows[0].verdict == StatPlanVerdict::kReached);
  CHECK(plan.rows[1].after == 4);
  CHECK(plan.rows[1].verdict == StatPlanVerdict::kShort);
  CHECK(plan.rows[1].missing_steps == 1);
  CHECK(plan.rows[1].missing_points == 2);
  CHECK(plan.points_before == 30);
  CHECK(plan.points_after == 1);
  CHECK(plan.raises() == 14);
  for (std::size_t rank = 2; rank < ro::sim::kBaseStatCount; ++rank) {
    CHECK(plan.rows[rank].verdict == StatPlanVerdict::kReached);
    CHECK(plan.rows[rank].after == 1);
  }
}

TEST_CASE("le plan ne saute pas une stat chère pour servir une moins chère", "[stat_presets]") {
  // STR 99 → 100 coûte 11, LUK 1 → 2 en coûterait 2 : avec cinq points, rien
  // ne part, et LUK attend son tour.
  const BaseStats current = {99, 1, 1, 1, 1, 1};
  const BaseStats target = {100, 1, 1, 1, 1, 2};
  const StatPlan plan = PlanStatPreset(target, Input(current, 5));
  CHECK(plan.rows[0].verdict == StatPlanVerdict::kShort);
  CHECK(plan.rows[0].missing_points == 11);
  CHECK(plan.rows[5].verdict == StatPlanVerdict::kShort);
  CHECK(plan.rows[5].after == 1);
  CHECK(plan.points_after == 5);
  CHECK(plan.raises() == 0);
}

TEST_CASE("le coût annoncé par le serveur passe avant la règle", "[stat_presets]") {
  const BaseStats announced = {5, -1, -1, -1, -1, -1};
  const BaseStats target = {3, 1, 1, 1, 1, 1};
  // Première marche à 5 (annoncée), la seconde à 2 (règle) : sept points.
  const StatPlan plan = PlanStatPreset(target, Input(kNovice, 7, announced));
  CHECK(plan.rows[0].after == 3);
  CHECK(plan.rows[0].spent == 7);
  CHECK(plan.points_after == 0);
}

TEST_CASE("une stat au-dessus de la cible reste telle quelle", "[stat_presets]") {
  const BaseStats current = {50, 1, 1, 1, 1, 1};
  const BaseStats target = {40, 3, 1, 1, 1, 1};
  const StatPlan plan = PlanStatPreset(target, Input(current, 100));
  CHECK(plan.rows[0].verdict == StatPlanVerdict::kAboveTarget);
  CHECK(plan.rows[0].after == 50);
  CHECK(plan.rows[0].spent == 0);
  // La suivante est servie quand même.
  CHECK(plan.rows[1].after == 3);
}

TEST_CASE("le plafond arrête une stat et laisse passer la suivante", "[stat_presets]") {
  SECTION("le serveur annonce un coût nul : au plafond") {
    const BaseStats announced = {0, -1, -1, -1, -1, -1};
    const BaseStats current = {99, 1, 1, 1, 1, 1};
    const BaseStats target = {120, 2, 1, 1, 1, 1};
    const StatPlan plan = PlanStatPreset(target, Input(current, 100, announced));
    CHECK(plan.rows[0].verdict == StatPlanVerdict::kCeiling);
    CHECK(plan.rows[0].after == 99);
    CHECK(plan.rows[0].missing_steps == 21);
    CHECK(plan.rows[0].missing_points == 0);
    CHECK(plan.rows[1].after == 2);
  }
  SECTION("le plafond du serveur Moonlight") {
    const BaseStats current = {998, 1, 1, 1, 1, 1};
    const BaseStats target = {1000, 1, 1, 1, 1, 1};
    const StatPlan plan = PlanStatPreset(target, Input(current, 1000));
    CHECK(plan.rows[0].after == ro::sim::kBaseStatCeiling);
    CHECK(plan.rows[0].verdict == StatPlanVerdict::kCeiling);
    CHECK(plan.rows[0].missing_steps == 1);
  }
}

TEST_CASE("le carnet enregistre, remplace, renomme et supprime", "[stat_presets]") {
  StatPresetBook book;
  const BaseStats agi = {1, 99, 1, 1, 40, 1};
  CHECK(book.Save("  AGI-crit 99  ", agi) == StatPresetRefusal::kNone);
  REQUIRE(book.all().size() == 1);
  CHECK(book.all()[0].name == "AGI-crit 99");

  // Le même nom met à jour, sans ajouter.
  const BaseStats later = {1, 99, 10, 1, 50, 1};
  CHECK(book.Save("AGI-crit 99", later) == StatPresetRefusal::kNone);
  REQUIRE(book.all().size() == 1);
  CHECK(book.all()[0].stats == later);

  CHECK(book.Save("   ", agi) == StatPresetRefusal::kEmptyName);
  CHECK(book.Save("INT 99", kNovice) == StatPresetRefusal::kNone);
  CHECK(book.Rename(1, "AGI-crit 99") == StatPresetRefusal::kNameTaken);
  CHECK(book.Rename(1, "Mage") == StatPresetRefusal::kNone);
  CHECK(book.all()[1].name == "Mage");
  CHECK(book.Rename(7, "x") == StatPresetRefusal::kUnknown);

  CHECK(book.Remove(0) == StatPresetRefusal::kNone);
  REQUIRE(book.all().size() == 1);
  CHECK(book.all()[0].name == "Mage");
  CHECK(book.Remove(3) == StatPresetRefusal::kUnknown);
}

TEST_CASE("le carnet a un plafond", "[stat_presets]") {
  StatPresetBook book;
  for (std::size_t count = 0; count < ro::sim::kStatPresetLimit; ++count) {
    REQUIRE(book.Save("p" + std::to_string(count), kNovice) == StatPresetRefusal::kNone);
  }
  CHECK(book.Save("encore un", kNovice) == StatPresetRefusal::kFull);
  // Remplacer reste permis carnet plein.
  CHECK(book.Save("p0", kNovice) == StatPresetRefusal::kNone);
}

TEST_CASE("un nom trop long se coupe sans couper un caractère", "[stat_presets]") {
  // Trente octets d'ASCII puis « é » (deux octets) : le trente-et-unième octet
  // est le premier de « é », qui ne tient plus.
  const std::string typed = std::string(30, 'a') + "\xC3\xA9" + "zz";
  const std::string tidy = ro::sim::TidyStatPresetName(typed);
  CHECK(tidy == std::string(30, 'a'));
  CHECK(tidy.size() < ro::sim::kStatPresetNameBytes);
}
