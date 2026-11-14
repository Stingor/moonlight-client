// LA MÉTA-PROGRESSION : le prix d'une Bénédiction, les refus d'achat, le
// remboursement intégral, le départ que donnent les rangs et les Zeny d'une
// partie. Les règles de prix relisent le catalogue ; un seul essai épingle
// les prix livrés, mesurés au robot (ADR 0133).

#include "ro/sim/survivors_meta.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

using ro::sim::Blessing;
using ro::sim::BlessingCost;
using ro::sim::BuyBlessing;
using ro::sim::BuyKafra;
using ro::sim::ClassRule;
using ro::sim::FindBlessing;
using ro::sim::FullMetaCost;
using ro::sim::KafraCost;
using ro::sim::KafraService;
using ro::sim::LoadoutOf;
using ro::sim::LoadoutWithinBounds;
using ro::sim::MetaPurchase;
using ro::sim::MetaRefusal;
using ro::sim::MostRank;
using ro::sim::RefundChurch;
using ro::sim::RefundKafra;
using ro::sim::RunZeny;
using ro::sim::ShippedSurvivorsMeta;
using ro::sim::SurvivorsMeta;
using ro::sim::SurvivorsMetaRanks;
using ro::sim::SurvivorsMetaStart;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsStat;
using ro::sim::SurvivorsTally;

namespace {

constexpr std::uint64_t kRich = 1'000'000'000;

[[nodiscard]] const SurvivorsMeta& Meta() { return ShippedSurvivorsMeta(); }

[[nodiscard]] const ClassRule& Mage() {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  REQUIRE_FALSE(rules.classes.empty());
  return rules.classes.front();
}

[[nodiscard]] std::uint64_t BasePrice(Blessing blessing) {
  const ro::sim::BlessingRule* rule = FindBlessing(Meta(), blessing);
  REQUIRE(rule != nullptr);
  return rule->base_price;
}

void Buy(SurvivorsMetaRanks& ranks, std::uint64_t& zeny, Blessing blessing) {
  REQUIRE(BuyBlessing(Meta(), ranks, zeny, blessing).has_value());
}

}  // namespace

TEST_CASE("Méta survivors : le catalogue suit l'ordre de ses énumérations", "[sim][survivors]") {
  REQUIRE(Meta().blessings.size() == ro::sim::kBlessingCount);
  REQUIRE(Meta().kafra.size() == ro::sim::kKafraServiceCount);
  for (std::size_t index = 0; index < Meta().blessings.size(); ++index) {
    CHECK(static_cast<std::size_t>(Meta().blessings[index].blessing) == index);
    CHECK_FALSE(Meta().blessings[index].totals.empty());
    CHECK(Meta().blessings[index].base_price > 0);
  }
  for (std::size_t index = 0; index < Meta().kafra.size(); ++index) {
    CHECK(static_cast<std::size_t>(Meta().kafra[index].service) == index);
    CHECK(Meta().kafra[index].rank_prices.size() == 3);
  }
}

TEST_CASE("Méta survivors : le prix au premier rang, puis au rang 3 après quatre achats",
          "[sim][survivors]") {
  SurvivorsMetaRanks ranks;
  CHECK(BlessingCost(Meta(), ranks, Blessing::kBlessing) == BasePrice(Blessing::kBlessing));

  std::uint64_t zeny = kRich;
  Buy(ranks, zeny, Blessing::kBlessing);
  Buy(ranks, zeny, Blessing::kBlessing);
  Buy(ranks, zeny, Blessing::kAngelus);
  Buy(ranks, zeny, Blessing::kGloria);
  // Rang 3, quatre rangs déjà bénis : base × 3 × 140 / 100.
  constexpr std::uint64_t kThirdRank = 3;
  constexpr std::uint64_t kFourBoughtPct = 140;
  constexpr std::uint64_t kPercent = 100;
  CHECK(BlessingCost(Meta(), ranks, Blessing::kBlessing) ==
        BasePrice(Blessing::kBlessing) * kThirdRank * kFourBoughtPct / kPercent);
}

TEST_CASE("Méta survivors : refus au rang maximal et sans Zeny, sans rien changer",
          "[sim][survivors]") {
  SurvivorsMetaRanks ranks;
  std::uint64_t zeny = 0;
  const MetaPurchase poor = BuyBlessing(Meta(), ranks, zeny, Blessing::kBlessing);
  REQUIRE_FALSE(poor.has_value());
  CHECK(poor.error() == MetaRefusal::kNotEnoughZeny);
  CHECK(ranks == SurvivorsMetaRanks{});

  zeny = kRich;
  for (std::uint8_t rank = 0; rank < MostRank(Meta(), Blessing::kResurrection); ++rank) {
    Buy(ranks, zeny, Blessing::kResurrection);
  }
  const SurvivorsMetaRanks full = ranks;
  const std::uint64_t left = zeny;
  const MetaPurchase capped = BuyBlessing(Meta(), ranks, zeny, Blessing::kResurrection);
  REQUIRE_FALSE(capped.has_value());
  CHECK(capped.error() == MetaRefusal::kMostRank);
  CHECK(ranks == full);
  CHECK(zeny == left);

  for (std::uint8_t rank = 0; rank < MostRank(Meta(), KafraService::kFlyWing); ++rank) {
    REQUIRE(BuyKafra(Meta(), ranks, zeny, KafraService::kFlyWing).has_value());
  }
  CHECK(KafraCost(Meta(), ranks, KafraService::kFlyWing) == 0);
  // Une ligne pleine n'a plus de prix, à l'Église comme à la Kafra.
  CHECK(BlessingCost(Meta(), ranks, Blessing::kResurrection) == 0);
  const MetaPurchase wings = BuyKafra(Meta(), ranks, zeny, KafraService::kFlyWing);
  REQUIRE_FALSE(wings.has_value());
  CHECK(wings.error() == MetaRefusal::kMostRank);
}

TEST_CASE("Méta survivors : le remboursement rend tout, quel que soit l'ordre des achats",
          "[sim][survivors]") {
  SurvivorsMetaRanks first_order;
  std::uint64_t first_zeny = kRich;
  Buy(first_order, first_zeny, Blessing::kBlessing);
  Buy(first_order, first_zeny, Blessing::kSuffragium);
  Buy(first_order, first_zeny, Blessing::kBlessing);

  SurvivorsMetaRanks second_order;
  std::uint64_t second_zeny = kRich;
  Buy(second_order, second_zeny, Blessing::kSuffragium);
  Buy(second_order, second_zeny, Blessing::kBlessing);
  Buy(second_order, second_zeny, Blessing::kBlessing);

  // Les mêmes rangs, payés autrement.
  CHECK(first_order.church == second_order.church);
  CHECK(first_order.church_spent != second_order.church_spent);
  const std::uint64_t first_spent = first_order.church_spent;
  const std::uint64_t second_spent = second_order.church_spent;
  CHECK(RefundChurch(first_order, first_zeny) == first_spent);
  CHECK(RefundChurch(second_order, second_zeny) == second_spent);
  CHECK(first_zeny == kRich);
  CHECK(second_zeny == kRich);
  CHECK(first_order == SurvivorsMetaRanks{});

  SurvivorsMetaRanks kafra;
  std::uint64_t kafra_zeny = kRich;
  REQUIRE(BuyKafra(Meta(), kafra, kafra_zeny, KafraService::kMagnifier).has_value());
  REQUIRE(BuyKafra(Meta(), kafra, kafra_zeny, KafraService::kButterflyWing).has_value());
  Buy(kafra, kafra_zeny, Blessing::kAngelus);
  CHECK(RefundKafra(kafra, kafra_zeny) > 0);
  // L'Église garde ses rangs quand la Kafra rembourse.
  CHECK(kafra.Of(Blessing::kAngelus) == 1);
  CHECK(kafra.kafra_spent == 0);
  CHECK(kafra_zeny == kRich - kafra.church_spent);
}

TEST_CASE("Méta survivors : un prix et un remboursement saturent au lieu de déborder",
          "[sim][survivors]") {
  constexpr std::uint64_t kMostZeny = std::numeric_limits<std::uint64_t>::max();
  constexpr std::array<std::int32_t, 2> kTwoRanks{1, 2};
  const std::array<ro::sim::BlessingRule, 1> blessings{{{.blessing = Blessing::kBlessing,
                                                         .key = "blessing",
                                                         .totals = kTwoRanks,
                                                         .base_price = kMostZeny / 2}}};
  const SurvivorsMeta huge{.blessings = blessings};
  SurvivorsMetaRanks ranks;
  ranks.church[static_cast<std::size_t>(Blessing::kBlessing)] = 1;
  // Rang 2, un rang déjà béni : base × 2 × 110 dépasse 64 bits.
  CHECK(BlessingCost(huge, ranks, Blessing::kBlessing) == kMostZeny);
  std::uint64_t zeny = kRich;
  const MetaPurchase refused = BuyBlessing(huge, ranks, zeny, Blessing::kBlessing);
  REQUIRE_FALSE(refused.has_value());
  CHECK(refused.error() == MetaRefusal::kNotEnoughZeny);
  CHECK(zeny == kRich);

  constexpr std::uint64_t kNearlyAll = kMostZeny - 10;
  ranks.church_spent = kNearlyAll;
  CHECK(RefundChurch(ranks, zeny) == kNearlyAll);
  CHECK(zeny == kMostZeny);
  CHECK(ranks == SurvivorsMetaRanks{});
}

TEST_CASE("Méta survivors : un carnet vierge donne le départ neutre", "[sim][survivors]") {
  const SurvivorsMetaStart start = LoadoutOf(Meta(), SurvivorsMetaRanks{}, Mage());
  CHECK(start == SurvivorsMetaStart{});
  ro::sim::SurvivorsStart run_start;
  run_start.loadout = start.loadout;
  CHECK(ro::sim::IsNeutralStart(run_start));
}

TEST_CASE("Méta survivors : Kyrie en PV, Suffragium en totaux, charges et vies",
          "[sim][survivors]") {
  constexpr std::int32_t kPercent = 100;
  constexpr std::int32_t kKyrieSecondRankPct = 10;
  SurvivorsMetaRanks ranks;
  std::uint64_t zeny = kRich;
  Buy(ranks, zeny, Blessing::kKyrieEleison);
  Buy(ranks, zeny, Blessing::kKyrieEleison);
  Buy(ranks, zeny, Blessing::kSuffragium);
  SurvivorsMetaStart start = LoadoutOf(Meta(), ranks, Mage());
  CHECK(start.loadout.bonus.Of(SurvivorsStat::kMaxHp) ==
        Mage().max_hp * kKyrieSecondRankPct / kPercent);
  CHECK(start.loadout.bonus.Of(SurvivorsStat::kCooldownPct) == 2);
  Buy(ranks, zeny, Blessing::kSuffragium);
  start = LoadoutOf(Meta(), ranks, Mage());
  CHECK(start.loadout.bonus.Of(SurvivorsStat::kCooldownPct) == 5);
  CHECK(start.lives == 0);

  REQUIRE(BuyKafra(Meta(), ranks, zeny, KafraService::kMagnifier).has_value());
  REQUIRE(BuyKafra(Meta(), ranks, zeny, KafraService::kMagnifier).has_value());
  REQUIRE(BuyKafra(Meta(), ranks, zeny, KafraService::kButterflyWing).has_value());
  Buy(ranks, zeny, Blessing::kResurrection);
  start = LoadoutOf(Meta(), ranks, Mage());
  CHECK(start.loadout.rerolls == 2);
  CHECK(start.loadout.skips == 0);
  CHECK(start.loadout.banishes == 1);
  CHECK(start.lives == 1);
  CHECK(LoadoutWithinBounds(start.loadout));

  // Un rang lu au-delà du catalogue compte pour le maximum.
  SurvivorsMetaRanks edited;
  constexpr std::uint8_t kEditedRank = 200;
  edited.church.fill(kEditedRank);
  edited.kafra.fill(kEditedRank);
  const SurvivorsMetaStart most = LoadoutOf(Meta(), edited, Mage());
  CHECK(most.lives == 1);
  CHECK(most.loadout.rerolls == 3);
  CHECK(most.loadout.bonus.Of(SurvivorsStat::kCooldownPct) == 5);
  CHECK(LoadoutWithinBounds(most.loadout));
}

TEST_CASE("Méta survivors : les Zeny d'une victoire et d'une mort", "[sim][survivors]") {
  constexpr std::uint64_t kGold = 40;
  SurvivorsTally tally;
  tally.kills = 123;
  CHECK(RunZeny(tally, kGold, SurvivorsOutcome::kVictory) == kGold + 12 + ro::sim::kVictoryZeny);
  CHECK(RunZeny(tally, kGold, SurvivorsOutcome::kFell) == kGold + 12);
  CHECK(RunZeny(SurvivorsTally{}, 0, SurvivorsOutcome::kAbandoned) == 0);
}

TEST_CASE("Méta survivors : la Cursed Water majore les Zeny versés", "[sim][survivors]") {
  using ro::sim::CursedWaterZeny;
  CHECK(CursedWaterZeny(1234, 0) == 1234);
  // 1 234 × 130 / 100 = 1 604,2 : la division entière garde 1 604.
  CHECK(CursedWaterZeny(1234, 3) == 1604);
  CHECK(CursedWaterZeny(1000, ro::sim::kMostCursedWater) == 1500);
  CHECK(CursedWaterZeny(std::numeric_limits<std::uint64_t>::max(), 1) ==
        std::numeric_limits<std::uint64_t>::max());
}

TEST_CASE("Méta survivors : les prix livrés, mesurés au robot", "[sim][survivors]") {
  // Les relevés de l'ADR 0133 : 1 037 Zeny médians au départ neutre, 1 699 au
  // départ maximal. Tout acheter vaut environ 40 parties prises à mi-chemin,
  // la première Bénédiction moins d'une partie neutre.
  constexpr std::uint64_t kNeutralMedianZeny = 1037;
  constexpr std::uint64_t kMostMedianZeny = 1699;
  constexpr std::uint64_t kTargetRuns = 40;
  constexpr std::uint64_t kShippedFullCost = 57072;
  const std::uint64_t full_cost = FullMetaCost(Meta());
  CHECK(full_cost == kShippedFullCost);
  const std::uint64_t midway_median = (kNeutralMedianZeny + kMostMedianZeny) / 2;
  // À une partie près.
  // La feuille d'Yggdrasil ne sert qu'en coop : le rythme du solo se juge sans elle.
  constexpr std::uint64_t kLeafCost = 300 + 600 + 1200;
  CHECK(full_cost - kLeafCost >= (kTargetRuns - 1) * midway_median);
  CHECK(full_cost - kLeafCost <= (kTargetRuns + 1) * midway_median);

  std::uint64_t cheapest_first = std::numeric_limits<std::uint64_t>::max();
  for (const ro::sim::BlessingRule& rule : Meta().blessings) {
    cheapest_first =
        std::min(cheapest_first, BlessingCost(Meta(), SurvivorsMetaRanks{}, rule.blessing));
  }
  CHECK(cheapest_first <= kNeutralMedianZeny);

  // L'ORDRE LE PLUS ÉCONOME. Chaque rang béni majore les suivants : acheter
  // d'abord ce qui est cher économise, et le remboursement de l'Église permet
  // de tout racheter ainsi. Les rangs d'une ligne montant de prix, la ligne
  // s'achète d'un trait, et deux lignes voisines s'échangent au profit de la
  // plus chère en moyenne : c'est le minimum sur tous les ordres, que retrouve
  // un parcours exhaustif des 1,1 million d'états. Il descend à 32 parties.
  constexpr std::uint64_t kThriftiestRuns = 32;
  constexpr std::uint64_t kShippedThriftiestCost = 46353;
  std::vector<const ro::sim::BlessingRule*> lines;
  for (const ro::sim::BlessingRule& rule : Meta().blessings) lines.push_back(&rule);
  // Moyenne d'une ligne de n rangs : base × (n + 1) / 2, comparée sans le
  // facteur commun 1/2.
  std::ranges::stable_sort(lines, std::ranges::greater{}, [](const ro::sim::BlessingRule* rule) {
    return rule->base_price * (rule->totals.size() + 1);
  });
  SurvivorsMetaRanks ranks;
  std::uint64_t zeny = kRich;
  for (const ro::sim::BlessingRule* rule : lines) {
    while (BuyBlessing(Meta(), ranks, zeny, rule->blessing).has_value()) {
    }
  }
  for (const ro::sim::KafraRule& rule : Meta().kafra) {
    while (BuyKafra(Meta(), ranks, zeny, rule.service).has_value()) {
    }
  }
  const std::uint64_t thriftiest_cost = kRich - zeny;
  CHECK(ranks.church == ro::sim::MostMetaRanks(Meta()).church);
  CHECK(ranks.kafra == ro::sim::MostMetaRanks(Meta()).kafra);
  CHECK(thriftiest_cost == kShippedThriftiestCost);
  CHECK(thriftiest_cost >= kThriftiestRuns * midway_median);
}

TEST_CASE("Méta survivors : les totaux par rang, mesurés au robot", "[sim][survivors]") {
  // Les garde-fous de l'ADR 0133 : au départ maximal, le robot gagne 86 % de
  // ses parties à Cursed Water 0 (au plus 90) et 32 % à Cursed Water 5 (au
  // moins 30), charges Kafra non comprises puisqu'il ne s'en sert pas. Ces
  // 400 parties sont trop longues pour un essai : retoucher un total fait
  // échouer celui-ci, et l'épingler de nouveau demande d'abord de rejouer
  //   roc-cli survivors simulate --graines 1..200 --premier tous
  //       --depart maximal --eau-maudite 0 --victoires-entre 0..90
  // puis la même à --eau-maudite 5 --victoires-entre 30..100.
  struct Totals {
    Blessing blessing;
    std::vector<std::int32_t> totals;
  };
  const std::array<Totals, ro::sim::kBlessingCount> kMeasured{{
      {Blessing::kBlessing, {3, 6, 9, 12, 15}},
      {Blessing::kKyrieEleison, {5, 10, 15}},
      {Blessing::kAngelus, {1}},
      {Blessing::kMagnificat, {3, 6, 9, 12, 15}},
      {Blessing::kIncreaseAgility, {3, 6}},
      {Blessing::kSuffragium, {2, 5}},
      {Blessing::kImpositioManus, {3, 6}},
      {Blessing::kAspersio, {10, 20}},
      {Blessing::kGloria, {5, 10, 15}},
      {Blessing::kBenedictio, {2, 4, 6, 8, 10}},
      {Blessing::kResurrection, {1}},
  }};
  for (const Totals& measured : kMeasured) {
    const ro::sim::BlessingRule* rule = FindBlessing(Meta(), measured.blessing);
    REQUIRE(rule != nullptr);
    CHECK(std::vector<std::int32_t>(rule->totals.begin(), rule->totals.end()) == measured.totals);
  }
}

TEST_CASE("Méta survivors : tout acheter prend toujours le rang le moins cher",
          "[sim][survivors]") {
  // Deux Bénédictions et un service : 100 puis 200 pour l'une, 150 pour
  // l'autre majorée de 10 % par rang béni, 120 pour le service.
  constexpr std::array<std::int32_t, 2> kTwoRanks{1, 2};
  constexpr std::array<std::int32_t, 1> kOneRank{1};
  constexpr std::array<std::uint64_t, 1> kOneKafraRank{120};
  const std::array<ro::sim::BlessingRule, 2> blessings{{
      {.blessing = Blessing::kBlessing, .totals = kTwoRanks, .base_price = 100},
      {.blessing = Blessing::kKyrieEleison, .totals = kOneRank, .base_price = 150},
  }};
  const std::array<ro::sim::KafraRule, 1> kafra{{
      {.service = KafraService::kMagnifier, .rank_prices = kOneKafraRank},
  }};
  const SurvivorsMeta small{.blessings = blessings, .kafra = kafra};
  // Blessing 1 (100), Magnifier (120), Kyrie (150 × 110 / 100 = 165), puis
  // Blessing 2 (100 × 2 × 120 / 100 = 240).
  constexpr std::uint64_t kExpected = 100 + 120 + 165 + 240;
  CHECK(FullMetaCost(small) == kExpected);
  CHECK(FullMetaCost(SurvivorsMeta{}) == 0);
}
