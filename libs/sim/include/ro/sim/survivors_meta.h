#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

namespace ro::sim {

// ── LA MÉTA-PROGRESSION LOCALE ───────────────────────────────────────────────
//
// Des Zeny qui restent d'une partie à l'autre, dépensés à l'Église de Prontera
// (les Bénédictions, des bonus permanents) et à la Kafra (des charges par
// partie). Tout vit dans le DÉPART : une Bénédiction devient un
// `SurvivorsLoadout`, que la partie sait lire, et le catalogue n'entre pas
// dans `SurvivorsRules`. Changer un prix ne monte donc pas la version des
// règles, et un journal se rejoue sans ce catalogue.
//
// Les rangs, les soldes et les prix sont des entiers non signés ; tout calcul
// sature au lieu de déborder.

// Un ajout se fait en FIN d'énumération : le carnet range les rangs par CLÉ,
// mais le catalogue suit cet ordre.
enum class Blessing : std::uint8_t {
  kBlessing,
  kKyrieEleison,
  kAngelus,
  kMagnificat,
  kIncreaseAgility,
  kSuffragium,
  kImpositioManus,
  kAspersio,
  kGloria,
  kBenedictio,
  kResurrection,
};
inline constexpr std::size_t kBlessingCount = 11;
static_assert(static_cast<std::size_t>(Blessing::kResurrection) + 1 == kBlessingCount);

enum class KafraService : std::uint8_t { kMagnifier, kFlyWing, kButterflyWing };
inline constexpr std::size_t kKafraServiceCount = 3;
static_assert(static_cast<std::size_t>(KafraService::kButterflyWing) + 1 == kKafraServiceCount);

// Ce qu'une Bénédiction change au départ.
enum class BlessingEffect : std::uint8_t {
  kStat,      // `totals` s'ajoute à la statistique `stat`
  kMaxHpPct,  // `totals` en pour cent des PV de la classe, versés en PV
  kLife,      // `totals` vies
};
// La charge qu'un service Kafra donne à chaque partie : un rang, une charge.
enum class KafraCharge : std::uint8_t { kReroll, kSkip, kBanish };

struct BlessingRule {
  Blessing blessing = Blessing::kBlessing;
  const char* key = "";          // clé stable du carnet
  const char* skill_name = "";   // nom RO du skill (`AL_BLESSING`…) : l'icône, côté app
  const char* description = "";  // TrText
  BlessingEffect effect = BlessingEffect::kStat;
  SurvivorsStat stat = SurvivorsStat::kDamagePct;  // lu pour kStat seulement
  // Le TOTAL au rang r est `totals[r − 1]`, jamais un incrément à cumuler ; la
  // taille est le rang maximal.
  std::span<const std::int32_t> totals;
  std::uint64_t base_price = 0;  // Zeny
};

struct KafraRule {
  KafraService service = KafraService::kMagnifier;
  const char* key = "";
  std::uint16_t item_id = 0;  // identifiant d'objet RO : l'icône, côté app
  KafraCharge charge = KafraCharge::kReroll;
  // Le prix du rang r est `rank_prices[r − 1]` ; la taille est le rang
  // maximal.
  std::span<const std::uint64_t> rank_prices;
};

struct SurvivorsMeta {
  std::span<const BlessingRule> blessings;  // dans l'ordre de `Blessing`
  std::span<const KafraRule> kafra;         // dans l'ordre de `KafraService`
};

// Le catalogue livré : une table constante.
[[nodiscard]] const SurvivorsMeta& ShippedSurvivorsMeta();
[[nodiscard]] const BlessingRule* FindBlessing(const SurvivorsMeta& meta,
                                               Blessing blessing) noexcept;
[[nodiscard]] const KafraRule* FindKafra(const SurvivorsMeta& meta, KafraService service) noexcept;

// CE QUI A ÉTÉ ACHETÉ : un rang par Bénédiction et par service, et les Zeny
// payés dans chaque lieu. Le remboursement rend ce qui a été payé, quel que
// soit l'ordre des achats.
struct SurvivorsMetaRanks {
  std::array<std::uint8_t, kBlessingCount> church = {};
  std::array<std::uint8_t, kKafraServiceCount> kafra = {};
  std::uint64_t church_spent = 0;
  std::uint64_t kafra_spent = 0;
  [[nodiscard]] std::uint8_t Of(Blessing blessing) const noexcept {
    return church[static_cast<std::size_t>(blessing)];
  }
  [[nodiscard]] std::uint8_t Of(KafraService service) const noexcept {
    return kafra[static_cast<std::size_t>(service)];
  }
  [[nodiscard]] bool operator==(const SurvivorsMetaRanks&) const = default;
};

// Le rang maximal d'une Bénédiction ou d'un service ; 0 s'il manque au
// catalogue.
[[nodiscard]] std::uint8_t MostRank(const SurvivorsMeta& meta, Blessing blessing) noexcept;
[[nodiscard]] std::uint8_t MostRank(const SurvivorsMeta& meta, KafraService service) noexcept;
// Chaque Bénédiction et chaque service à son rang maximal, sans Zeny payé :
// le départ chargé de la partie en or et des mesures du robot.
[[nodiscard]] SurvivorsMetaRanks MostMetaRanks(const SurvivorsMeta& meta) noexcept;

// LE PRIX DU RANG SUIVANT : `base × r × (100 + 10 × rangs déjà achetés à
// l'Église) / 100`, division entière. Le prix croît donc avec tout ce qui a
// été béni, pas seulement avec la ligne. Saturé à la plus grande valeur ; 0 au
// rang maximal, comme `KafraCost`.
[[nodiscard]] std::uint64_t BlessingCost(const SurvivorsMeta& meta, const SurvivorsMetaRanks& ranks,
                                         Blessing blessing) noexcept;
// Le prix du rang suivant d'un service ; 0 au rang maximal.
[[nodiscard]] std::uint64_t KafraCost(const SurvivorsMeta& meta, const SurvivorsMetaRanks& ranks,
                                      KafraService service) noexcept;

// TOUT ACHETER : le total payé pour mener chaque Bénédiction et chaque
// service à son rang maximal, en prenant toujours le rang suivant le moins
// cher (à prix égal, l'Église avant la Kafra, puis l'ordre du catalogue),
// comme un joueur qui dépense dès qu'il peut. C'est la grandeur que les prix
// règlent contre les Zeny médians d'une partie, et l'ordre le plus coûteux :
// chaque rang béni majorant les suivants, acheter d'abord le moins cher fait
// payer la majoration sur ce qui est cher. Le remboursement de l'Église
// rendant tout, un joueur peut racheter dans l'ordre le plus économe, que
// l'ADR 0133 chiffre et que `survivors_meta_test` borne.
[[nodiscard]] std::uint64_t FullMetaCost(const SurvivorsMeta& meta) noexcept;

enum class MetaRefusal : std::uint8_t { kMostRank, kNotEnoughZeny };
// Les Zeny payés, ou le refus dit.
using MetaPurchase = std::expected<std::uint64_t, MetaRefusal>;

// UN ACHAT : le rang monte, le solde baisse, le lieu retient ce qu'il a reçu.
// Refusé sans rien changer au rang maximal ou quand le solde ne suffit pas.
[[nodiscard]] MetaPurchase BuyBlessing(const SurvivorsMeta& meta, SurvivorsMetaRanks& ranks,
                                       std::uint64_t& zeny, Blessing blessing) noexcept;
[[nodiscard]] MetaPurchase BuyKafra(const SurvivorsMeta& meta, SurvivorsMetaRanks& ranks,
                                    std::uint64_t& zeny, KafraService service) noexcept;
// TOUT LE LIEU D'UN COUP : ses rangs à zéro, ce qu'il a reçu rendu au solde.
// Rend les Zeny rendus.
std::uint64_t RefundChurch(SurvivorsMetaRanks& ranks, std::uint64_t& zeny) noexcept;
std::uint64_t RefundKafra(SurvivorsMetaRanks& ranks, std::uint64_t& zeny) noexcept;

// LE DÉPART QUE DONNENT LES RANGS : le loadout (bonus et charges) et les vies.
// Un rang au-delà du maximum compte pour le maximum ; chaque bonus reste dans
// `LoadoutWithinBounds`.
struct SurvivorsMetaStart {
  SurvivorsLoadout loadout;
  std::uint8_t lives = 0;
  [[nodiscard]] bool operator==(const SurvivorsMetaStart&) const = default;
};
[[nodiscard]] SurvivorsMetaStart LoadoutOf(const SurvivorsMeta& meta,
                                           const SurvivorsMetaRanks& ranks,
                                           const ClassRule& class_rule) noexcept;

// LES ZENY D'UNE PARTIE : le Gold ramassé (`hero_zeny`), plus un Zeny par
// dix tués, plus kVictoryZeny pour une victoire.
inline constexpr std::uint32_t kKillsPerZeny = 10;
inline constexpr std::uint64_t kVictoryZeny = 500;
[[nodiscard]] std::uint64_t RunZeny(const SurvivorsTally& tally, std::uint64_t hero_zeny,
                                    SurvivorsOutcome outcome) noexcept;
// LES ZENY QUE LE CARNET VERSE : ceux de la partie × (100 + 10 × rang de
// Cursed Water) / 100, division entière, saturé, comme le score d'une partie
// libre.
inline constexpr std::uint64_t kCursedWaterZenyPctPerRank = 10;
[[nodiscard]] std::uint64_t CursedWaterZeny(std::uint64_t run_zeny,
                                            std::uint8_t cursed_water) noexcept;

}  // namespace ro::sim
