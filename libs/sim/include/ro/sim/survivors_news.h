#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

namespace ro::sim {

// LES NOUVELLES D'UNE PARTIE : des FAITS, que app traduit en effets, nombres et
// sons. La simulation ne les relit jamais ; app les prend par TakeNews. Au-delà
// d'un plafond, une nouvelle est écartée et seulement comptée dans `dropped`.
inline constexpr std::size_t kMostNewsHits = 512;
inline constexpr std::size_t kMostNewsFalls = 256;
inline constexpr std::size_t kMostNewsCasts = 256;

struct SurvivorsHit {
  std::uint32_t foe_serial = 0;
  FieldPoint at;
  std::int32_t amount = 0;
  std::int32_t efficiency_pct = 100;
  std::uint16_t skill_id = 0;
  bool froze = false;    // le coup a gelé le monstre
  bool knocked = false;  // le coup l'a repoussé
};
struct SurvivorsFall {
  std::uint32_t foe_serial = 0;
  std::uint16_t foe_id = 0;
  FieldPoint at;
  FoeRank rank = FoeRank::kCommon;
};
// Un ajout se fait en FIN d'énumération.
enum class CastKind : std::uint8_t {
  kStrike,
  kShot,
  kOrbitOn,
  kOrbitOff,
  kBlast,
  kWallOn,
  kRingOn,
  kNova,
  kZoneOff,
};
// Un lancer : UNE nouvelle par frappe (kStrike, rebond compris) et par tir
// (kShot), pas une par salve, avec le monstre visé et, pour une frappe, le pas
// où elle tombe (0 pour un tir). Une orbite publie kOrbitOn à l'entrée de sa
// phase active et kOrbitOff à sa sortie, `from` et `to` au héros. Un éclat
// (kBlast) et une nova (kNova) portent leur centre dans `to` et leur pas de
// chute ; un mur (kWallOn) ses deux bouts, un pilier (kRingOn) son centre dans
// `from` et `to`, et chaque zone publie kZoneOff, aux mêmes points, quand elle
// s'éteint.
struct SurvivorsCast {
  std::uint16_t skill_id = 0;
  CastKind kind = CastKind::kStrike;
  FieldPoint from;
  FieldPoint to;
  std::uint32_t target_serial = 0;
  std::uint32_t land_step = 0;
};
inline constexpr std::size_t kMostNewsWarnings = 128;
// Un ajout se fait en FIN d'énumération.
enum class WarningKind : std::uint8_t { kCharge, kLeap, kShot, kQuake };
// UNE ANNONCE : un coup qui tombera à `strike_step`, que app dessine au sol
// d'ici là. Une charge va de `from` à `to`, un bond s'y pose, un tir y vise
// (avec son rayon), un séisme frappe le cercle de centre `to` et de rayon
// `radius_units`.
struct SurvivorsWarning {
  WarningKind kind = WarningKind::kCharge;
  std::uint32_t source_serial = 0;
  FieldPoint from;
  FieldPoint to;
  std::int32_t radius_units = 0;
  std::uint32_t strike_step = 0;
};
inline constexpr std::size_t kMostNewsChests = 32;
inline constexpr std::size_t kMostNewsEvolutions = 16;
inline constexpr std::size_t kMostNewsPickups = 64;
enum class ChestGainKind : std::uint8_t { kEvolution, kSkillLevel, kCardLevel, kZeny };
// UN GAIN DE COFFRE : une évolution (l'Id de l'évolué, niveau 1), la montée
// d'un skill ou d'une carte possédés (l'Id et le niveau atteint), ou des zenys
// (Id et niveau nuls).
struct SurvivorsChestGain {
  ChestGainKind kind = ChestGainKind::kZeny;
  std::uint16_t id = 0;
  std::uint8_t level = 0;
  [[nodiscard]] bool operator==(const SurvivorsChestGain&) const = default;
};
// Un coffre ouvert en `at`, et ses gains dans l'ordre où ils s'appliquent.
struct SurvivorsChestNews {
  FieldPoint at;
  std::array<SurvivorsChestGain, kMostChestGains> gains = {};
  std::uint8_t gain_count = 0;
};
// L'emplacement `slot` passe du skill de base à son évolué.
struct SurvivorsEvolutionNews {
  std::uint8_t slot = 0;
  std::uint16_t from_skill_id = 0;
  std::uint16_t to_skill_id = 0;
};
// Un objet ramassé en `at`.
struct SurvivorsPickupNews {
  PickupKind kind = PickupKind::kGold;
  std::uint16_t item_id = 0;
  FieldPoint at;
};
inline constexpr std::size_t kMostNewsBossMoves = 64;
// Un coup de boss parti : annoncé (bond, charge, séisme) ou joué tout de suite
// (invocation, hurlement, fuite).
struct SurvivorsBossMoveNews {
  std::uint32_t serial = 0;
  BossMoveKind kind = BossMoveKind::kSummon;
};
inline constexpr std::size_t kMostNewsCards = 16;
// Une carte tombée en `at`, ou ramassée là : ramassée, elle entre dans
// l'Album, sans effet sur la partie.
struct SurvivorsCardNews {
  std::uint16_t card_item_id = 0;
  FieldPoint at;
};
// Le héros relevé en `at` par une vie de réserve ; il lui en reste
// `lives_left`.
struct SurvivorsReviveNews {
  FieldPoint at;
  std::uint8_t lives_left = 0;
};
struct SurvivorsNews {
  std::vector<SurvivorsHit> hits;
  std::vector<SurvivorsFall> falls;
  std::vector<SurvivorsCast> casts;
  std::vector<SurvivorsWarning> warnings;
  std::vector<SurvivorsChestNews> chests;
  std::vector<SurvivorsEvolutionNews> evolutions;
  std::vector<SurvivorsPickupNews> pickups_taken;
  std::vector<std::uint32_t> boss_arrivals;  // serials
  std::vector<SurvivorsBossMoveNews> boss_moves;
  std::uint16_t level_ups = 0;
  std::uint32_t gems_gathered = 0;
  std::int32_t hero_hurt = 0;  // somme des dégâts encaissés
  std::uint32_t dropped = 0;   // nouvelles écartées par les plafonds
  std::vector<SurvivorsCardNews> cards_found;
  std::vector<SurvivorsReviveNews> revives;
  // Les cartes tombées au sol, sous le plafond des cartes ramassées.
  std::vector<SurvivorsCardNews> cards_dropped;
};

}  // namespace ro::sim
