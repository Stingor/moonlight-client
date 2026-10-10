#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/sim/actor.h"

namespace ro::sim {

// ── LES TACTIQUES PAR MONSTRE D'UN COMPAGNON (niveau 2 de l'ADR 0092) ──────
//
// Une table « monstre → conduite ». Une ligne ne donne pas d'ordre elle-même :
// elle dit si ce monstre devient une cible, à quel rang, et comment le traiter
// (skills, poursuite, partage, évitement). Sans ligne pour un monstre, la ligne
// générique qui le couvre s'applique, et à défaut le comportement de base.
//
// ⚠ LES MONSTRES INVOQUÉS N'ONT PAS DE LIGNE GÉNÉRIQUE : rien de ce que le
// serveur envoie ne distingue une invocation d'un monstre ordinaire.

// UN MONSTRE N'EST « IMMOBILE » qu'après avoir été vu ce temps sans bouger,
// attaquer ni incanter : un monstre au repos n'est pas une plante.
inline constexpr std::uint32_t kStillObservationMs = 3000;

// AU PLUS CE NOMBRE DE LIGNES par profil, génériques comprises.
inline constexpr std::size_t kCompanionTacticMost = 64;

// LE RANG, du plus prioritaire au moins prioritaire. « En dernier » n'est pris
// qu'en l'absence de tout autre candidat ; « Tenir », en dernier recours, pour
// une frappe qui touche, puis le monstre est laissé à d'autres.
enum class TacticRank : std::uint8_t { kAbsolute, kHigh, kMedium, kLow, kLast, kHold, kIgnore };
inline constexpr std::size_t kTacticRankCount = 7;

// QUAND LE MONSTRE DEVIENT UNE CIBLE. « S'il attaque » : il frappe ou incante
// contre le maître, le compagnon ou l'autre compagnon du maître.
enum class TacticTrigger : std::uint8_t { kAlways, kIfAttacks, kIfAttacksCompanion };
inline constexpr std::size_t kTacticTriggerCount = 3;

enum class TacticAvoid : std::uint8_t { kNever, kIfAttacks, kAlways };
// L'emploi des skills automatiques sur ce monstre. « N fois » se compte par
// cible ; « une fois au niveau L » lance un skill au niveau L, puis plus rien.
enum class TacticSkillUse : std::uint8_t { kNever, kTimes, kOnceAtLevel, kAlways };
// Prendre un monstre qu'un autre joueur combat : jamais, s'il ne bouge pas, ou
// toujours. Remplace « ne pas prendre le monstre d'un autre joueur ».
enum class TacticShare : std::uint8_t { kNever, kIfStill, kAlways };
// La poursuite : celle du comportement de base, toujours, ou jamais.
enum class TacticChase : std::uint8_t { kAsBase, kAlways, kNever };

// CE QU'UNE LIGNE DÉSIGNE : une classe de monstre, ou une ligne générique.
enum class TacticKeyKind : std::uint8_t {
  kMonster,
  // Tout monstre sans ligne à lui ni ligne générique plus précise.
  kAnyMonster,
  // Les coffres au trésor des sièges, voir `IsTreasureChest`.
  kChest,
  // Les plantes et champignons : un monstre que le compagnon a vu au moins
  // `kStillObservationMs` sur cette carte sans jamais le voir bouger, attaquer ni
  // incanter.
  kStill,
  // Les sbires : un monstre dont le maître, que le serveur annonce, est un
  // autre monstre.
  kMinion,
};

struct TacticKey {
  TacticKeyKind kind = TacticKeyKind::kMonster;
  // La classe du monstre, pour `kMonster` seulement.
  std::uint16_t job = 0;

  [[nodiscard]] friend bool operator==(const TacticKey&, const TacticKey&) = default;
};

// L'évitement d'usine : à trois cases, s'écarter de cinq.
inline constexpr int kTacticAvoidThresholdCells = 3;
inline constexpr int kTacticAvoidStepCells = 5;
// La SP à garder d'une ligne qui n'en fixe pas : celle du niveau 1.
inline constexpr std::int64_t kTacticSpReserveFromBase = -1;
// Le poids d'usine d'un monstre dans les comptages des règles.
inline constexpr float kTacticDefaultWeight = 1.0F;

// LES BORNES DU VOCABULAIRE : ce qu'un fichier peut porter sans sortir du cadre.
inline constexpr int kTacticAvoidMostCells = 20;
inline constexpr int kTacticSkillUseMost = 99;
inline constexpr std::int64_t kTacticSpReserveMost = 99999;
inline constexpr float kTacticWeightMost = 100.0F;

struct CompanionTactic {
  TacticKey key;
  TacticRank rank = TacticRank::kMedium;
  TacticTrigger trigger = TacticTrigger::kAlways;
  TacticAvoid avoid = TacticAvoid::kNever;
  int avoid_threshold_cells = kTacticAvoidThresholdCells;
  int avoid_step_cells = kTacticAvoidStepCells;
  TacticSkillUse skill_use = TacticSkillUse::kAlways;
  // N pour « N fois », L pour « une fois au niveau L ».
  int skill_use_count = 0;
  // Lancé une fois au premier contact de chaque cible de cette classe ; zéro
  // pour aucun.
  std::uint16_t opening_skill_id = 0;
  // Ce monstre peut recevoir un skill pendant un combat contre un autre.
  bool harass = false;
  std::int64_t sp_reserve_points = kTacticSpReserveFromBase;
  // Combien il compte dans les comptages de monstres : les règles s'en servent.
  float weight = kTacticDefaultWeight;
  TacticShare share = TacticShare::kNever;
  TacticChase chase = TacticChase::kAsBase;

  [[nodiscard]] friend bool operator==(const CompanionTactic&, const CompanionTactic&) = default;
};

// UNE LIGNE DANS LE CADRE : énumérations connues, nombres dans leurs bornes,
// classe non nulle pour une ligne de monstre.
[[nodiscard]] bool ValidTactic(const CompanionTactic& tactic) noexcept;

// UN COFFRE AU TRÉSOR des sièges, d'après sa classe.
[[nodiscard]] bool IsTreasureChest(std::uint16_t job) noexcept;

class CompanionTactics {
 public:
  [[nodiscard]] const CompanionTactic* Find(TacticKey key) const noexcept;
  // Pose la ligne, en remplaçant celle de même clé. Rend faux quand la table
  // est pleine ou la ligne hors du cadre.
  bool Set(const CompanionTactic& tactic);
  bool Remove(TacticKey key);
  [[nodiscard]] std::span<const CompanionTactic> lines() const noexcept { return lines_; }

  [[nodiscard]] friend bool operator==(const CompanionTactics&, const CompanionTactics&) = default;

 private:
  std::vector<CompanionTactic> lines_;
};

// LA LIGNE QUI S'APPLIQUE À CE MONSTRE, de la plus précise à la plus large : la
// ligne de sa classe ; sinon « sbires » s'il en est un (`minion`) ; sinon celle
// des coffres ou des immobiles s'il en est ; sinon « tout autre monstre » ;
// sinon aucune. Un sbire de classe coffre suit donc « sbires » avant « coffres ».
[[nodiscard]] const CompanionTactic* TacticFor(const CompanionTactics& tactics,
                                               const Actor& monster, bool still,
                                               bool minion) noexcept;

}  // namespace ro::sim
