#include "ro/sim/survivors_meta.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "ro/core/i18n.h"

namespace ro::sim {
namespace {

constexpr std::uint64_t kMostZeny = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint64_t kPercent = 100;
// Chaque rang déjà béni, toutes lignes confondues, majore le prix suivant de
// ce pourcentage.
constexpr std::uint64_t kPricePctPerRankBought = 10;

// ── LES TOTAUX PAR RANG ──────────────────────────────────────────────────────
// Réglés pour qu'à départ maximal le robot gagne au plus 90 % de ses parties à
// Cursed Water 0, et au moins 30 % à Cursed Water 5 (ADR 0133, « Lot 6 : le
// prix et la puissance de la méta »). Aucun essai rapide ne rejoue ces
// 400 parties : `survivors_meta_test` épingle les totaux, et retoucher l'un
// d'eux demande de refaire la mesure par `roc-cli survivors simulate
// --depart maximal --victoires-entre`, qui échoue hors des bornes.
// Puissance +3 % par rang.
constexpr std::array<std::int32_t, 5> kBlessingTotals{3, 6, 9, 12, 15};
// PV +5 % de ceux de la classe par rang.
constexpr std::array<std::int32_t, 3> kKyrieTotals{5, 10, 15};
// Armure +1, un seul rang : chaque point retranche un PV à des contacts de
// 3 à 12.
constexpr std::array<std::int32_t, 1> kAngelusTotals{1};
// PV récupérés par minute, +3 par rang.
constexpr std::array<std::int32_t, 5> kMagnificatTotals{3, 6, 9, 12, 15};
// Marche +3 % par rang.
constexpr std::array<std::int32_t, 2> kIncreaseAgilityTotals{3, 6};
// Recharge −2,5 % par rang : la statistique est un pourcentage entier, donc
// les totaux s'écrivent arrondis (2, puis 5).
constexpr std::array<std::int32_t, 2> kSuffragiumTotals{2, 5};
// Zone +3 % par rang.
constexpr std::array<std::int32_t, 2> kImpositioTotals{3, 6};
// Durée +10 % par rang.
constexpr std::array<std::int32_t, 2> kAspersioTotals{10, 20};
// Chance +5 % par rang.
constexpr std::array<std::int32_t, 3> kGloriaTotals{5, 10, 15};
// XP +2 % par rang.
constexpr std::array<std::int32_t, 5> kBenedictioTotals{2, 4, 6, 8, 10};
// Une vie.
constexpr std::array<std::int32_t, 1> kResurrectionTotals{1};

// ── LES PRIX ─────────────────────────────────────────────────────────────────
// Tout acheter (`FullMetaCost`) coûte environ 40 parties médianes du robot,
// prises à mi-chemin entre le départ neutre et le départ maximal, et l'ordre
// d'achat le plus économe (chaque ligne d'un trait, la plus chère en moyenne
// d'abord) au moins 32 ; la première Bénédiction coûte moins d'une partie
// médiane (ADR 0133).
constexpr std::uint64_t kCheapBlessingPrice = 180;
constexpr std::uint64_t kMiddleBlessingPrice = 270;
constexpr std::uint64_t kDearBlessingPrice = 360;
constexpr std::uint64_t kResurrectionPrice = 1800;
// Trois rangs par service : un rang, une charge par partie.
constexpr std::size_t kKafraRanks = 3;
constexpr std::array<std::uint64_t, kKafraRanks> kKafraRankPrices{300, 600, 1200};

// Les identifiants d'objet RO des trois services.
constexpr std::uint16_t kMagnifierItemId = 611;
constexpr std::uint16_t kFlyWingItemId = 601;
constexpr std::uint16_t kButterflyWingItemId = 602;

constexpr std::array<BlessingRule, kBlessingCount> kBlessings{{
    {.blessing = Blessing::kBlessing,
     .key = "blessing",
     .skill_name = "AL_BLESSING",
     .description = core::TrText("Dégâts de tous les skills augmentés."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kDamagePct,
     .totals = kBlessingTotals,
     .base_price = kCheapBlessingPrice},
    {.blessing = Blessing::kKyrieEleison,
     .key = "kyrie_eleison",
     .skill_name = "PR_KYRIE",
     .description = core::TrText("PV maximum augmentés."),
     .effect = BlessingEffect::kMaxHpPct,
     .stat = SurvivorsStat::kMaxHp,
     .totals = kKyrieTotals,
     .base_price = kMiddleBlessingPrice},
    {.blessing = Blessing::kAngelus,
     .key = "angelus",
     .skill_name = "AL_ANGELUS",
     .description = core::TrText("Armure : chaque coup reçu est réduit."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kArmor,
     .totals = kAngelusTotals,
     .base_price = kMiddleBlessingPrice},
    {.blessing = Blessing::kMagnificat,
     .key = "magnificat",
     .skill_name = "PR_MAGNIFICAT",
     .description = core::TrText("Récupération de PV au fil du temps."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kRegenPerMinute,
     .totals = kMagnificatTotals,
     .base_price = kCheapBlessingPrice},
    {.blessing = Blessing::kIncreaseAgility,
     .key = "increase_agility",
     .skill_name = "AL_INCAGI",
     .description = core::TrText("Vitesse de marche augmentée."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kWalkPct,
     .totals = kIncreaseAgilityTotals,
     .base_price = kMiddleBlessingPrice},
    {.blessing = Blessing::kSuffragium,
     .key = "suffragium",
     .skill_name = "PR_SUFFRAGIUM",
     .description = core::TrText("Recharge de tous les skills raccourcie."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kCooldownPct,
     .totals = kSuffragiumTotals,
     .base_price = kDearBlessingPrice},
    {.blessing = Blessing::kImpositioManus,
     .key = "impositio_manus",
     .skill_name = "PR_IMPOSITIO",
     .description = core::TrText("Zone de tous les skills élargie."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kAreaPct,
     .totals = kImpositioTotals,
     .base_price = kMiddleBlessingPrice},
    {.blessing = Blessing::kAspersio,
     .key = "aspersio",
     .skill_name = "PR_ASPERSIO",
     .description = core::TrText("Durée de tous les skills allongée."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kDurationPct,
     .totals = kAspersioTotals,
     .base_price = kMiddleBlessingPrice},
    {.blessing = Blessing::kGloria,
     .key = "gloria",
     .skill_name = "PR_GLORIA",
     .description = core::TrText("Chance d'une quatrième offre à la montée de niveau."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kLuckPct,
     .totals = kGloriaTotals,
     .base_price = kCheapBlessingPrice},
    {.blessing = Blessing::kBenedictio,
     .key = "benedictio",
     .skill_name = "PR_BENEDICTIO",
     .description = core::TrText("Expérience gagnée augmentée."),
     .effect = BlessingEffect::kStat,
     .stat = SurvivorsStat::kExpPct,
     .totals = kBenedictioTotals,
     .base_price = kCheapBlessingPrice},
    {.blessing = Blessing::kResurrection,
     .key = "resurrection",
     .skill_name = "ALL_RESURRECTION",
     .description = core::TrText("Une vie de plus : le héros se relève une fois."),
     .effect = BlessingEffect::kLife,
     .stat = SurvivorsStat::kMaxHp,
     .totals = kResurrectionTotals,
     .base_price = kResurrectionPrice},
}};

constexpr std::array<KafraRule, kKafraServiceCount> kKafraServices{{
    {.service = KafraService::kMagnifier,
     .key = "magnifier",
     .item_id = kMagnifierItemId,
     .charge = KafraCharge::kReroll,
     .rank_prices = kKafraRankPrices},
    {.service = KafraService::kFlyWing,
     .key = "fly_wing",
     .item_id = kFlyWingItemId,
     .charge = KafraCharge::kSkip,
     .rank_prices = kKafraRankPrices},
    {.service = KafraService::kButterflyWing,
     .key = "butterfly_wing",
     .item_id = kButterflyWingItemId,
     .charge = KafraCharge::kBanish,
     .rank_prices = kKafraRankPrices},
}};

constexpr SurvivorsMeta kShippedMeta{.blessings = kBlessings, .kafra = kKafraServices};

[[nodiscard]] std::uint64_t SaturatedAdd(std::uint64_t total, std::uint64_t amount) noexcept {
  return amount > kMostZeny - total ? kMostZeny : total + amount;
}

[[nodiscard]] std::uint64_t SaturatedMultiply(std::uint64_t first, std::uint64_t second) noexcept {
  if (first != 0 && second > kMostZeny / first) return kMostZeny;
  return first * second;
}

// Un rang lu dans un carnet peut dépasser le catalogue : il compte pour le
// maximum.
[[nodiscard]] std::size_t HeldRank(std::uint8_t rank, std::size_t most) noexcept {
  return std::min<std::size_t>(rank, most);
}

// Un bonus ajouté à une statistique du loadout, gardé dans ±kMostStatBonus.
void AddBonus(SurvivorsStats& stats, SurvivorsStat stat, std::int64_t amount) noexcept {
  std::int32_t& value = stats.values[static_cast<std::size_t>(stat)];
  value = static_cast<std::int32_t>(
      std::clamp<std::int64_t>(std::int64_t{value} + amount, -kMostStatBonus, kMostStatBonus));
}

[[nodiscard]] std::uint8_t SaturatedCharge(std::uint64_t total) noexcept {
  return static_cast<std::uint8_t>(
      std::min<std::uint64_t>(total, std::numeric_limits<std::uint8_t>::max()));
}

}  // namespace

const SurvivorsMeta& ShippedSurvivorsMeta() { return kShippedMeta; }

const BlessingRule* FindBlessing(const SurvivorsMeta& meta, Blessing blessing) noexcept {
  for (const BlessingRule& rule : meta.blessings) {
    if (rule.blessing == blessing) return &rule;
  }
  return nullptr;
}

const KafraRule* FindKafra(const SurvivorsMeta& meta, KafraService service) noexcept {
  for (const KafraRule& rule : meta.kafra) {
    if (rule.service == service) return &rule;
  }
  return nullptr;
}

std::uint8_t MostRank(const SurvivorsMeta& meta, Blessing blessing) noexcept {
  const BlessingRule* rule = FindBlessing(meta, blessing);
  return rule == nullptr ? std::uint8_t{0} : SaturatedCharge(rule->totals.size());
}

std::uint8_t MostRank(const SurvivorsMeta& meta, KafraService service) noexcept {
  const KafraRule* rule = FindKafra(meta, service);
  return rule == nullptr ? std::uint8_t{0} : SaturatedCharge(rule->rank_prices.size());
}

SurvivorsMetaRanks MostMetaRanks(const SurvivorsMeta& meta) noexcept {
  SurvivorsMetaRanks ranks;
  for (std::size_t index = 0; index < kBlessingCount; ++index) {
    ranks.church[index] = MostRank(meta, static_cast<Blessing>(index));
  }
  for (std::size_t index = 0; index < kKafraServiceCount; ++index) {
    ranks.kafra[index] = MostRank(meta, static_cast<KafraService>(index));
  }
  return ranks;
}

std::uint64_t BlessingCost(const SurvivorsMeta& meta, const SurvivorsMetaRanks& ranks,
                           Blessing blessing) noexcept {
  const BlessingRule* rule = FindBlessing(meta, blessing);
  if (rule == nullptr || ranks.Of(blessing) >= MostRank(meta, blessing)) return 0;
  std::uint64_t bought = 0;
  for (const std::uint8_t rank : ranks.church) bought += rank;
  const std::uint64_t next_rank = std::uint64_t{ranks.Of(blessing)} + 1;
  const std::uint64_t raised_pct = SaturatedAdd(kPercent, bought * kPricePctPerRankBought);
  const std::uint64_t product =
      SaturatedMultiply(SaturatedMultiply(rule->base_price, next_rank), raised_pct);
  return product == kMostZeny ? kMostZeny : product / kPercent;
}

std::uint64_t KafraCost(const SurvivorsMeta& meta, const SurvivorsMetaRanks& ranks,
                        KafraService service) noexcept {
  const KafraRule* rule = FindKafra(meta, service);
  if (rule == nullptr) return 0;
  const std::size_t rank = ranks.Of(service);
  return rank < rule->rank_prices.size() ? rule->rank_prices[rank] : 0;
}

std::uint64_t FullMetaCost(const SurvivorsMeta& meta) noexcept {
  SurvivorsMetaRanks ranks;
  std::uint64_t total = 0;
  for (;;) {
    std::optional<Blessing> cheapest_blessing;
    std::optional<KafraService> cheapest_service;
    std::uint64_t cheapest = kMostZeny;
    for (std::size_t index = 0; index < kBlessingCount; ++index) {
      const auto blessing = static_cast<Blessing>(index);
      if (ranks.Of(blessing) >= MostRank(meta, blessing)) continue;
      const std::uint64_t price = BlessingCost(meta, ranks, blessing);
      if (!cheapest_blessing || price < cheapest) {
        cheapest_blessing = blessing;
        cheapest = price;
      }
    }
    for (std::size_t index = 0; index < kKafraServiceCount; ++index) {
      const auto service = static_cast<KafraService>(index);
      if (ranks.Of(service) >= MostRank(meta, service)) continue;
      const std::uint64_t price = KafraCost(meta, ranks, service);
      if ((!cheapest_blessing && !cheapest_service) || price < cheapest) {
        cheapest_blessing.reset();
        cheapest_service = service;
        cheapest = price;
      }
    }
    if (cheapest_blessing) {
      ++ranks.church[static_cast<std::size_t>(*cheapest_blessing)];
    } else if (cheapest_service) {
      ++ranks.kafra[static_cast<std::size_t>(*cheapest_service)];
    } else {
      return total;
    }
    total = SaturatedAdd(total, cheapest);
  }
}

MetaPurchase BuyBlessing(const SurvivorsMeta& meta, SurvivorsMetaRanks& ranks, std::uint64_t& zeny,
                         Blessing blessing) noexcept {
  if (ranks.Of(blessing) >= MostRank(meta, blessing)) {
    return std::unexpected(MetaRefusal::kMostRank);
  }
  const std::uint64_t price = BlessingCost(meta, ranks, blessing);
  if (price > zeny) return std::unexpected(MetaRefusal::kNotEnoughZeny);
  zeny -= price;
  ranks.church_spent = SaturatedAdd(ranks.church_spent, price);
  ++ranks.church[static_cast<std::size_t>(blessing)];
  return price;
}

MetaPurchase BuyKafra(const SurvivorsMeta& meta, SurvivorsMetaRanks& ranks, std::uint64_t& zeny,
                      KafraService service) noexcept {
  if (ranks.Of(service) >= MostRank(meta, service)) {
    return std::unexpected(MetaRefusal::kMostRank);
  }
  const std::uint64_t price = KafraCost(meta, ranks, service);
  if (price > zeny) return std::unexpected(MetaRefusal::kNotEnoughZeny);
  zeny -= price;
  ranks.kafra_spent = SaturatedAdd(ranks.kafra_spent, price);
  ++ranks.kafra[static_cast<std::size_t>(service)];
  return price;
}

std::uint64_t RefundChurch(SurvivorsMetaRanks& ranks, std::uint64_t& zeny) noexcept {
  const std::uint64_t refunded = ranks.church_spent;
  zeny = SaturatedAdd(zeny, refunded);
  ranks.church = {};
  ranks.church_spent = 0;
  return refunded;
}

std::uint64_t RefundKafra(SurvivorsMetaRanks& ranks, std::uint64_t& zeny) noexcept {
  const std::uint64_t refunded = ranks.kafra_spent;
  zeny = SaturatedAdd(zeny, refunded);
  ranks.kafra = {};
  ranks.kafra_spent = 0;
  return refunded;
}

SurvivorsMetaStart LoadoutOf(const SurvivorsMeta& meta, const SurvivorsMetaRanks& ranks,
                             const ClassRule& class_rule) noexcept {
  SurvivorsMetaStart start;
  std::uint64_t lives = 0;
  for (const BlessingRule& rule : meta.blessings) {
    const std::size_t rank = HeldRank(ranks.Of(rule.blessing), rule.totals.size());
    if (rank == 0) continue;
    const std::int32_t total = rule.totals[rank - 1];
    switch (rule.effect) {
      case BlessingEffect::kStat:
        AddBonus(start.loadout.bonus, rule.stat, total);
        break;
      case BlessingEffect::kMaxHpPct:
        AddBonus(start.loadout.bonus, SurvivorsStat::kMaxHp,
                 std::int64_t{class_rule.max_hp} * total / static_cast<std::int64_t>(kPercent));
        break;
      case BlessingEffect::kLife:
        if (total > 0) lives += static_cast<std::uint64_t>(total);
        break;
    }
  }
  start.lives = SaturatedCharge(lives);
  for (const KafraRule& rule : meta.kafra) {
    const std::uint8_t charges =
        SaturatedCharge(HeldRank(ranks.Of(rule.service), rule.rank_prices.size()));
    switch (rule.charge) {
      case KafraCharge::kReroll:
        start.loadout.rerolls = charges;
        break;
      case KafraCharge::kSkip:
        start.loadout.skips = charges;
        break;
      case KafraCharge::kBanish:
        start.loadout.banishes = charges;
        break;
    }
  }
  return start;
}

std::uint64_t RunZeny(const SurvivorsTally& tally, std::uint64_t hero_zeny,
                      SurvivorsOutcome outcome) noexcept {
  std::uint64_t zeny = SaturatedAdd(hero_zeny, tally.kills / kKillsPerZeny);
  if (outcome == SurvivorsOutcome::kVictory) zeny = SaturatedAdd(zeny, kVictoryZeny);
  return zeny;
}

std::uint64_t CursedWaterZeny(std::uint64_t run_zeny, std::uint8_t cursed_water) noexcept {
  const std::uint64_t raised_pct = kPercent + kCursedWaterZenyPctPerRank * cursed_water;
  const std::uint64_t product = SaturatedMultiply(run_zeny, raised_pct);
  return product == kMostZeny ? kMostZeny : product / kPercent;
}

}  // namespace ro::sim
