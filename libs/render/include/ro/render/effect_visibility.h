#pragma once

#include <array>
#include <cstdint>
#include <span>

// CE QUE LES DEUX BASCULES D'EFFET DU JEU COMMANDENT.
//
// Le client a deux commandes qui touchent les effets, et elles ne font pas la
// même chose du tout :
//
//   /effect     — un CRIBLE D'AFFICHAGE. Réglage 8 (`TT_EFFECT_ON_OFF`),
//                 défaut ACTIVÉ. Un seul portier le lit, au moment de peindre,
//                 et il passe outre pour une liste blanche.
//   /mineffect  — un CHOIX DE RESSOURCE. Réglage 229 (`TT_MIN_EFFECT_ON_OFF`),
//                 défaut COUPÉ. C'est le drapeau le plus lu du client :
//                 230 sites dans 43 fonctions, contre 13 pour /effect.
//
// ⚠ AUCUNE DES DEUX N'A DE CODE À ELLE. Les deux tombent dans le `default` du
// répartiteur de chat, qui délègue à la bascule générique des réglages. Ce qui
// les distingue — titre, onglet, défaut, messages — est une ligne de
// `gamesettings.lub`, un fichier texte en clair.
namespace ro::render {

// Les effets que `/effect` NE COUPE PAS, triés.
//
// Le portier du client écarte un effet quand le réglage 8 est coupé ET que
// l'effet n'est pas dans la liste blanche.
//
// ⚠ CE SECOND TERME EST CETTE LISTE, et elle vit dans les DONNÉES du client
// (`forcerendereffect.lub`), pas dans son code. Couper tous les effets sans
// elle éteindrait cent quatorze repères que le jeu garde exprès.
[[nodiscard]] std::span<const std::uint32_t> ForcedVisibleEffects();

// Vrai si cet effet reste visible alors même que `/effect` est coupé.
[[nodiscard]] bool IsForcedVisibleEffect(std::uint32_t effect_id);

// LES EFFETS QUE `/mineffect` FAIT DISPARAÎTRE ENTIERS, triés : leur bâtisseur
// lit le réglage 229 en tête et sort aussitôt — ni nœud, ni bruit. Bash (16)
// et le trait de Frost Diver (27, 801). Les autres bâtisseurs qui le lisent
// choisissent une autre branche, et non le silence.
inline constexpr std::array<std::uint32_t, 3> kEffectsVanishingWithMinimalEffects = {16, 27, 801};

[[nodiscard]] constexpr bool EffectVanishesWithMinimalEffects(std::uint32_t effect_id) noexcept {
  for (const std::uint32_t vanishing : kEffectsVanishingWithMinimalEffects) {
    if (vanishing == effect_id) {
      return true;
    }
  }
  return false;
}

}  // namespace ro::render
