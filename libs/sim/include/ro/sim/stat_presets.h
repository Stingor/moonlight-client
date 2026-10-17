#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ro::sim {

// LES PRÉRÉGLAGES DE CARACTÉRISTIQUES : six valeurs de BASE nommées, propres au
// compte, et le plan qui dépense les points libres pour s'en approcher.
//
// ⚠ DES VALEURS DE BASE, JAMAIS LE TOTAL : le bonus de l'équipement change avec
// la tenue, et seul ce qu'on achète avec des points se vise.

// Force, agilité, vitalité, intelligence, dextérité, chance : l'ordre de la
// fiche, et celui dans lequel le plan dépense.
inline constexpr std::size_t kBaseStatCount = 6;
using BaseStats = std::array<std::int64_t, kBaseStatCount>;

// Le numéro serveur de la force (`SP_STR`) ; les cinq autres suivent.
inline constexpr std::uint16_t kFirstBaseStatParameter = 13;

// LE PLAFOND D'UNE CARACTÉRISTIQUE DE BASE sur le serveur Moonlight : ses
// `max_*_parameter` valent tous 999, pour toutes les familles de métier.
//
// ⚠ LE SERVEUR A LE DERNIER MOT : un coût annoncé NUL dit « au plafond » pour
// la valeur actuelle, et le plan s'y arrête quoi que dise cette constante.
inline constexpr std::int64_t kBaseStatCeiling = 999;

// CE QUE COÛTE LE POINT QUI FAIT PASSER UNE CARACTÉRISTIQUE DE `value` À
// `value + 1`, selon la règle du serveur Moonlight :
//
//   jusqu'à 200 : 1 + (value + 9) / 10
//   jusqu'à 300 : 1 + (2 × value + 9) / 10
//   au-delà     : 1 + (4 × value + 9) / 10
//
// ⚠ C'EST UNE PRÉVISION. Le coût du point SUIVANT, le serveur l'annonce
// lui-même, et c'est celui-là que le plan prend pour la première marche de
// chaque caractéristique ; la règle ne sert qu'aux marches d'après.
[[nodiscard]] std::int64_t BaseStatPointCost(std::int64_t value);

// ── LE CARNET ───────────────────────────────────────────────────────────────

struct StatPreset {
  std::string name;
  BaseStats stats = {};

  [[nodiscard]] bool operator==(const StatPreset&) const = default;
};

// Le nom saisi, terminateur compris, et le nombre de préréglages d'un compte.
inline constexpr std::size_t kStatPresetNameBytes = 32;
inline constexpr std::size_t kStatPresetLimit = 20;

enum class StatPresetRefusal : std::uint8_t {
  kNone,
  // Un nom vide, ou fait de blancs.
  kEmptyName,
  // Un autre préréglage porte déjà ce nom.
  kNameTaken,
  // Le carnet est plein.
  kFull,
  // Le rang ne désigne aucun préréglage.
  kUnknown,
};

class StatPresetBook {
 public:
  StatPresetBook() = default;
  explicit StatPresetBook(std::vector<StatPreset> presets) : presets_(std::move(presets)) {}

  // ENREGISTRE `stats` SOUS CE NOM. Un nom déjà porté REMPLACE ses valeurs, à
  // sa place dans la liste : c'est ainsi qu'on met un préréglage à jour.
  //
  // Le nom perd ses blancs de tête et de queue, et se coupe à la taille de la
  // saisie sans couper un caractère UTF-8 en deux.
  StatPresetRefusal Save(std::string_view name, const BaseStats& stats);
  StatPresetRefusal Rename(std::size_t index, std::string_view name);
  StatPresetRefusal Remove(std::size_t index);

  [[nodiscard]] std::span<const StatPreset> all() const noexcept { return presets_; }
  [[nodiscard]] const StatPreset* at(std::size_t index) const noexcept {
    return index < presets_.size() ? &presets_[index] : nullptr;
  }

 private:
  std::vector<StatPreset> presets_;
};

// Le nom tel que le carnet le range : sans blancs autour, borné à la saisie.
[[nodiscard]] std::string TidyStatPresetName(std::string_view typed);

// ── LE PLAN ─────────────────────────────────────────────────────────────────

// Ce que le plan sait du personnage.
struct StatPlanInput {
  BaseStats current = {};
  // Le coût du point SUIVANT tel que le serveur l'a annoncé, par
  // caractéristique. Nul : au plafond. Négatif : pas encore dit, et la règle
  // `BaseStatPointCost` le remplace.
  BaseStats next_cost = {};
  std::int64_t status_point = 0;
  std::int64_t ceiling = kBaseStatCeiling;
};

enum class StatPlanVerdict : std::uint8_t {
  // La cible est atteinte, ou l'était déjà.
  kReached,
  // Déjà AU-DESSUS de la cible : le client ne retire jamais de points, la
  // caractéristique reste telle quelle.
  kAboveTarget,
  // Les points manquent : le plan s'est arrêté sur elle, ou avant elle.
  kShort,
  // Le plafond du serveur arrête la caractéristique sous la cible.
  kCeiling,
};

struct StatPlanRow {
  std::int64_t current = 0;
  std::int64_t after = 0;
  std::int64_t target = 0;
  StatPlanVerdict verdict = StatPlanVerdict::kReached;
  // Les points de statut que coûtent les montées prévues.
  std::int64_t spent = 0;
  // Ce qui manquera pour atteindre la cible, en crans, et ce que ces crans
  // coûteraient en points (le plafond n'en vend aucun).
  std::int64_t missing_steps = 0;
  std::int64_t missing_points = 0;
};

struct StatPlan {
  std::array<StatPlanRow, kBaseStatCount> rows = {};
  std::int64_t points_before = 0;
  std::int64_t points_after = 0;

  // Le nombre de montées que l'application enverra.
  [[nodiscard]] std::int64_t raises() const noexcept;
};

// LE PLAN QUI RAPPROCHE LE PERSONNAGE DE `target`.
//
// Les caractéristiques se servent DANS L'ORDRE FIXE de la fiche — STR, AGI,
// VIT, INT, DEX, LUK —, chacune montée jusqu'à sa cible ou jusqu'au plafond
// avant de passer à la suivante. Dès qu'un point coûte plus que ce qui reste,
// le plan S'ARRÊTE : les suivantes ne reçoivent rien, même si un point moins
// cher y serait payable. L'ordre est ainsi celui que le joueur lit dans
// l'aperçu, et rien ne se dépense hors de lui.
[[nodiscard]] StatPlan PlanStatPreset(const BaseStats& target, const StatPlanInput& input);

}  // namespace ro::sim
