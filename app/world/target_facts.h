#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/world_fwd.h"
#include "ro/vfs/item_words.h"

namespace moonclient {

// ── CE QUE LE HUD DE CIBLE SAIT DE L'ENTITÉ DÉSIGNÉE ────────────────────────
//
// Deux sources. Le CLIENT sait le nom, la carte que le serveur a écrite sous ce
// nom, et la jauge de vie qu'il tient pour un monstre frappé ou un membre du
// groupe. Le SERVEUR, par le couple de la cible, sait le reste — et dès qu'il a
// répondu, c'est lui qui classe et qui chiffre.

// La plaque d'un monstre, telle que le serveur l'écrit dans le champ du groupe :
// « Lv. 42 | HP: 63% » ou « HP: 1234/2000 ». Le format dépend d'un réglage du
// serveur : chaque morceau se cherche seul, et ce qui manque reste absent.
struct MobPlate {
  std::optional<int> level;
  std::optional<std::uint32_t> health;
  std::optional<std::uint32_t> max_health;
  std::optional<int> health_percent;
};
[[nodiscard]] MobPlate ParseMobPlate(std::string_view plate);

struct TargetFacts {
  bool present = false;
  bool is_player = false;
  bool is_monster = false;
  // Confirmé par le serveur, jamais supposé d'après la classe.
  bool is_npc = false;
  // Le serveur a livré le SP : soi, son groupe ou sa guilde.
  bool ally = false;
  std::string name;
  // Sa guilde, pour un joueur seulement.
  std::string guild;
  // « Demi-Human · Neutral 1 · Medium », ou la race et l'élément de la plaque.
  std::string species;
  std::optional<std::int16_t> level;
  std::optional<std::uint32_t> health;
  std::optional<std::uint32_t> max_health;
  std::optional<std::uint32_t> magic;
  std::optional<std::uint32_t> max_magic;
};

// Ce qu'on sait de la désignation en cours. `present` faux sans désignation.
//
// Race, élément et taille se disent avec les mots de la fiche d'un monstre,
// que `words` tient.
[[nodiscard]] TargetFacts FactsOfTarget(const ro::sim::World& world,
                                        const ro::vfs::ItemWords& words);

}  // namespace moonclient
