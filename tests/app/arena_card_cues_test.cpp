// LA CARTE TOMBÉE DANS L'ARÈNE (Q6) : elle se dessine par le sprite RO d'une
// carte au sol, luit tant qu'elle reste, sonne à sa chute ; ramassée, elle
// brille sur le héros, sonne, et son illustration paraît trois secondes au HUD.

#include "survivors/arena_card_cues.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/item_drop.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors/arena_effect_plan.h"
#include "survivors/survivors_looks.h"

namespace {

using moonclient::CardVignettes;
using moonclient::CardVignetteShown;
using moonclient::kCardVignetteMs;
using ro::sim::PickupKind;
using ro::sim::SurvivorsCardNews;
using ro::sim::SurvivorsNews;
using ro::sim::SurvivorsPickup;

constexpr std::uint16_t kGolemCard = 4072;
constexpr std::uint16_t kPoringCard = 4001;
constexpr std::uint16_t kMeatItem = 517;
constexpr std::uint64_t kPickedAtMs = 10'000;
constexpr std::uint64_t kHalfVignetteMs = kCardVignetteMs / 2;
const ro::sim::FieldPoint kCardAt{1'200, 3'400};

[[nodiscard]] SurvivorsNews FoundNews(std::uint16_t card_item_id) {
  SurvivorsNews news;
  news.cards_found.push_back(SurvivorsCardNews{.card_item_id = card_item_id, .at = kCardAt});
  return news;
}

[[nodiscard]] SurvivorsPickup CardOnGround(std::uint32_t serial) {
  return SurvivorsPickup{
      .serial = serial, .kind = PickupKind::kCard, .item_id = kGolemCard, .position = kCardAt};
}

}  // namespace

TEST_CASE("Carte au sol : le sprite RO d'une carte, le même pour toutes", "[app][survivors]") {
  CHECK(moonclient::PickupLookOf(CardOnGround(ro::sim::kFirstCardSerial)) ==
        moonclient::kGroundCardItem);
  const SurvivorsPickup meat{.serial = 1, .kind = PickupKind::kMeat, .item_id = kMeatItem};
  CHECK(moonclient::PickupLookOf(meat) == kMeatItem);
}

TEST_CASE("Carte au sol : un ramassage donne une vignette et un son", "[app][survivors]") {
  const SurvivorsNews news = FoundNews(kGolemCard);
  const moonclient::ArenaCardSounds sounds = moonclient::CardSoundsOf(news);
  CHECK(sounds.pickup == moonclient::kCardPickupSound);
  CHECK(sounds.drop.empty());

  CardVignettes vignettes;
  CHECK_FALSE(vignettes.ShownAt(kPickedAtMs).has_value());
  vignettes.Notice(news.cards_found, kPickedAtMs);
  const std::optional<CardVignetteShown> shown = vignettes.ShownAt(kPickedAtMs + kHalfVignetteMs);
  REQUIRE(shown.has_value());
  CHECK(shown->card_item_id == kGolemCard);
  CHECK(shown->fraction == Catch::Approx(0.5F));
  // Trois secondes, puis plus rien.
  CHECK_FALSE(vignettes.ShownAt(kPickedAtMs + kCardVignetteMs).has_value());
  CHECK(vignettes.waiting() == 0U);
}

TEST_CASE("Carte au sol : une chute sonne au son du butin violet", "[app][survivors]") {
  SurvivorsNews news;
  news.cards_dropped.push_back(SurvivorsCardNews{.card_item_id = kGolemCard, .at = kCardAt});
  const moonclient::ArenaCardSounds sounds = moonclient::CardSoundsOf(news);
  CHECK(sounds.drop == ro::render::ItemDropPillarOf(moonclient::kRareGlowPillar).sound);
  CHECK_FALSE(sounds.drop.empty());
  CHECK(sounds.pickup.empty());
  CHECK(moonclient::CardSoundsOf(SurvivorsNews{}).drop.empty());
}

TEST_CASE("Carte au sol : deux cartes prises ont chacune leurs trois secondes",
          "[app][survivors]") {
  CardVignettes vignettes;
  SurvivorsNews news = FoundNews(kGolemCard);
  news.cards_found.push_back(SurvivorsCardNews{.card_item_id = kPoringCard, .at = kCardAt});
  vignettes.Notice(news.cards_found, kPickedAtMs);
  CHECK(vignettes.ShownAt(kPickedAtMs)->card_item_id == kGolemCard);
  const std::optional<CardVignetteShown> second =
      vignettes.ShownAt(kPickedAtMs + kCardVignetteMs + kHalfVignetteMs);
  REQUIRE(second.has_value());
  CHECK(second->card_item_id == kPoringCard);
  CHECK(second->fraction == Catch::Approx(0.5F));

  // Une file finie ne retarde pas la carte suivante.
  const std::uint64_t later_ms = kPickedAtMs + 10 * kCardVignetteMs;
  vignettes.Notice(FoundNews(kGolemCard).cards_found, later_ms);
  CHECK(vignettes.ShownAt(later_ms)->fraction == Catch::Approx(0.0F));

  // Une horloge qui recule est une partie neuve : la file se vide.
  CHECK_FALSE(vignettes.ShownAt(kPickedAtMs).has_value());
}

TEST_CASE("Carte au sol : la file des vignettes a son plafond", "[app][survivors]") {
  CardVignettes vignettes;
  SurvivorsNews news;
  for (std::size_t index = 0; index < moonclient::kMostCardVignettes + 2; ++index) {
    news.cards_found.push_back(SurvivorsCardNews{.card_item_id = kGolemCard, .at = kCardAt});
  }
  vignettes.Notice(news.cards_found, kPickedAtMs);
  CHECK(vignettes.waiting() == moonclient::kMostCardVignettes);
}

TEST_CASE("Carte au sol : une lueur par carte, gardée sous « Effets réduits »",
          "[app][survivors]") {
  std::vector<SurvivorsPickup> pickups;
  pickups.push_back(SurvivorsPickup{.serial = 3, .kind = PickupKind::kMeat, .item_id = kMeatItem});
  pickups.push_back(CardOnGround(ro::sim::kFirstCardSerial));
  std::vector<moonclient::ArenaRareGlow> glows;
  moonclient::PlanArenaRareGlows(pickups, {}, glows);
  REQUIRE(glows.size() == 1U);
  CHECK(glows[0].actor_id == ro::sim::kFirstCardSerial);
  CHECK(glows[0].at == kCardAt);

  moonclient::PlanArenaRareGlows(pickups, {.reduced = true}, glows);
  CHECK(glows.size() == 1U);
  moonclient::PlanArenaRareGlows(pickups, {.shown = false}, glows);
  CHECK(glows.empty());

  // Au plus `kMostArenaRareGlows`, les plus anciennes d'abord.
  pickups.clear();
  for (std::uint32_t index = 0; index < moonclient::kMostArenaRareGlows + 3; ++index) {
    pickups.push_back(CardOnGround(ro::sim::kFirstCardSerial + index));
  }
  moonclient::PlanArenaRareGlows(pickups, {}, glows);
  CHECK(glows.size() == moonclient::kMostArenaRareGlows);
  CHECK(glows.front().actor_id == ro::sim::kFirstCardSerial);
}

TEST_CASE("Coffre au sol : il luit de la colonne violette, par un acteur à lui",
          "[app][survivors]") {
  // Un coffre de numéro 1 ou 2 prendrait sans base l'acteur du héros ou de la
  // première orbe du monde privé.
  constexpr std::uint32_t kChestSerial = 2;
  constexpr std::uint16_t kOldBlueBox = 603;
  const ro::sim::FieldPoint chest_at{5'000, 6'000};
  std::vector<SurvivorsPickup> pickups;
  pickups.push_back(SurvivorsPickup{.serial = 1, .kind = PickupKind::kMeat, .item_id = kMeatItem});
  pickups.push_back(SurvivorsPickup{.serial = kChestSerial,
                                    .kind = PickupKind::kChest,
                                    .item_id = kOldBlueBox,
                                    .position = chest_at});
  pickups.push_back(CardOnGround(ro::sim::kFirstCardSerial));
  std::vector<moonclient::ArenaRareGlow> glows;
  moonclient::PlanArenaRareGlows(pickups, {}, glows);
  REQUIRE(glows.size() == 2U);
  CHECK(glows[0].actor_id == moonclient::kFirstChestGlowActorId + kChestSerial);
  CHECK(glows[0].at == chest_at);
  CHECK(glows[1].actor_id == ro::sim::kFirstCardSerial);

  moonclient::PlanArenaRareGlows(pickups, {.reduced = true}, glows);
  CHECK(glows.size() == 2U);
}

TEST_CASE("Carte au sol : la carte prise brille sur le héros, sauf effets réduits",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const ro::sim::SurvivorsField field;
  moonclient::ArenaEffectBudget budget;
  std::vector<moonclient::ArenaLaunch> plan;
  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(FoundNews(kGolemCard), field, looks, {}, {}, budget, plan);
  REQUIRE(plan.size() == 1U);
  CHECK(plan[0].effect_id == moonclient::kCardPickupEffect);
  CHECK(plan[0].kind == moonclient::ArenaLaunchKind::kOnHero);

  budget.BeginFrame(0);
  moonclient::PlanArenaLaunches(FoundNews(kGolemCard), field, looks, {.reduced = true}, {}, budget,
                                plan);
  CHECK(plan.empty());
}
