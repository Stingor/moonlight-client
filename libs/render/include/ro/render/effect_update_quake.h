#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace ro::render {

// LES TREMBLEMENTS QU'UN EFFET DEMANDE EN VIVANT, tic par tic.
//
// Le client fait avancer chaque instance d'effet par un répartiteur indexé sur
// son identifiant ; pour une poignée d'identifiants, la case de ce répartiteur
// envoie au mode de jeu la commande qui fait trembler l'écran, à des âges
// précis de l'instance. Meteor Storm tremble quand sa boule touche le sol, au
// quarantième tic ; Lord of Vermilion tous les cinquante tics, cinq fois.
//
// Ce n'est ni le lecteur de `.str` (`str_effect_setup.h`), ni la table des effets
// d'acteur : un effet de cette table tremble quel que soit le chemin qui l'a
// fait naître, fichier ou nœuds.

// La durée courte d'un tremblement, quand la commande la demande.
inline constexpr std::uint32_t kEffectShortQuakeMs = 200;

struct EffectUpdateQuake {
  std::uint32_t effect_id = 0;
  // LES ÂGES DE L'INSTANCE, en tics, où l'écran se met à trembler.
  std::span<const std::uint16_t> ticks;
  // Combien de temps chaque tremblement dure, en millisecondes.
  std::uint32_t quake_ms = 0;
};

// La table entière, triée par identifiant. Engendrée.
[[nodiscard]] std::span<const EffectUpdateQuake> EffectUpdateQuakeTable();

// Les tremblements de cet identifiant, ou nul s'il n'en demande aucun.
[[nodiscard]] inline const EffectUpdateQuake* EffectUpdateQuakeFor(std::uint32_t effect_id) {
  const std::span<const EffectUpdateQuake> table = EffectUpdateQuakeTable();
  const auto found = std::ranges::lower_bound(table, effect_id, {}, &EffectUpdateQuake::effect_id);
  return found != table.end() && found->effect_id == effect_id ? &*found : nullptr;
}

}  // namespace ro::render
