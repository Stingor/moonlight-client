// LES EFFETS D'USAGE D'OBJET : ce que la table engendrée dit d'un objet, et la
// ligne du manuel d'expérience selon le métier montré.

#include "ro/sim/item_use_effect.h"

#include <algorithm>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::sim::FindItemUseEffect;
using ro::sim::ItemUseEffect;
using ro::sim::ItemUseLine;
using ro::sim::ItemUseLineFor;

namespace {

constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kDurian = 684;
constexpr std::uint32_t kFruitOfMastela = 522;
constexpr std::uint32_t kInspectorCertificate = 662;
constexpr std::uint32_t kAssumptioScroll = 12218;
constexpr std::uint32_t kBathFoamA = 100147;
constexpr std::uint32_t kExpManual = 12563;
// Un plat de cuisine, que le client nomme par une plage et non par le switch.
constexpr std::uint32_t kFirstStrDish = 12041;
// L'objet le plus haut que le client compare.
constexpr std::uint32_t kHighestItem = 103941;

constexpr std::uint32_t kRuneKnight = 4054;
constexpr std::uint32_t kLastThirdJob = 4112;
constexpr std::uint32_t kNovice = 0;

}  // namespace

TEST_CASE("Usage d'objet : une potion rouge montre l'effet 204, sans son", "[sim][item_use]") {
  const ItemUseEffect* use = FindItemUseEffect(kRedPotion);
  REQUIRE(use != nullptr);
  CHECK(use->effect_count == 1);
  CHECK(use->effect_ids[0] == 204);
  CHECK(use->sound.empty());
}

TEST_CASE("Usage d'objet : deux effets dans l'ordre du client", "[sim][item_use]") {
  const ItemUseEffect* durian = FindItemUseEffect(kDurian);
  REQUIRE(durian != nullptr);
  REQUIRE(durian->effect_count == 2);
  CHECK(durian->effect_ids[0] == 261);
  CHECK(durian->effect_ids[1] == 220);

  const ItemUseEffect* assumptio = FindItemUseEffect(kAssumptioScroll);
  REQUIRE(assumptio != nullptr);
  REQUIRE(assumptio->effect_count == 2);
  CHECK(assumptio->effect_ids[0] == 375);
  CHECK(assumptio->effect_ids[1] == 440);

  // L'effet du dessus, puis celui du dessous.
  const ItemUseEffect* foam = FindItemUseEffect(kBathFoamA);
  REQUIRE(foam != nullptr);
  REQUIRE(foam->effect_count == 2);
  CHECK(foam->effect_ids[0] == 1597);
  CHECK(foam->effect_ids[1] == 1598);
}

TEST_CASE("Usage d'objet : un son seul, ou un son et son effet", "[sim][item_use]") {
  const ItemUseEffect* mastela = FindItemUseEffect(kFruitOfMastela);
  REQUIRE(mastela != nullptr);
  CHECK(mastela->effect_count == 0);
  CHECK(mastela->sound == "_heal_effect.wav");

  const ItemUseEffect* certificate = FindItemUseEffect(kInspectorCertificate);
  REQUIRE(certificate != nullptr);
  CHECK(certificate->sound == "effect\\mapae.wav");
  REQUIRE(certificate->effect_count == 1);
}

TEST_CASE("Usage d'objet : les plats de cuisine et le plus haut numéro", "[sim][item_use]") {
  const ItemUseEffect* dish = FindItemUseEffect(kFirstStrDish);
  REQUIRE(dish != nullptr);
  CHECK(dish->effect_ids[0] == 593);
  const ItemUseEffect* highest = FindItemUseEffect(kHighestItem);
  REQUIRE(highest != nullptr);
  CHECK(highest->effect_ids[0] == 2434);
}

TEST_CASE("Usage d'objet : la plupart des objets ne jouent rien", "[sim][item_use]") {
  CHECK(FindItemUseEffect(0) == nullptr);
  CHECK(FindItemUseEffect(kExpManual) == nullptr);
  CHECK(FindItemUseEffect(kHighestItem + 1) == nullptr);
}

TEST_CASE("Usage d'objet : la table est triée et sans doublon", "[sim][item_use]") {
  const auto table = ro::sim::ItemUseEffects();
  CHECK(table.size() == 275);
  CHECK(std::ranges::adjacent_find(table, [](const ItemUseEffect& a, const ItemUseEffect& b) {
          return a.item_id >= b.item_id;
        }) == table.end());
  for (const ItemUseEffect& use : table) {
    CHECK((use.effect_count > 0 || !use.sound.empty()));
  }
}

TEST_CASE("Usage d'objet : la ligne du manuel suit le métier montré", "[sim][item_use]") {
  CHECK(ItemUseLineFor(kExpManual, kRuneKnight - 1) == ItemUseLine::kExpBonus75);
  CHECK(ItemUseLineFor(kExpManual, kRuneKnight) == ItemUseLine::kExpBonus50);
  CHECK(ItemUseLineFor(kExpManual, kLastThirdJob) == ItemUseLine::kExpBonus50);
  CHECK(ItemUseLineFor(kExpManual, kLastThirdJob + 1) == ItemUseLine::kExpBonus75);
  CHECK(ItemUseLineFor(kExpManual, kNovice) == ItemUseLine::kExpBonus75);
  CHECK(ItemUseLineFor(kRedPotion, kRuneKnight) == ItemUseLine::kNone);
}
