#pragma once

#include <cstddef>
#include <cstdint>

namespace ro::sim {

// LE CONTENU D'UNE ARÈNE COMPLÈTE : comportements des monstres, scripts des
// boss, événements, élites, évolutions, coffres, plantes et objets au sol.
// Des données, comme le reste des règles, que survivors_rules.h assemble.
//
// Chaque champ a un défaut INERTE : une règle qui ne le pose pas se joue comme
// une règle qui l'ignore. Un ajout se fait en FIN d'énumération ou de
// structure, pour que les tables désignées existantes compilent sans retouche.

// La façon dont un monstre approche le héros.
enum class FoeMove : std::uint8_t {
  kChase,    // poursuite par le champ de marche
  kStill,    // immobile
  kFlyer,    // droit sur le héros, en oscillant ; les murs l'arrêtent sauf kFoePassesWalls
  kThief,    // prend les gemmes libres, et rend leur XP à sa mort
  kRanged,   // se tient à distance et tire, après une annonce
  kCharger,  // fonce en ligne droite, après une annonce
  kBlinker,  // saute vers le héros, après une annonce
};

// Les traits d'un monstre, un masque d'entiers.
inline constexpr std::uint8_t kFoeKnockbackImmune = 1U << 0U;
inline constexpr std::uint8_t kFoeFreezeImmune = 1U << 1U;
inline constexpr std::uint8_t kFoePassesWalls = 1U << 2U;
inline constexpr std::uint8_t kFoeHides = 1U << 3U;
inline constexpr std::uint8_t kFoeOverHorde = 1U << 4U;  // hors de la séparation

// Ce qu'un monstre fait d'autre que marcher. Un comportement à acte (tir,
// charge, bond, cachette) se répète toutes les `period_ms`, annoncé
// `telegraph_ms` avant de jouer, et dure `active_ms`.
struct FoeBehaviour {
  FoeMove move = FoeMove::kChase;
  std::uint8_t traits = 0;
  std::uint8_t pack = 1;  // nés ensemble par un tirage de vague
  std::uint32_t period_ms = 0;
  std::uint32_t telegraph_ms = 0;
  std::uint32_t active_ms = 0;
  std::int32_t reach_units = 0;  // tir, charge, bond, déclenchement
  std::int32_t dash_units_per_s = 0;
  std::int32_t shot_damage = 0;
  std::int32_t shot_units_per_s = 0;
  std::int32_t shot_radius_units = 0;
  std::int32_t puddle_damage = 0;  // flaque laissée à la mort
  std::int32_t puddle_radius_units = 0;
  std::uint32_t puddle_ms = 0;
  std::uint32_t puddle_period_ms = 0;
};

// Un comportement qui joue un acte à intervalle : il lui faut une période.
[[nodiscard]] constexpr bool HasActs(const FoeBehaviour& behaviour) noexcept {
  return behaviour.move == FoeMove::kRanged || behaviour.move == FoeMove::kCharger ||
         behaviour.move == FoeMove::kBlinker || (behaviour.traits & kFoeHides) != 0;
}

enum class BossMoveKind : std::uint8_t { kSummon, kLeap, kCharge, kQuake, kHowl, kFlee };
inline constexpr std::uint8_t kWholeHpPct = 100;
// Les coups qu'un boss suit un à un : CheckSurvivorsRules refuse un script plus
// long.
inline constexpr std::size_t kMostBossMoves = 8;

// Un coup d'un boss scripté. Actif tant que
// to_hp_pct < PV restants (en %) <= from_hp_pct.
struct BossMoveRule {
  BossMoveKind kind = BossMoveKind::kSummon;
  std::uint8_t from_hp_pct = kWholeHpPct;
  std::uint8_t to_hp_pct = 0;
  std::uint32_t first_ms = 0;
  std::uint32_t period_ms = 0;
  std::uint32_t telegraph_ms = 0;
  std::uint32_t effect_ms = 0;
  std::uint16_t foe_id = 0;  // invoqué, ou espèce qu'un hurlement presse
  std::uint8_t count = 0;    // invoqués, cercles de séisme
  std::int32_t reach_units = 0;
  std::int32_t radius_units = 0;
  std::int32_t damage = 0;
  std::int32_t boost_pct = 0;
};

// Un coup qui a besoin d'une espèce : celle qu'il invoque ou qu'il presse.
[[nodiscard]] constexpr bool NamesFoe(BossMoveKind kind) noexcept {
  return kind == BossMoveKind::kSummon || kind == BossMoveKind::kHowl;
}

enum class EventKind : std::uint8_t { kSwarm, kRaid, kRing, kGroup };
// Une naissance groupée, une fois, à at_ms.
struct EventRule {
  std::uint32_t at_ms = 0;
  EventKind kind = EventKind::kGroup;
  std::uint16_t foe_id = 0;
  std::uint8_t count = 0;
  std::int32_t radius_units = 0;  // anneau
};

// Une élite à first_ms, puis toutes les interval_ms jusqu'à last_ms compris ;
// un intervalle nul n'en fait naître aucune. Les majorations sont en %.
struct EliteRule {
  std::uint32_t first_ms = 0;
  std::uint32_t interval_ms = 0;
  std::uint32_t last_ms = 0;
  std::int32_t hp_pct = 0;
  std::int32_t touch_pct = 0;
  std::int32_t exp_pct = 0;
};

// Un skill au plus haut niveau, et sa carte possédée, donnent l'évolué.
struct EvolutionRule {
  std::uint16_t skill_id = 0;
  std::uint16_t card_item_id = 0;
  std::uint16_t evolved_skill_id = 0;
};

enum class PickupKind : std::uint8_t {
  kChest,
  kMeat,
  kGold,
  kManual,
  kHaste,
  kHolyWater,
  kMagnetAll,
  // Une carte tombée d'un monstre : hors du plafond des objets, jamais
  // effacée avant la fin, ignorée par l'aimant et par les voleurs. Elle n'a
  // pas de PickupRule : son objet est la carte elle-même.
  kCard,
};
// Un objet au sol, ramassé au contact.
struct PickupRule {
  PickupKind kind = PickupKind::kGold;
  std::uint16_t item_id = 0;  // identifiant d'objet RO : nom, icône côté app
  std::int32_t amount = 0;    // PV, Zeny ou majoration en %
  std::uint32_t duration_ms = 0;
  std::int32_t reach_units = 0;
};

// Une plante, posée au départ, qui lâche son objet une fois cassée.
struct PlantRule {
  std::uint16_t foe_id = 0;  // classe RO : nom et sprite côté app
  PickupKind pickup = PickupKind::kGold;
  std::uint32_t weight = 0;
};

// Les gains qu'un coffre donne au plus : la nouvelle d'un coffre les porte tous.
inline constexpr std::size_t kMostChestGains = 5;
inline constexpr std::uint32_t kChestDrawBound = 10000;

// Le coffre qu'une élite ou un mini-boss lâche. Un objet nul : aucun coffre.
// Les seuils se lisent sur un tirage dans [0, kChestDrawBound).
struct ChestRule {
  std::uint16_t item_id = 0;
  std::uint32_t evolution_from_ms = 0;
  std::uint32_t triple_per_10000 = 0;
  std::uint32_t quintuple_per_10000 = 0;
  std::uint8_t single = 1;
  std::uint8_t triple = 3;
  std::uint8_t quintuple = static_cast<std::uint8_t>(kMostChestGains);
};

}  // namespace ro::sim
