#pragma once

#include <optional>
#include <string_view>

namespace moonclient {

// `/eqopen`, ET SES DEUX GRAPHIES CORÉENNES : la commande du jeu qui bascule
// « Montrer mon équipement », le réglage que la case de la fiche bascule aussi
// (ADR 0062). Seul le premier mot compte, sans la casse latine, comme pour
// toute la table des commandes du jeu.
[[nodiscard]] bool IsShowEquipmentCommand(std::string_view line);

// Ce que la commande demande au serveur : l'inverse de l'état qu'il a dit.
// Tant qu'il n'a rien dit, c'est celui d'un personnage neuf — l'équipement
// caché —, et la commande demande donc de le montrer.
//
// ⚠ LA COMMANDE N'ÉCRIT RIEN : la ligne « Equipment information (not) open to
// the public. » est celle de l'écho du serveur, déjà écrite à chaque annonce.
[[nodiscard]] constexpr bool EquipmentShownAsked(const std::optional<bool>& shown) noexcept {
  return !shown.value_or(false);
}

}  // namespace moonclient
