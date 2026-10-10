#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

// LA PORTE DE DESSIN D'UN EFFET PORTÉ PAR UN ACTEUR.
//
// Le client ne dessine pas l'effet d'un personnage joueur que ses options
// cachent — Hiding, Cloaking, invisible —, et cela vaut pour les nœuds, les
// `.str` et les classes d'effet. Deux exceptions :
//   - le joueur voit les effets qu'il porte lui-même, sauf ceux de ses
//     chapeaux ;
//   - sur certains types de carte, un membre du groupe en ligne voit ceux d'un
//     autre — voir `ro::sim::PartySeesConcealedEffects`.
// Un monstre, un PNJ ou un familier n'est jamais caché à cette porte. Le
// jugement d'un porteur vit dans `effect_bearer_gate.h` : il lit le monde, que
// la troupe ne connaît pas.
//
// ⚠ COUPÉ N'EST PAS MORT : la porte saute le DESSIN et laisse l'instance vivre.
// Un effet qui ne se dessine pas ne voit pas non plus son alpha plafonné sur
// `ecl_fild01`, puisque c'est le dessin qui l'écrit.
namespace moonclient {

// L'EFFET D'UN TRAIT DE DEVOTION : le tube tendu du croisé vers un protégé. La
// porte le juge aussi sur l'acteur qu'il vise — voir `EffectDrawGate::DrawsTarget`.
inline constexpr std::uint32_t kDevotionLinkEffect = 232;
// LE SIGNE DU VENT qu'un Windhawk pose sur sa cible. Sa porte à lui ne regarde
// que les boss — voir `EffectDrawGate::DrawsWindSign`.
inline constexpr std::uint32_t kWindSignEffect = 1900;

// CE QUE LA PORTE DIT D'UN PORTEUR.
enum class BearerGate : std::uint8_t {
  // Visible, ou un acteur que la porte ne cache jamais.
  kShown,
  // Caché, mais vu quand même : le joueur lui-même, ou un membre du groupe là où
  // la carte le permet. Ses chapeaux, eux, restent cachés.
  kExcepted,
  kConcealed,
};

// LA PORTE D'UNE IMAGE : les porteurs cachés, les acteurs présents et les boss
// cachés. Les autres porteurs se montrent.
struct EffectDrawGate {
  std::unordered_map<std::uint32_t, BearerGate> concealed;
  std::unordered_set<std::uint32_t> present;
  std::unordered_set<std::uint32_t> concealed_bosses;

  // Vrai quand l'effet porté par `bearer_id` se dessine. `from_hat` dit que son
  // instance est née d'un chapeau : le client la marque à sa naissance, et la
  // marque retire les exceptions.
  [[nodiscard]] bool Draws(std::uint32_t bearer_id, bool from_hat) const;
  // VRAI QUAND L'ACTEUR QU'UN TRAIT DE DEVOTION VISE SE LAISSE VOIR : le client
  // le cherche avant de juger le porteur, refuse le dessin s'il ne le trouve
  // pas, et le passe à la même porte, sans regarder les chapeaux.
  [[nodiscard]] bool DrawsTarget(std::uint32_t target_id) const;
  // LA PORTE DU SIGNE DU VENT, qui remplace l'autre : sans porteur, rien ne se
  // dessine ; un boss que ses options cachent le cache aussi ; tout autre
  // porteur le montre, caché ou non.
  [[nodiscard]] bool DrawsWindSign(std::uint32_t bearer_id) const;
  // LA PORTE QUI VAUT POUR CET EFFET : celle du signe du vent pour lui, la
  // porte commune pour tout autre.
  [[nodiscard]] bool DrawsEffect(std::uint32_t effect_id, std::uint32_t bearer_id,
                                 bool from_hat) const;
};

}  // namespace moonclient
